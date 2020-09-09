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

// This is in a namespace so we can cleanly access it from other compilation units with extern
namespace internals {
    uint64_t system_frame_size = 0;
    uint64_t window_size;
    uint64_t new_window_size(1<<12); // initial size
    uint64_t hop_samples(new_window_size/2);
    uint64_t hop_fract = 0;
    uint64_t computed_hop = hop_samples;
    double hop = double(hop_samples);
    deque<float> aqueue, atmp, ainqueue;
    WaveBuf audio_in, audio_out;
    uint64_t time_samples = 0;
    uint64_t time_fract = 0;
    size_t nch_in = 0, nch_out = 0, new_nch_in = 0, new_nch_out = 0;
    mutex frame_notify_mtx, // for waking up processing thread
          exec_mtx, // guards user code exec
          data_mtx; // guards aqueue, ainqueue
    condition_variable frame_notify;
    bool frame_ready = false;
    using func_t = function<void(WaveBuf&, WaveBuf&, double)>;
    func_t fptr(nullptr);
    string command_file;
    string client_name;
    double rate = 44100; // reset by jack
    jack_client_t *client;
    vector<jack_port_t *> in_ports;
    vector<jack_port_t *> out_ports;
}
using namespace internals;

#ifdef USE_CLING
struct Callbacks : public cling::InterpreterCallbacks {
    Callbacks(cling::Interpreter *interp) : cling::InterpreterCallbacks(interp) { }
    void *EnteringUserCode() { exec_mtx.lock(); return nullptr; }
    void ReturnedFromUserCode(void *) { exec_mtx.unlock(); }
};
void init_cling(int argc, char **argv) { cling::Interpreter interp(argc, argv, LLVMRESDIR, {},
            true); // disable runtime for simplicity
    interp.setCallbacks(make_unique<Callbacks>(&interp));
    interp.setDefaultOptLevel(2);
    interp.allowRedefinition();
    interp.process("#include \"rtinc/root.hpp\"\n");

    // make a fifo called "cmd", which commands are read from
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

            interp.process(to_exec);

            // move to next block
            begin = code.find(CODE_BEGIN);
            end = code.find(CODE_END);
        }
    }
}
#endif

double current_time_secs() {
    return (double(time_samples) + time_fract/double(uint64_t(-1)))/rate;
}
void generate_frames() {
    struct timespec yield_time;
    clock_gettime(CLOCK_MONOTONIC, &yield_time);
    while (true) {
        { // wait until there's more to do
            unique_lock<mutex> lk(frame_notify_mtx);
            frame_notify.wait(lk, []{ return frame_ready; });
            frame_ready = false;
        }
        while (true) {
            struct timespec current_time;
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            if (current_time.tv_sec > yield_time.tv_sec ||
                    (current_time.tv_sec >= yield_time.tv_sec && current_time.tv_nsec >= yield_time.tv_nsec)) {
                usleep(0);
                yield_time.tv_nsec = current_time.tv_nsec + 100000000;
                yield_time.tv_sec = current_time.tv_sec + (yield_time.tv_nsec > 999999999);
                yield_time.tv_nsec %= 1000000000;
            }
            lock_guard<mutex> exec_lk(exec_mtx);
            {
                lock_guard<mutex> data_lk(data_mtx);
                if (new_nch_in != nch_in || new_nch_out != nch_out || new_window_size != window_size) {
                    window_size = new_window_size;
                    nch_in = new_nch_in;
                    nch_out = new_nch_out;
                }
                audio_in.resize(nch_in, window_size);
                audio_out.resize(nch_out, window_size);
                if (ainqueue.size() < nch_in*(computed_hop+window_size))
                    break;
                if (aqueue.size() >= max(system_frame_size, window_size) * nch_out)
                    break;
                // advance input by hop
                size_t sz_tmp = ainqueue.size();
                for (size_t i = 0; i < min(nch_in*computed_hop, sz_tmp); i++)
                    ainqueue.pop_front();
                // get input
                for (uint64_t i = 0; i < window_size; i++) {
                    for (size_t c = 0; c < nch_in; c++) {
                        if (i*nch_in+c < ainqueue.size())
                            audio_in[c][i] = ainqueue[i*nch_in+c];
                        else audio_in[c][i] = 0;
                    }
                }
            }
            if (time_samples == 0 && time_fract == 0 && base_time == 0)
                gettimeofday(&init_time, NULL); // TODO use clock_gettime
            if (fptr) { // call synthesis function
                fptr(audio_in, audio_out, current_time_secs());
                if (time_fract + hop_fract < time_fract)
                    computed_hop = hop_samples + 1;
                else
                    computed_hop = hop_samples;
                time_samples += computed_hop;
                time_fract += hop_fract;
            }
            {
                lock_guard<mutex> data_lk(data_mtx);
                // output region that overlaps with previous frame(s)
                for (uint64_t i = 0; i < min(computed_hop, audio_out.size); i++) {
                    for (size_t c = 0; c < nch_out; c++) {
                        float overlap = 0;
                        if (!atmp.empty()) {
                            overlap = atmp.front();
                            atmp.pop_front();
                        }
                        aqueue.push_back(overlap + audio_out[c][i]);
                    }
                }
                // if the hop is larger than the frame size
                for (int i = 0; i < int(nch_out)*(int(computed_hop)-int(audio_out.size)); i++) {
                    if (!atmp.empty()) {
                        aqueue.push_back(atmp.front()); // output overlap if any
                        atmp.pop_front();
                    } else {
                        aqueue.push_back(0); // otherwise output silence
                    }
                }
            }
            // save region that overlaps with next frame
            for (int i = 0; i < int(audio_out.size)-int(computed_hop); i++) {
                for (size_t c = 0; c < nch_out; c++) {
                    float out_val = audio_out[c][computed_hop+i];
                    if (i*nch_out+c < atmp.size()) atmp[i*nch_out+c] += out_val;
                    else atmp.push_back(out_val);
                }
            }
        }
    }
}

int audio_cb(jack_nframes_t len, void *) {
    lock_guard<mutex> data_lk(data_mtx);
    system_frame_size = len;
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
    size_t underrun = 0;
    for (size_t i = 0; i < len; i++)
        for (size_t c = 0; c < out_ports.size(); c++) {
            if (!aqueue.empty()) {
                out_bufs[c][i] = aqueue.front();
                aqueue.pop_front();
            } else {
                out_bufs[c][i] = 0;
                underrun++;
            }
        }
    static bool warmed_up = false;
    if (underrun && warmed_up)
        cerr << "WARNING: audio output buffer underrun (" << underrun << " samples)" << endl;
    else if (!underrun)
        warmed_up = true;
    return 0;
}
int sample_rate_changed(jack_nframes_t sr, void*) {
    cerr << "INFO: set sample rate to " << sr << endl;
    rate = sr;
    return 0;
}
void init_audio() {
    jack_status_t status;
    client = jack_client_open(client_name.c_str(), JackNullOption, &status);
    if (!client) {
        cerr << "FATAL: Failed to open jack client: " << status << endl;
        exit(1);
    }
    client_name = string(jack_get_client_name(client));
    set_num_channels(2,2);
    jack_set_process_callback(client, audio_cb, nullptr);
    jack_set_sample_rate_callback(client, sample_rate_changed, nullptr);
    jack_activate(client);
    cout << "Playing..." << endl;
}

sighandler_t prev_handlers[32];
void at_exit(int i) {
    cout << "Cleaning up... " << endl;
    jack_deactivate(client);
    unlink(command_file.c_str());
    signal(i, prev_handlers[i]);
    raise(i);
}

int main(int argc, char **argv) {
    command_file = argc > 1 ? argv[1] : "cmd";
    if (access(command_file.c_str(), F_OK) != -1) {
        cerr << "FATAL: command pipe \"" << command_file << "\" already exists. Remove it before continuing." << endl;
        return 1;
    }
#ifdef HACK
    client_name = string("tinyspec_") + STR(HACK);
#else
    client_name = string("tinyspec_") + command_file;
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

