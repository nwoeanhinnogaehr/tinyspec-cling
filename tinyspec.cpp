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
#include"cling/Utils/Casting.h"
#endif
#include"osc.h"
#include"synth.h"
#ifdef HACK
#include"osc_rt.h"
#endif
using namespace std;


#define STR_(x) #x
#define STR(x) STR_(x)

#define CONNECT_MIN 0
#define CONNECT_MAX 1
#define CONNECT_ALL 2

const char *LLVMRESDIR = "cling_bin"; // path to cling resource directory
const char *CODE_BEGIN = "<<<";
const char *CODE_END = ">>>";

// This is in a namespace so we can cleanly access it from other compilation units with extern
namespace internals {
    size_t system_frame_size = 0;
    size_t window_size;
    size_t new_window_size(1<<12); // initial size
    size_t hop(new_window_size/2);
    deque<float> aqueue, atmp, ainqueue;
    WaveBuf audio_in, audio_out;
    uint64_t time_samples = 0;
    size_t nch_in = 0, nch_out = 0, new_nch_in = 0, new_nch_out = 0;
    mutex frame_notify_mtx, // for waking up processing thread
          exec_mtx, // guards user code exec
          data_mtx; // guards aqueue, ainqueue
    condition_variable frame_notify;
    bool frame_ready = false;
    using func_t = function<void(WaveBuf&, WaveBuf&, int, double)>;
    func_t fptr(nullptr);
    const char *command_file;
    string client_name;
}
using namespace internals;

// builtins
void set_hop(int new_hop) {
    if (new_hop <= 0) cerr << "ignoring invalid hop: " << new_hop << endl;
    else hop = new_hop;
}
void next_hop_samples(uint32_t n, uint32_t h) {
    new_window_size = n;
    set_hop(h);
}
void next_hop_ratio(uint32_t n, double ratio=0.25) {
    new_window_size = n;
    set_hop(n*ratio);
}
void next_hop_hz(uint32_t n, double hz) {
    new_window_size = n;
    set_hop(double(RATE)/hz);
}
void set_process_fn(func_t fn) {
    fptr = fn;
}

#ifdef USE_CLING
void init_cling(int argc, char **argv) { cling::Interpreter interp(argc, argv, LLVMRESDIR, {},
            true); // disable runtime for simplicity
    interp.setDefaultOptLevel(2);
    interp.process( // preamble
            "#define RATE " STR(RATE) "\n"
            "#define CONNECT_MIN 0\n"
            "#define CONNECT_MAX 1\n"
            "#define CONNECT_ALL 2\n"
            "#include<complex>\n"
            "#include<iostream>\n"
            "#include<vector>\n"
            "#include\"osc_rt.h\"\n"
            "#include\"synth.h\"\n"
            "using namespace std;\n");
    interp.process("#define CLIENT_NAME string(\"" + client_name + "\")");
    interp.declare("void connect(string a, string b, string filter_a=\"\", string filter_b=\"\", int mode=CONNECT_MAX);");
    interp.declare("void disconnect(string a, string b, string filter_a=\"\", string filter_b=\"\", int mode=CONNECT_MAX);");
    interp.declare("void next_hop_ratio(uint32_t n, double hop_ratio=0.25);");
    interp.declare("void next_hop_samples(uint32_t n, uint32_t h);");
    interp.declare("void next_hop_hz(uint32_t n, double hz);");
    interp.declare("void set_num_channels(size_t in, size_t out);");
    interp.declare("void skip_to_now();");
    interp.declare("void frft(FFTBuf &in, FFTBuf &out, double exponent);");
    interp.declare("void set_process_fn(function<void(WaveBuf&, WaveBuf&, int, double)> fn);");

    // make a fifo called "cmd", which commands are read from
    mkfifo(command_file, 0700);
    int fd = open(command_file, O_RDONLY);

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

            exec_mtx.lock();
            interp.process(to_exec);
            exec_mtx.unlock();

            // move to next block
            begin = code.find(CODE_BEGIN);
            end = code.find(CODE_END);
        }
    }
}
#endif

// fast fractional fourier transform
// https://algassert.com/post/1710
void frft(FFTBuf &in, FFTBuf &out, double exponent) {
    assert(in.size == out.size);
    assert(in.num_channels == in.num_channels);
    size_t num_channels = in.num_channels;
    size_t fft_size = in.size;
    if (num_channels*fft_size == 0)
        return;
    size_t data_size = sizeof(cplx)*num_channels*fft_size;
    FFTBuf fft_tmp(num_channels, fft_size);
    int size_arr[] = {(int) fft_size};
    fftw_plan plan = fftw_plan_many_dft(1, size_arr, num_channels,
            (fftw_complex*) in.data, nullptr, 1, fft_size,
            (fftw_complex*) out.data, nullptr, 1, fft_size,
            FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(plan);
    fftw_destroy_plan(plan);
    memcpy(fft_tmp.data, out.data, data_size);
    cplx im(0, 1);
    cplx im1 = pow(im, exponent);
    cplx im2 = pow(im, exponent*2);
    cplx im3 = pow(im, exponent*3);
    double sqrtn = sqrt(fft_size);
    for (size_t c = 0; c < num_channels; c++) {
        for (size_t i = 0; i < fft_size; i++) {
            int j = i ? fft_size-i : 0;
            cplx f0 = in[c][i];
            cplx f1 = fft_tmp[c][i]/sqrtn;
            cplx f2 = in[c][j];
            cplx f3 = fft_tmp[c][j]/sqrtn;
            cplx b0 = f0 + f1 + f2 + f3;
            cplx b1 = f0 + im*f1 - f2 - im*f3;
            cplx b2 = f0 - f1 + f2 - f3;
            cplx b3 = f0 - im*f1 - f2 + im*f3;
            b1 *= im1;
            b2 *= im2;
            b3 *= im3;
            out[c][i] = (b0 + b1 + b2 + b3) / 4.0;
        }
    }
}

void generate_frames() {
    while (true) {
        { // wait until there's more to do
            unique_lock<mutex> lk(frame_notify_mtx);
            frame_notify.wait(lk, []{ return frame_ready; });
            frame_ready = false;
        }
        lock_guard<mutex> exec_lk(exec_mtx);
        while (true) {
            {
                lock_guard<mutex> data_lk(data_mtx);
                if (new_nch_in != nch_in || new_nch_out != nch_out || new_window_size != window_size) {
                    window_size = new_window_size;
                    nch_in = new_nch_in;
                    nch_out = new_nch_out;
                    audio_in.resize(nch_in, window_size);
                    audio_out.resize(nch_out, window_size);
                }
                if (ainqueue.size() < nch_in*(hop+window_size))
                    break;
                if (aqueue.size() >= system_frame_size * nch_out)
                    break;
                size_t sz_tmp = ainqueue.size();
                for (size_t i = 0; i < min(nch_in*hop, sz_tmp); i++)
                    ainqueue.pop_front();
                for (size_t i = 0; i < window_size; i++) {
                    for (size_t c = 0; c < nch_in; c++) {
                        if (i*nch_in+c < ainqueue.size())
                            audio_in[c][i] = ainqueue[i*nch_in+c];
                        else audio_in[c][i] = 0;
                    }
                }
            }
            if (time_samples == 0)
                gettimeofday(&init_time, NULL);
            if (fptr) { // call synthesis function
                fptr(audio_in, audio_out, window_size, time_samples/(double)RATE);
                time_samples += hop;
            }
            {
                lock_guard<mutex> data_lk(data_mtx);
                // output region that overlaps with previous frame(s)
                for (size_t i = 0; i < min(hop, audio_out.size); i++) {
                    for (size_t c = 0; c < nch_out; c++) {
                        float overlap = 0;
                        if (!atmp.empty()) {
                            overlap = atmp.front();
                            atmp.pop_front();
                        }
                        aqueue.push_back(overlap + audio_out[c][i]);
                    }
                }
                // if the hop is larger than the frame size, insert silence
                for (int i = 0; i < int(nch_out)*(int(hop)-int(audio_out.size)); i++)
                    aqueue.push_back(0);
            }
            // save region that overlaps with next frame
            for (int i = 0; i < int(audio_out.size)-int(hop); i++) {
                for (size_t c = 0; c < nch_out; c++) {
                    float out_val = audio_out[c][hop+i];
                    if (i*nch_out+c < atmp.size()) atmp[i*nch_out+c] += out_val;
                    else atmp.push_back(out_val);
                }
            }

        }
    }
}

jack_client_t *client;
vector<jack_port_t *> in_ports;
vector<jack_port_t *> out_ports;

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
    for (size_t i = 0; i < len; i++)
        for (size_t c = 0; c < out_ports.size(); c++) {
            if (!aqueue.empty()) {
                out_bufs[c][i] = aqueue.front();
                aqueue.pop_front();
            } else {
                out_bufs[c][i] = 0;
            }
        }
    return 0;
}

void set_num_channels(size_t in, size_t out) {
    lock_guard<mutex> data_lk(data_mtx);
    ainqueue.clear();
    aqueue.clear();
    //TODO should probably clear atmp too
    for (size_t i = in; i < new_nch_in; i++) {
        jack_port_unregister(client, in_ports.back());
        in_ports.pop_back();
    }
    for (size_t i = new_nch_in; i < in; i++) {
        string name = "in" + to_string(i);
        in_ports.push_back(jack_port_register(client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0));
    }
    for (size_t i = out; i < new_nch_out; i++) {
        jack_port_unregister(client, out_ports.back());
        out_ports.pop_back();
    }
    for (size_t i = new_nch_out; i < out; i++) {
        string name = "out" + to_string(i);
        out_ports.push_back(jack_port_register(client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0));
    }
    new_nch_in = in;
    new_nch_out = out;
}

struct PortPattern {
    string regex;
    size_t idx_s, idx_t;
    unsigned long flags;
};
size_t parse_int_or(string s, size_t otherwise) {
    if (s.size() == 0)
        return otherwise;
    try { return stoi(s); }
    catch (...) { cerr << "failed to parse \"" << s << "\" as integer" << endl; }
    return otherwise;
}
PortPattern parse_port_pattern(string filter) {
    PortPattern p;
    p.regex = ":.*";
    p.idx_s = 0;
    p.idx_t = -1;
    p.flags = JackPortIsOutput | JackPortIsInput; 
    if (filter.size() == 0);
    else if (filter[0] == ':') {
        p.regex = filter;
    } else {
        size_t range_idx = 1;
        if (filter[0] == 'i')
            p.flags = JackPortIsInput;
        else if (filter[0] == 'o')
            p.flags = JackPortIsOutput;
        else
            range_idx = 0;
        p.regex = ":.*";
        int idx = filter.find(',');
        if (idx == -1) {
            p.idx_s = parse_int_or(filter.substr(range_idx), 0);
            p.idx_t = parse_int_or(filter.substr(range_idx), -2) + 1;
        } else {
            p.idx_s = parse_int_or(filter.substr(range_idx, idx), 0);
            p.idx_t = parse_int_or(filter.substr(idx+1), -1);
        }
    }
    return p;
}

void dis_connect_impl(bool con, string client_a, string client_b, string filter_a, string filter_b, int mode) {
    const char* what = con ? "connect" : "disconnect";
    auto fn = con ? jack_connect : jack_disconnect;
    PortPattern a_ptn = parse_port_pattern(filter_a);
    PortPattern b_ptn = parse_port_pattern(filter_b);
    const char** ports_a_in = jack_get_ports(client, (client_a + a_ptn.regex).c_str(),
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput);
    const char** ports_a_out = jack_get_ports(client, (client_a + a_ptn.regex).c_str(),
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
    const char** ports_b_in = jack_get_ports(client, (client_b + b_ptn.regex).c_str(),
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput);
    const char** ports_b_out = jack_get_ports(client, (client_b + b_ptn.regex).c_str(),
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
    size_t ports_a_in_len = 0; for (int i = 0; ports_a_in && ports_a_in[i]; i++) ports_a_in_len++;
    size_t ports_a_in_len_x = min(ports_a_in_len, a_ptn.idx_t-a_ptn.idx_s);
    size_t ports_a_out_len = 0; for (int i = 0; ports_a_out && ports_a_out[i]; i++) ports_a_out_len++;
    size_t ports_a_out_len_x = min(ports_a_out_len, a_ptn.idx_t-a_ptn.idx_s);
    size_t ports_b_in_len = 0; for (int i = 0; ports_b_in && ports_b_in[i]; i++) ports_b_in_len++;
    size_t ports_b_in_len_x = min(ports_b_in_len, b_ptn.idx_t-b_ptn.idx_s);
    size_t ports_b_out_len = 0; for (int i = 0; ports_b_out && ports_b_out[i]; i++) ports_b_out_len++;
    size_t ports_b_out_len_x = min(ports_b_out_len, b_ptn.idx_t-b_ptn.idx_s);
    auto lenfn = [=](size_t a, size_t b) { return mode ? max(a, b) : min(a, b); };
    int count = 0;
    if ((a_ptn.flags & JackPortIsInput) && (b_ptn.flags & JackPortIsOutput)
            && ports_b_out_len_x && ports_a_in_len_x)
        for (size_t i = 0; i < lenfn(ports_a_in_len_x, ports_b_out_len_x); i++) {
            for (size_t j = i; j < i + (mode == CONNECT_ALL ? lenfn(ports_a_in_len_x, ports_b_out_len_x) : 1); j++) {
                fn(client, ports_b_out[(b_ptn.idx_s+i%ports_b_out_len_x)%ports_b_out_len],
                        ports_a_in[(a_ptn.idx_s+j%ports_a_in_len_x)%ports_a_in_len]);
                cerr << what << " " << ports_b_out[(b_ptn.idx_s+i%ports_b_out_len_x)%ports_b_out_len]
                    << " and " << ports_a_in[(a_ptn.idx_s+j%ports_a_in_len_x)%ports_a_in_len] << endl;
                count++;
            }
        }
    if ((a_ptn.flags & JackPortIsOutput) && (b_ptn.flags & JackPortIsInput)
            && ports_a_out_len_x && ports_b_in_len_x)
        for (size_t i = 0; i < lenfn(ports_a_out_len_x, ports_b_in_len_x); i++) {
            for (size_t j = i; j < i + (mode == CONNECT_ALL ? lenfn(ports_a_in_len_x, ports_b_out_len_x) : 1); j++) {
                fn(client, ports_a_out[(a_ptn.idx_s+i%ports_a_out_len_x)%ports_a_out_len],
                        ports_b_in[(b_ptn.idx_s+j%ports_b_in_len_x)%ports_b_in_len]);
                cerr << what << " " << ports_a_out[(a_ptn.idx_s+i%ports_a_out_len_x)%ports_a_out_len]
                    << " and " << ports_b_in[(b_ptn.idx_s+j%ports_b_in_len_x)%ports_b_in_len] << endl;
                count++;
            }
        }
    jack_free(ports_a_in); jack_free(ports_b_in); jack_free(ports_a_out); jack_free(ports_b_out);
    if (count == 0)
        cerr << what << ": no matching ports" << endl;
}
void disconnect(string client_a, string client_b, string filter_a="", string filter_b="", int mode=CONNECT_MAX) {
    dis_connect_impl(false, client_a, client_b, filter_a, filter_b, mode);
}
void connect(string client_a, string client_b, string filter_a="", string filter_b="", int mode=CONNECT_MAX) {
    dis_connect_impl(true, client_a, client_b, filter_a, filter_b, mode);
}

void skip_to_now() {
    lock_guard<mutex> data_lk(data_mtx);
    ainqueue.resize(nch_in*(hop+window_size));
    aqueue.resize(0);
}

void init_audio() {
    jack_status_t status;
    client = jack_client_open(client_name.c_str(), JackNullOption, &status);
    if (!client) {
        cerr << "Failed to open jack client: " << status << endl;
        exit(1);
    }
    client_name = string(jack_get_client_name(client));
    set_num_channels(2,2);
    jack_set_process_callback(client, audio_cb, nullptr);
    jack_activate(client);
    cout << "Playing..." << endl;
}

sighandler_t prev_handlers[32];
void at_exit(int i) {
    cout << "Cleaning up... " << endl;
    jack_deactivate(client);
    unlink(command_file);
    signal(i, prev_handlers[i]);
    raise(i);
}

int main(int argc, char **argv) {
    command_file = argc > 1 ? argv[1] : "cmd";
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
#define CLIENT_NAME client_name
#include STR(HACK)
    this_thread::sleep_until(chrono::time_point<chrono::system_clock>::max());
#else
    #error Not using cling but no hack specified. Compile with `./compile --no-cling hack.cpp`
#endif
#endif
}

