#include <fftw3.h>
#include <functional>
#include <cassert>
typedef std::complex<double> cplx;

struct FFTBuf;
struct PVBuf;

void frft(FFTBuf &in, FFTBuf &out, double exponent);

struct WaveBuf {
    double *data = nullptr;
    size_t size = 0;
    size_t num_channels = 0;
    WaveBuf() {}
    WaveBuf(size_t num_channels, size_t size) { resize(num_channels, size); }
    ~WaveBuf() { if (data) free(data); }
    void resize(size_t num_channels, size_t size) {
        if (this->size*this->num_channels != size*num_channels) {
            this->size = size;
            this->num_channels = num_channels;
            if (data) free(data);
            data = (double*) malloc(sizeof(double)*num_channels*size);
            zero();
        }
    }
    void fill_from(WaveBuf &other) {
        resize(other.num_channels, other.size);
        std::copy(other.data, other.data+num_channels*size, data);
    }
    void fill_from(FFTBuf &other, std::function<double(cplx)> mix=[](cplx x){ return x.real(); });
    void zero() {
        for (size_t i = 0; i < size*num_channels; i++)
            data[i] = 0;
    }
    double* operator[](int index) { return data + index*size; }
};

struct FFTBuf {
    cplx *data = nullptr;
    size_t size = 0;
    size_t num_channels = 0;
    FFTBuf() {}
    FFTBuf(size_t num_channels, size_t size) { resize(num_channels, size); }
    ~FFTBuf() { if (data) fftw_free(data); }
    void resize(size_t num_channels, size_t size) {
        if (this->size*this->num_channels != size*num_channels) {
            this->size = size;
            this->num_channels = num_channels;
            if (data) fftw_free(data);
            data = (cplx*) fftw_malloc(sizeof(cplx)*num_channels*size);
            for (size_t i = 0; i < size*num_channels; i++)
                data[i] = 0;
        }
    }
    void fill_from(WaveBuf &other) {
        resize(other.num_channels, other.size);
        for (size_t i = 0; i < num_channels; i++)
            for (size_t j = 0; j < size; j++)
                data[i*size + j] = other[i][j];
    }
    void fill_from(FFTBuf &other) {
        resize(other.num_channels, other.size);
        std::copy(other.data, other.data+num_channels*size, data);
    }
    void zero() {
        for (size_t i = 0; i < size*num_channels; i++)
            data[i] = 0;
    }
    cplx* operator[](int index) { return data + index*size; }
};

void window_sqrt_hann(WaveBuf &data) {
    //todo cache windows somehow
    for (size_t i = 0; i < data.size; i++) {
        double w = sqrt(0.5*(1-cos(2*M_PI*i/data.size))); // Hann
        for (size_t j = 0; j < data.num_channels; j++)
            data[j][i] *= w;
    }
}

//PV = Phase Vocoder
struct PVBin {
    double amp, freq;
    PVBin(double amp, double freq) : amp(amp), freq(freq) { }
};
namespace internals {
    extern size_t hop;
}
struct PVBuf {
    PVBin *data = nullptr;
    size_t size = 0;
    size_t num_channels = 0;
    PVBuf() { }
    PVBuf(size_t num_channels, size_t size) { }
    ~PVBuf() { if (data) free(data); }
    void resize(size_t num_channels, size_t size) {
        if (this->size*this->num_channels != size*num_channels) {
            this->size = size;
            this->num_channels = num_channels;
            if (data) free(data);
            data = (PVBin*) malloc(sizeof(PVBin)*num_channels*size);
            zero();
        }
    }
    void zero() {
        for (size_t i = 0; i < size*num_channels; i++)
            data[i] = PVBin(0,0);
    }
    void fill_from(PVBuf &other) {
        resize(other.num_channels, other.size);
        std::copy(other.data, other.data+num_channels*size, data);
    }
    PVBin* operator[](int index) { return data + index*size; }
};
struct PhaseVocoder {
    PVBuf data;
    WaveBuf last_phase;
    WaveBuf phase_sum;
    size_t size = 0;
    size_t num_channels = 0;
    FFTBuf fft;
    bool global_hop;
    double user_hop;
    PhaseVocoder() { use_global_hop(); }
    PhaseVocoder(size_t num_channels, size_t size) : PhaseVocoder() { resize(num_channels, size); }
    void resize(size_t num_channels, size_t size) {
        if (this->size*this->num_channels != size*num_channels) {
            this->size = size;
            this->num_channels = num_channels;
            data.resize(num_channels, size);
            last_phase.resize(num_channels, size);
            phase_sum.resize(num_channels, size);
        }
    }
    void reset(bool last=true, bool sum=true) {
        if (last) last_phase.zero();
        if (sum) phase_sum.zero();
    }
    void fill_from(PhaseVocoder &other) {
        num_channels = other.num_channels;
        size = other.size;
        global_hop = other.global_hop;
        user_hop = other.user_hop;
        data.fill_from(other.data);
        last_phase.fill_from(other.last_phase);
        phase_sum.fill_from(other.phase_sum);
    }
    void use_global_hop() { global_hop = true; }
    void set_hop(double new_hop) {
        user_hop = new_hop;
        global_hop = false;
    }
    double hop() {
        if (global_hop) return internals::hop;
        else return user_hop;
    }
    void analyze(WaveBuf &other) {
        window_sqrt_hann(other);
        fft.fill_from(other);
        frft(fft, fft, 1);
        analyze(fft);
    }
    void synthesize(WaveBuf &other) {
        other.resize(num_channels, size);
        synthesize(fft);
        frft(fft, fft, -1);
        other.fill_from(fft);
        window_sqrt_hann(other);
    }
    void analyze(FFTBuf &other) {
        resize(other.num_channels, other.size);
        for (size_t i = 0; i < num_channels; i++) {
            for (size_t j = 0; j < size; j++) {
                double amp = abs(other[i][j]);
                double phase = arg(other[i][j]);
                double freq = phase_to_frequency(j, phase - last_phase[i][j]);
                last_phase[i][j] = phase;
                data[i][j] = PVBin(amp, freq);
            }
        }
    }
    void synthesize(FFTBuf &other) {
        other.resize(num_channels, size);
        for (size_t i = 0; i < num_channels; i++) {
            for (size_t j = 0; j < size; j++) {
                double phase = frequency_to_phase(j, data[i][j].freq);
                phase_sum[i][j] += phase;
                other[i][j] = std::polar(data[i][j].amp, phase_sum[i][j]);
            }
        }
    }
    double phase_to_frequency(size_t bin, double phase_diff) {
        double delta = phase_diff - bin*2.0*M_PI*hop()/size;
        int qpd = (int)(delta / M_PI);
        if (qpd >= 0) qpd += qpd & 1;
        else qpd -= qpd & 1;
        return (bin + size/hop()*(delta - M_PI*qpd)/2.0/M_PI)*RATE/size;
    }
    double frequency_to_phase(size_t bin, double freq) {
        return 2.0*M_PI*freq/RATE*hop();
    }
    void shift(std::function<double(double)> fn) {
        shift([&](WaveBuf& buf) {
            for (size_t i = 0; i < num_channels; i++)
                for (size_t j = 0; j < size/2; j++)
                    buf[i][j] = fn(buf[i][j]);
        });
    }
    void shift(std::function<void(WaveBuf&)> fn) {
        WaveBuf bins(num_channels, size/2);
        WaveBuf freqs(num_channels, size/2);
        for (size_t i = 0; i < num_channels; i++) {
            for (size_t j = 0; j < size/2; j++) {
                bins[i][j] = j/(double)size*RATE;
                freqs[i][j] = data[i][j].freq;
            }
        }
        fn(bins);
        fn(freqs);
        PVBuf tmp;
        tmp.fill_from(data);
        data.zero();
        for (size_t i = 0; i < num_channels; i++) {
            for (size_t j = 0; j < size/2; j++) {
                int k = bins[i][j]/RATE*size;
                if (k >= 0 && k < (int)size/2) {
                    data[i][k].amp += tmp[i][j].amp;
                    data[i][k].freq = freqs[i][j];
                }
            }
        }
    }
    PVBin* operator[](int index) { return data[index]; }
};

void WaveBuf::fill_from(FFTBuf &other, std::function<double(cplx)> mix) {
    resize(other.num_channels, other.size);
    for (size_t i = 0; i < num_channels; i++)
        for (size_t j = 0; j < size; j++)
            data[i*size + j] = mix(other[i][j]);
}

