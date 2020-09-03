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

