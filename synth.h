#include <fftw3.h>
typedef std::complex<double> cplx;


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
            data = (double*) malloc(sizeof(cplx)*num_channels*size);
            for (size_t i = 0; i < size*num_channels; i++)
                data[i] = 0;
        }
    }
    double* operator[](int index) { return data + index*size; }
};

struct FFTBuf {
    cplx *data = nullptr;
    size_t fft_size = 0;
    size_t num_channels = 0;
    FFTBuf() {}
    FFTBuf(size_t num_channels, size_t fft_size) { resize(num_channels, fft_size); }
    ~FFTBuf() { if (data) fftw_free(data); }
    void resize(size_t num_channels, size_t fft_size) {
        if (this->fft_size*this->num_channels != fft_size*num_channels) {
            this->fft_size = fft_size;
            this->num_channels = num_channels;
            if (data) fftw_free(data);
            data = (cplx*) fftw_malloc(sizeof(cplx)*num_channels*fft_size);
            for (size_t i = 0; i < fft_size*num_channels; i++)
                data[i] = 0;
        }
    }
    void fill(WaveBuf &real) {
        resize(real.num_channels, real.size);
        for (size_t i = 0; i < num_channels; i++)
            for (size_t j = 0; j < fft_size; j++)
                data[i*fft_size + j] = real[i][j];
    }
    cplx* operator[](int index) { return data + index*fft_size; }
};

void window_sqrt_hann(WaveBuf &data) {
    //todo cache windows somehow
    for (size_t i = 0; i < data.size; i++) {
        double w = sqrt(0.5*(1-cos(2*M_PI*i/data.size))); // Hann
        for (size_t j = 0; j < data.num_channels; j++)
            data[j][i] *= w;
    }
}
