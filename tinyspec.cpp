#include<cmath>
#include<unistd.h>
#include<cstring>
#include<complex>
#include<cassert>
#include<vector>
#include<string>
#include<iostream>
#include<atomic>
#include<queue>
#include<sys/stat.h>
#include<fcntl.h>
#include<SDL2/SDL.h>
#include<SDL2/SDL_audio.h>
#include<fftw3.h>
#include"cling/Interpreter/Interpreter.h"
#include"cling/Utils/Casting.h"
using namespace std;
typedef complex<double> cplx;

const char *LLVMRESDIR = "cling_bin"; // path to cling resource directory
const char *CODE_BEGIN = "<<<";
const char *CODE_END = ">>>";
const char *SYNTH_MAIN = "synth_main";
const int BUFFER_SIZE = 1024;

int fft_size;
atomic<int> new_fft_size(1<<12); // initial size
atomic<int> hop(new_fft_size/2);
cplx *fft_out = nullptr, *fft_in = nullptr;
vector<float> abuf, window;
SDL_AudioDeviceID adev;
queue<float> aqueue;
deque<float> atmp;
double time_secs = 0;
fftw_plan plan_left;
fftw_plan plan_right;

void set_next_size(int n, float hop_ratio=0.25) {
    new_fft_size = n;
    hop = int(n*hop_ratio);
    if (hop <= 0) {
        cerr << "ignoring invalid hop: " << hop << endl;
        hop = n/4;
    }
}

using func_t = void(cplx *buf[2], int, double);
atomic<func_t*> fptr(nullptr);
void init_cling(int argc, char **argv) {
    cling::Interpreter interp(argc, argv, LLVMRESDIR,
            true); // disable runtime for simplicity
    interp.setDefaultOptLevel(2);
    interp.process( // preamble
            "#include<complex>\n"
            "#include<iostream>\n"
            "#include<vector>\n"
            "using namespace std;\n"
            "using cplx=complex<double>;\n");
    interp.declare("void set_next_size(int n, float hop_ratio=0.25);");

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
                    cerr << "swap synth_main to " << addr << endl;
                }
            } else {
                interp.process(to_exec);
            }

            // move to next block
            begin = code.find(CODE_BEGIN);
            end = code.find(CODE_END);
        }
    }
}

void generate_frame() {
    int new_size_copy = new_fft_size;
    if (new_size_copy != fft_size) {
        fft_size = new_size_copy;
        if (fft_out) fftw_free(fft_out);
        if (fft_in) fftw_free(fft_in);
        fft_out = (cplx*) fftw_malloc(sizeof(cplx) * fft_size*2);
        fft_in = (cplx*) fftw_malloc(sizeof(cplx) * fft_size*2);
        plan_left = fftw_plan_dft_1d(fft_size,
                (fftw_complex*) fft_in, (fftw_complex*) fft_out,
                FFTW_BACKWARD, FFTW_ESTIMATE);
        plan_right = fftw_plan_dft_1d(fft_size,
                (fftw_complex*) fft_in+fft_size, (fftw_complex*) fft_out+fft_size,
                FFTW_BACKWARD, FFTW_ESTIMATE);
        abuf.resize(fft_size);
        window.resize(fft_size);
        for (int i = 0; i < fft_size; i++)
            window[i] = i<=fft_size/2 ? i/(fft_size/2.0) : (fft_size-i-1)/(fft_size/2.0);
    }
    memset(fft_in, 0, fft_size*2*sizeof(cplx));
    cplx* fft_buf[2] = {fft_in, fft_in+fft_size};
    if (fptr) // call synthesis function
        fptr.load()(fft_buf, fft_size/2, time_secs);
    fftw_execute(plan_left);
    fftw_execute(plan_right);

    int hop_fix = hop; // fix current value of hop
    // output region that overlaps with previous frame(s)
    for (int i = 0; i < min(hop_fix, fft_size); i++) {
        for (int c = 0; c < 2; c++) {
            float overlap = 0;
            if (!atmp.empty()) {
                overlap = atmp.front();
                atmp.pop_front();
            }
            aqueue.push(overlap + fft_out[i+c*fft_size].real()*window[i]);
        }
    }
    // if the hop is larger than the frame size, insert silence
    for (int i = 0; i < 2*(hop_fix-fft_size); i++)
        aqueue.push(0);
    // save region that overlaps with next frame
    for (int i = 0; i < fft_size-hop_fix; i++) {
        for (int c = 0; c < 2; c++) {
            float out_val = fft_out[hop_fix+i+c*fft_size].real()*window[i+hop_fix];
            if (i*2+c < (int)atmp.size()) atmp[i*2+c] += out_val;
            else atmp.push_back(out_val);
        }
    }

    time_secs += hop_fix/(double)RATE;
}

void audio_cb(void *, uint8_t *stream, int len) {
    float *out = (float*) stream;
    for (size_t i = 0; i < len/sizeof(float); i++) {
        if (aqueue.empty())
            generate_frame();
        out[i] = aqueue.front();
        aqueue.pop();
    }
}

void init_audio() {
    SDL_Init(SDL_INIT_AUDIO);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = RATE;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = BUFFER_SIZE;
    want.callback = audio_cb;
    adev = SDL_OpenAudioDevice(nullptr,0,&want,&have,0);
    if (adev == 0) {
        cerr << "Failed to open audio: " << SDL_GetError() << endl;
        exit(1);
    }

    SDL_PauseAudioDevice(adev,0);
    cerr << "Playing... " << endl;
}

int main(int argc, char **argv) {
    init_audio();
    init_cling(argc, argv);
}
