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
#include<fcntl.h>
#include<jack/jack.h>
#include<fftw3.h>
#include"cling/Interpreter/Interpreter.h"
#include"cling/Utils/Casting.h"
#include"osc.h"
using namespace std;
typedef complex<double> cplx;

const char *LLVMRESDIR = "cling_bin"; // path to cling resource directory
const char *CODE_BEGIN = "<<<";
const char *CODE_END = ">>>";
const char *SYNTH_MAIN = "process";

int fft_size;
int new_fft_size(1<<12); // initial size
int hop(new_fft_size/2);
cplx *fft_out = nullptr, *fft_in = nullptr;
vector<float> window;
deque<float> aqueue, atmp, ainqueue;
uint64_t time_samples = 0;
fftw_plan plan_fwd, plan_inv;
size_t nch_in = 0, nch_out = 0, nch_prev = 0;
mutex frame_notify_mtx, // for waking up processing thread
      exec_mtx, // guards user code exec
      data_mtx; // guards aqueue, ainqueue
condition_variable frame_notify;
bool frame_ready = false;

// builtins
void set_hop(int new_hop) {
    if (new_hop <= 0) cerr << "ignoring invalid hop: " << new_hop << endl;
    else hop = new_hop;
}
void next_hop_samples(uint32_t n, uint32_t h) {
    new_fft_size = n;
    set_hop(h);
}
void next_hop_ratio(uint32_t n, double ratio=0.25) {
    new_fft_size = n;
    set_hop(n*ratio);
}
void next_hop_hz(uint32_t n, double hz) {
    new_fft_size = n;
    set_hop(double(RATE)/hz);
}

using func_t = void(cplx **, int, cplx **, int, int, double);
func_t* fptr(nullptr);
void init_cling(int argc, char **argv) { cling::Interpreter interp(argc, argv, LLVMRESDIR,
            true); // disable runtime for simplicity
    interp.setDefaultOptLevel(2);
    interp.process( // preamble
            "#include<complex>\n"
            "#include<iostream>\n"
            "#include<vector>\n"
            "#include\"osc_rt.h\"\n"
            "using namespace std;\n"
            "using cplx=complex<double>;\n");
    interp.declare("void next_hop_ratio(uint32_t n, double hop_ratio=0.25);");
    interp.declare("void next_hop_samples(uint32_t n, uint32_t h);");
    interp.declare("void next_hop_hz(uint32_t n, double hz);");
    interp.declare("void set_num_channels(size_t in, size_t out);");
    interp.declare("void skip_to_now();");

    // make a fifo called "cmd", which commands are read from
    const char *command_file = argc > 1 ? argv[1] : "cmd";
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

            // cling can't redefine functions, so replace the name of the entry point
            // with a unique name
            int name_loc = to_exec.find(SYNTH_MAIN);
            if (name_loc != -1) {
                string uniq;
                interp.createUniqueName(uniq);
                to_exec.replace(name_loc, strlen(SYNTH_MAIN), uniq);

                if (cling::Interpreter::kSuccess == interp.process(to_exec)) {
                    void *addr = interp.getAddressOfGlobal(uniq);
                    fptr = cling::utils::VoidToFunctionPtr<func_t*>(addr);
                    cerr << "swap " << SYNTH_MAIN << " to " << addr << endl;
                }
            } else {
                interp.process(to_exec);
            }

            exec_mtx.unlock();

            // move to next block
            begin = code.find(CODE_BEGIN);
            end = code.find(CODE_END);
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
            size_t nch = max(nch_in, nch_out);
            if (nch != nch_prev || new_fft_size != fft_size) {
                fft_size = new_fft_size;
                nch_prev = nch;
                if (fft_size) {
                    if (fft_out) fftw_free(fft_out);
                    if (fft_in) fftw_free(fft_in);
                    fft_out = (cplx*) fftw_malloc(sizeof(cplx)*nch*fft_size);
                    fft_in = (cplx*) fftw_malloc(sizeof(cplx)*nch*fft_size);
                    int n[] = {fft_size};
                    plan_fwd = fftw_plan_many_dft(1, n, nch_in,
                            (fftw_complex*) fft_in, nullptr, 1, fft_size,
                            (fftw_complex*) fft_out, nullptr, 1, fft_size,
                            FFTW_FORWARD, FFTW_ESTIMATE);
                    plan_inv = fftw_plan_many_dft(1, n, nch_out,
                            (fftw_complex*) fft_in, nullptr, 1, fft_size,
                            (fftw_complex*) fft_out, nullptr, 1, fft_size,
                            FFTW_BACKWARD, FFTW_ESTIMATE);
                    window.resize(fft_size);
                    for (int i = 0; i < fft_size; i++)
                        window[i] = sqrt(0.5*(1-cos(2*M_PI*i/fft_size))); // Hann
                }
            }
            memset(fft_in, 0, sizeof(cplx)*nch*fft_size);
            {
                lock_guard<mutex> data_lk(data_mtx);
                if (ainqueue.size() < nch_in*(hop+fft_size))
                    break;
                size_t sz_tmp = ainqueue.size();
                for (size_t i = 0; i < min(nch_in*hop, sz_tmp); i++)
                    ainqueue.pop_front();
                for (int i = 0; i < fft_size; i++) {
                    for (size_t c = 0; c < nch_in; c++) {
                        if (i*nch_in+c < ainqueue.size())
                            fft_in[i+c*fft_size] = ainqueue[i*nch_in+c]/fft_size*window[i];
                        else fft_in[i+c*fft_size] = 0;
                    }
                }
            }
            memset(fft_out, 0, sizeof(cplx)*nch*fft_size);
            if (fft_size) fftw_execute(plan_fwd);

            cplx *fft_buf_in[nch], *fft_buf_out[nch];
            for (size_t c = 0; c < nch; c++) {
                fft_buf_in[c] = fft_in + c*fft_size;
                fft_buf_out[c] = fft_out + c*fft_size;
            }
            memset(fft_in, 0, sizeof(cplx)*nch*fft_size);
            if (fptr) // call synthesis function
                fptr(fft_buf_out, nch_in, fft_buf_in, nch_out, fft_size/2, time_samples/(double)RATE);
            for (int i = 0; i < fft_size/2; i++)
                for (size_t c = 0; c < nch_out; c++)
                    fft_buf_in[c][i+fft_size/2] = conj(fft_buf_in[c][fft_size/2-i]);
            if (fft_size) fftw_execute(plan_inv);

            {
                lock_guard<mutex> data_lk(data_mtx);
                // output region that overlaps with previous frame(s)
                for (int i = 0; i < min(hop, fft_size); i++) {
                    for (size_t c = 0; c < nch_out; c++) {
                        float overlap = 0;
                        if (!atmp.empty()) {
                            overlap = atmp.front();
                            atmp.pop_front();
                        }
                        aqueue.push_back(overlap + fft_out[i+c*fft_size].real()*window[i]);
                    }
                }
                // if the hop is larger than the frame size, insert silence
                for (int i = 0; i < int(nch_out)*(hop-fft_size); i++)
                    aqueue.push_back(0);
            }
            // save region that overlaps with next frame
            for (int i = 0; i < fft_size-hop; i++) {
                for (size_t c = 0; c < nch_out; c++) {
                    float out_val = fft_out[hop+i+c*fft_size].real()*window[i+hop];
                    if (i*nch_out+c < atmp.size()) atmp[i*nch_out+c] += out_val;
                    else atmp.push_back(out_val);
                }
            }

            time_samples += hop;
        }
    }
}

jack_client_t *client;
vector<jack_port_t *> in_ports;
vector<jack_port_t *> out_ports;

int audio_cb(jack_nframes_t len, void *) {
    float *in_bufs[in_ports.size()];
    float *out_bufs[out_ports.size()];
    for (size_t i = 0; i < nch_in; i++)
        in_bufs[i] = (float*) jack_port_get_buffer(in_ports[i], len);
    for (size_t i = 0; i < nch_out; i++)
        out_bufs[i] = (float*) jack_port_get_buffer(out_ports[i], len);
    lock_guard<mutex> data_lk(data_mtx);
    for (size_t i = 0; i < len; i++)
        for (size_t c = 0; c < nch_in; c++)
            ainqueue.push_back(in_bufs[c][i]);
    {
        unique_lock<mutex> lk(frame_notify_mtx);
        frame_ready = true;
    }
    frame_notify.notify_one();
    for (size_t i = 0; i < len; i++)
        for (size_t c = 0; c < nch_out; c++) {
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
    for (size_t i = in; i < nch_in; i++) {
        jack_port_unregister(client, in_ports.back());
        in_ports.pop_back();
    }
    for (size_t i = nch_in; i < in; i++) {
        string name = "in" + to_string(i);
        in_ports.push_back(jack_port_register(client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0));
    }
    for (size_t i = out; i < nch_out; i++) {
        jack_port_unregister(client, out_ports.back());
        out_ports.pop_back();
    }
    for (size_t i = nch_out; i < out; i++) {
        string name = "out" + to_string(i);
        out_ports.push_back(jack_port_register(client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0));
    }
    nch_in = in;
    nch_out = out;
}

void skip_to_now() {
    lock_guard<mutex> data_lk(data_mtx);
    ainqueue.resize(nch_in*(hop+fft_size));
    aqueue.resize(0);
}

void init_audio(int argc, char **argv) {
    const char *command_file = argc > 1 ? argv[1] : "cmd";
    string client_name = string("tinyspec_") + command_file;
    jack_status_t status;
    client = jack_client_open(client_name.c_str(), JackNullOption, &status);
    if (!client) {
        cerr << "Failed to open jack client: " << status << endl;
        exit(1);
    }
    set_num_channels(2,2);
    jack_set_process_callback(client, audio_cb, nullptr);
    jack_activate(client);
}

int main(int argc, char **argv) {
    gettimeofday(&init_time, NULL);
    init_audio(argc, argv);
    thread worker(generate_frames);
    init_cling(argc, argv);
}
