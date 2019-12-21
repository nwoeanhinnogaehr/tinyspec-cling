#include <fftw3.h>
typedef std::complex<double> cplx;

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
    void fill(size_t num_channels, size_t fft_size, double **real) {
        resize(num_channels, fft_size);
        for (size_t i = 0; i < num_channels; i++)
            for (size_t j = 0; j < fft_size; j++)
                data[i*fft_size + j] = real[i][j];
    }
    cplx* operator[](int index) { return data + index*fft_size; }
};

void window_sqrt_hann(size_t nch, size_t n, double **data) {
    //todo cache windows somehow
    for (size_t i = 0; i < n; i++) {
        double w = sqrt(0.5*(1-cos(2*M_PI*i/n))); // Hann
        for (size_t j = 0; j < nch; j++)
            data[j][i] *= w;
    }
}
