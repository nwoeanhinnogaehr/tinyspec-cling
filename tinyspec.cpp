#include<cmath>
#include<unistd.h>
#include<cstring>
#include<complex>
#include<cassert>
#include<vector>
#include<string>
#include<iostream>
#include<thread>
#include<atomic>
#include<queue>
#include<sys/stat.h>
#include<fcntl.h>
#include<SDL2/SDL.h>
#include<SDL2/SDL_audio.h>
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
int new_fft_size = 1<<12; // initial size
vector<cplx> fft_out, fft_in;
vector<float> abuf, atmp;

using func_t = int(cplx *buf[2], int, double);
atomic<func_t*> fptr(0);
void init_cling(int argc, char **argv) {
    thread([argc, argv] {
        cling::Interpreter interp(argc, argv, LLVMRESDIR,
                true); // disable runtime for simplicity
        interp.setDefaultOptLevel(2);
        interp.process( // preamble
                "#include<complex>\n"
                "#include<iostream>\n"
                "#include<vector>\n"
                "using namespace std;\n"
                "using cplx=complex<double>;\n");

        // make a fifo called "cmd", which commands are read from
        mkfifo("cmd", 0700);
        int fd = open("cmd", O_RDONLY);

        const size_t buf_size = 1<<12;
        char codebuf[buf_size];
        string code;
        while (true) { // read code from fifo and execute it
            int n = read(fd, codebuf, buf_size);
            if (n == 0) sleep(0); // hack to avoid spinning too hard
            code += string(codebuf, n);
            int begin = code.find(CODE_BEGIN);
            int end = code.find(CODE_END);

            while (end != -1 && begin != -1) { // while there are blocks to execute
                string to_exec = code.substr(begin+strlen(CODE_BEGIN), end-begin-strlen(CODE_BEGIN));
                code = code.substr(end+strlen(CODE_END)-1);

                int nl_pos = to_exec.find("\n");
                int preview_len = nl_pos == -1 ? to_exec.size() : nl_pos;
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
    }).detach();
}

// fill one buffer for the FFT
void fill(cplx *buf[2], int n, double t) {
    if (fptr)
        new_fft_size = 1<<fptr.load()(buf, n, t);
}

// FFT author: Zachary Friggstad
void fft(cplx *in, cplx *v, int n, bool inverse) {
    assert(n > 0 && (n&(n-1)) == 0);
    for (int i = 0; i < n; ++i) {
        int r = 0, k = i;
        for (int j = 1; j < n; j <<= 1, r = (r<<1)|(k&1), k >>= 1);
        v[i] = in[r];
    }
    for (int m = 2; m <= n; m <<= 1) {
        int mm = m>>1;
        cplx zeta = polar<double>(1, (inverse?2:-2)*M_PI/m);
        for (int k = 0; k < n; k += m) {
            cplx om = 1;
            for (int j = 0; j < mm; ++j, om *= zeta) {
                cplx tl = v[k+j], th = om*v[k+j+mm];
                v[k+j] = tl+th;
                v[k+j+mm] = tl-th;
            }
        }
    }
}

SDL_AudioDeviceID adev;
queue<float> aqueue;

void audio_cb(void *, uint8_t *stream, int len) {
    float *out = (float*) stream;
    int underrun = 0;
    for (size_t i = 0; i < len/sizeof(float); i++) {
        if (aqueue.empty()) {
            out[i] = 0.0;
            underrun++;
        } else {
            out[i] = aqueue.front();
            aqueue.pop();
        }
    }
    if (underrun)
        cerr << "Warning: buffer underrun (" << underrun << " samples)" << endl;
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
    adev = SDL_OpenAudioDevice(NULL,0,&want,&have,0);
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
    for (double t = 0;; t += fft_size/(double)RATE/4) {
        if (new_fft_size != 0) {
            fft_size = new_fft_size;
            fft_out.resize(2*fft_size);
            fft_in.resize(2*fft_size);
            abuf.resize(fft_size);
            atmp.resize(fft_size);
            new_fft_size = 0;
        }
        while (aqueue.size() >= (size_t) max(fft_size, BUFFER_SIZE*8))
            sleep(0); // avoid queue growing too large
        memset(&fft_in[0], 0, fft_in.size()*sizeof(cplx));
        cplx* fft_buf[2] = {&fft_in[0], &fft_in[0]+fft_size};
        fill(fft_buf, fft_size/2, t);
        for (int c = 0; c < 2; c++) {
            fft(&fft_in[0]+c*fft_size, &fft_out[0]+c*fft_size, fft_size, true);
            for (int i = 0; i < fft_size/4; i++) {
                abuf[i*2+c] = (fft_out[i+c*fft_size].real()*(i/(fft_size/4.0))+atmp[i*2+c+fft_size/4]);
                atmp[i*2+c+fft_size/4] = fft_out[i+c*fft_size+fft_size/4].real()*(1.0-i/(fft_size/4.0));
            }
        }
        SDL_LockAudioDevice(adev); // prevent race with callback
        for (int i = 0; i < fft_size/2; i++)
            aqueue.push(abuf[i]);
        SDL_UnlockAudioDevice(adev);
    }
}
