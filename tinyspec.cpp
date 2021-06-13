#include<cmath>
#include<unistd.h>
#include<cstring>
#include<complex>
#include<cassert>
#include<vector>
#include<string>
#include<iostream>
#include<queue>
#include<mutex>
#include<condition_variable>
#include<thread>
#include<optional>
#include<sys/stat.h>
#include<sys/time.h>
#include<csignal>
#include<fcntl.h>
#include<jack/jack.h>
#ifdef USE_CLING
#include"cling/Interpreter/Interpreter.h"
#include"cling/Interpreter/InterpreterCallbacks.h"
#include"cling/Utils/Casting.h"
#endif
#include "rtinc/root.hpp"
#include "rtlib/root.hpp"

using namespace std;

#define STR_(x) #x
#define STR(x) STR_(x)

const char *LLVMRESDIR = "cling_bin"; // path to cling resource directory
const char *CODE_BEGIN = "<<<";
const char *CODE_END = ">>>";

const uint64_t INITIAL_LATENCY = 1<<13;
const uint64_t INITIAL_MAX_BACKBUFFER_SIZE = 1<<25; // about 6 minutes / 32 MB
const uint64_t INITIAL_NCH_IN = 2;
const uint64_t INITIAL_NCH_OUT = 2;

// This is in a namespace so we can cleanly access it from other compilation units with extern
namespace internals {
    uint64_t block_size = INITIAL_LATENCY;
    uint64_t max_backbuffer_size = INITIAL_MAX_BACKBUFFER_SIZE;
    Time time_now = 0;
    uint64_t block_time = 0;
    uint64_t latency = 0;
    uint64_t underrun = 0;
    size_t nch_in = 0, nch_out = 0;
    double rate = 44100; // reset by jack
    deque<float> aoutqueue, ainqueue, backbuffer;
    mutex frame_notify_mtx, // for waking up processing thread
          exec_mtx, // guards user code exec
          data_mtx; // guards aoutqueue, ainqueue
    condition_variable frame_notify;
    bool frame_ready = false;
    string command_file;
    string client_name;
    jack_client_t *client;
    vector<jack_port_t *> in_ports;
    vector<jack_port_t *> out_ports;
    EventTypeId next_type_id;
    EventId next_event_id;
    unordered_map<EventId, Event> event_map;
    optional<EventTypeId> current_event_type;
    optional<EventId> current_event;
    multimap<Time, EventId> event_queue; // map from start_time to event
    struct OutFrame {
        EventId ev;
        Time start_time;
        WaveBuf buf;
    };
    multimap<Time, OutFrame> out_frames;
}

#ifdef USE_CLING
struct Callbacks : public cling::InterpreterCallbacks {
    Callbacks(cling::Interpreter *interp) : cling::InterpreterCallbacks(interp) { }
    void *EnteringUserCode() { internals::exec_mtx.lock(); return nullptr; }
    void ReturnedFromUserCode(void *) { internals::exec_mtx.unlock(); }
};
void init_cling(int argc, char **argv) {
    using namespace internals;
    cling::Interpreter interp(argc, argv, LLVMRESDIR, {},
            true); // disable runtime for simplicity
    interp.setCallbacks(make_unique<Callbacks>(&interp));
    interp.setDefaultOptLevel(2);
    interp.getRuntimeOptions().AllowRedefinition = 1;
    interp.AddIncludePath("rtinc");
    interp.process("#include \"rtinc/root.hpp\"\n");

    cout << "Initialized cling" << endl;

    // make a fifo which commands are read from
    mkfifo(command_file.c_str(), 0700);
    int fd = open(command_file.c_str(), O_RDONLY);

    const size_t CODE_BUF_SIZE = 1<<12;
    char codebuf[CODE_BUF_SIZE];
    string code;
    while (true) { // read code from fifo and execute it
        int n = read(fd, codebuf, CODE_BUF_SIZE);
        if (n == 0) usleep(100000); // hack to avoid spinning too hard
        code += string(codebuf, n);
        int begin = code.find(CODE_BEGIN);
        int end = code.find(CODE_END);

        while (end != -1 && begin != -1) { // while there are blocks to execute
            string to_exec = code.substr(begin+strlen(CODE_BEGIN), end-begin-strlen(CODE_BEGIN));
            code = code.substr(end+strlen(CODE_END)-1);

            size_t nl_pos = to_exec.find("\n");
            int preview_len = nl_pos == string::npos ? to_exec.size() : nl_pos;
            string preview = to_exec.substr(0, preview_len);
            cerr << "execute: " << preview << "..." << endl;

            interp.process(to_exec, nullptr, nullptr, true);

            // move to next block
            begin = code.find(CODE_BEGIN);
            end = code.find(CODE_END);
        }
    }
}
#endif

void generate_frames() {
    using namespace internals;
    struct timespec yield_time;
    clock_gettime(CLOCK_MONOTONIC, &yield_time);
    while (true) {
        { // wait until there's more to do
            unique_lock<mutex> lk(frame_notify_mtx);
            frame_notify.wait(lk, []{ return frame_ready; });
            frame_ready = false;
        }
        while (true) {
            // yield every 100ms to ensure that the code execution thread has a chance to grab the locks
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            if (ts.tv_sec > yield_time.tv_sec ||
                    (ts.tv_sec >= yield_time.tv_sec && ts.tv_nsec >= yield_time.tv_nsec)) {
                usleep(0);
                const long ns_in_ms = 1000000;
                yield_time.tv_nsec = ts.tv_nsec + 100*ns_in_ms;
                yield_time.tv_sec = ts.tv_sec + (yield_time.tv_nsec >= 1000*ns_in_ms);
                yield_time.tv_nsec %= 1000*ns_in_ms;
            }

            lock_guard<mutex> exec_lk(exec_mtx);
            {
                lock_guard<mutex> data_lk(data_mtx);

                // if output queue is full, wait
                if (aoutqueue.size() >= internals::latency * nch_out)
                    break;

                // if not enough input data to process a whole block, wait
                if (ainqueue.size() < block_size * nch_in) {
                    assert(ainqueue.size() == 0);
                    break;
                }

                // copy input data into backbuffer
                for (uint64_t i = 0; i < block_size; i++)
                    for (size_t c = 0; c < nch_out; c++) {
                        backbuffer.push_back(ainqueue.front());
                        ainqueue.pop_front();
                    }
            }

            // drop old data from backbuffer
            while (backbuffer.size() > nch_in*internals::max_backbuffer_size)
                backbuffer.pop_front();

            // TODO
            //if (internals::time_now == 0 && base_time == 0)
                //gettimeofday(&init_time, NULL); // TODO use clock_gettime

            // process all events starting in this block
            while (true) {
                auto lower = event_queue.begin();
                if (lower == event_queue.end())
                    break;
                if (lower->first < block_time) {
                    event_queue.erase(lower);
                    cerr << "Discarded an event that was scheduled for the past" << endl;
                    // TODO give the name and time of the event
                    continue;
                }
                if (lower->first.integral >= block_time + block_size)
                    break; // no more events start in this block
                EventId id = lower->second;
                event_queue.erase(lower);
                Event *ev = &event_map[id];

                // execute!
                internals::time_now = ev->start_time.samples();
                current_event_type = ev->type_id;
                current_event = id;
                if (ev->run_fn)
                    (ev->run_fn)();
                if (ev->snd_fn)
                    out_frames.emplace(ev->start_time,
                                       OutFrame{id, ev->start_time, (ev->snd_fn)()});
                current_event_type = {};
                current_event = {};
                event_map.erase(id);
            }

            // TODO add support for custom mixers
            double buffer[block_size*nch_out];
            memset(buffer, 0, sizeof buffer);

            //fill buffer from events
            for (auto it = out_frames.begin(); it != out_frames.end();) {
                Time start_time = it->first;
                OutFrame &frame = it->second;

                if (frame.buf.num_channels != nch_out) {
                    cerr << "Mixer: discarding frame with incorrect channel count" << endl;
                    it = out_frames.erase(it);
                    continue;
                }

                if (start_time.integral+frame.buf.size < block_time) {
                    // already ended
                    it = out_frames.erase(it);
                    continue;
                } else ++it;

                // not started yet
                if (start_time.integral >= block_time+block_size)
                    break;

                // mix
                int64_t interval_start = max(int64_t(block_time), int64_t(start_time.integral));
                int64_t interval_end = min(int64_t(block_time+block_size), int64_t(start_time.integral+frame.buf.size));
                int64_t block_start = interval_start - block_time;
                int64_t frame_start = interval_start - start_time.integral;
                for (int64_t i = 0; i < interval_end - interval_start; i++)
                    for (size_t c = 0; c < nch_out; c++)
                        buffer[(i+block_start)*nch_out + c] += frame.buf[c][i+frame_start];
            }

            {
                // write out the buffer
                lock_guard<mutex> data_lk(data_mtx);
                for (size_t i = 0; i < block_size*nch_out; i++)
                    aoutqueue.push_back(buffer[i]);

                // if we are running behind, just discard some samples to catch up.
                while (aoutqueue.size() && underrun) {
                    aoutqueue.pop_front();
                    underrun--;
                }
            }

            // advance time
            block_time += block_size;
            internals::time_now = block_time;
        }
    }
}

int audio_cb(jack_nframes_t len, void *) {
    using namespace internals;
    lock_guard<mutex> data_lk(data_mtx);
    block_size = len;
    float *in_bufs[in_ports.size()];
    float *out_bufs[out_ports.size()];
    for (size_t i = 0; i < in_ports.size(); i++)
        in_bufs[i] = (float*) jack_port_get_buffer(in_ports[i], len);
    for (size_t i = 0; i < out_ports.size(); i++)
        out_bufs[i] = (float*) jack_port_get_buffer(out_ports[i], len);
    for (size_t i = 0; i < len; i++)
        for (size_t c = 0; c < in_ports.size(); c++)
            ainqueue.push_back(in_bufs[c][i]);
    {
        unique_lock<mutex> lk(frame_notify_mtx);
        frame_ready = true;
    }
    frame_notify.notify_one();
    for (size_t i = 0; i < len; i++)
        for (size_t c = 0; c < out_ports.size(); c++) {
            if (!aoutqueue.empty()) {
                out_bufs[c][i] = aoutqueue.front();
                aoutqueue.pop_front();
            } else {
                out_bufs[c][i] = 0;
                underrun++;
            }
        }
    if (underrun) {
        cerr << "WARNING: audio output buffer underrun (" << underrun/nch_out << " samples behind)" << endl;
        cerr << "         try increasing the latency by calling set_latency(uint64_t); current latency: " << internals::latency << " samples." << endl;
    }
    return 0;
}
int sample_rate_changed(jack_nframes_t sr, void*) {
    cerr << "INFO: set sample rate to " << sr << endl;
    internals::rate = sr;
    return 0;
}
void init_audio() {
    using namespace internals;
    jack_status_t status;
    client = jack_client_open(internals::client_name.c_str(), JackNullOption, &status);
    if (!client) {
        cerr << "FATAL: Failed to open jack client: " << status << endl;
        exit(1);
    }
    internals::client_name = string(jack_get_client_name(client));
    set_num_channels(INITIAL_NCH_IN, INITIAL_NCH_OUT);
    set_latency(INITIAL_LATENCY);
    jack_set_process_callback(client, audio_cb, nullptr);
    jack_set_sample_rate_callback(client, sample_rate_changed, nullptr);
    jack_activate(client);
    cout << "Initialized JACK" << endl;
}

sighandler_t prev_handlers[32];
void at_exit(int i) {
    cout << "Cleaning up... " << endl;
    jack_deactivate(internals::client);
    unlink(internals::command_file.c_str());
    signal(i, prev_handlers[i]);
    raise(i);
}

int main(int argc, char **argv) {
    internals::command_file = argc > 1 ? argv[1] : "cmd";
    if (access(internals::command_file.c_str(), F_OK) != -1) {
        cerr << "FATAL: command pipe \"" << internals::command_file << "\" already exists. Remove it before continuing." << endl;
        return 1;
    }
#ifdef HACK
    internals::client_name = string("tinyspec_") + STR(HACK);
#else
    internals::client_name = string("tinyspec_") + internals::command_file;
#endif
    prev_handlers[SIGINT] = signal(SIGINT, at_exit);
    prev_handlers[SIGABRT] = signal(SIGABRT, at_exit);
    prev_handlers[SIGTERM] = signal(SIGTERM, at_exit);
    init_audio();
    thread worker(generate_frames);
#ifdef USE_CLING
    init_cling(argc, argv);
#else
#ifdef HACK
#include STR(HACK)
    this_thread::sleep_until(chrono::time_point<chrono::system_clock>::max());
#else
    #error Not using cling but no hack specified. Compile with `./compile --no-cling hack.cpp`
#endif
#endif
}

