namespace internals {
    extern double hop;
}
void PhaseVocoder::analyze(WaveBuf &other) {
    window_sqrt_hann(other);
    fft.fill_from(other);
    frft(fft, fft, 1);
    analyze(fft);
}
void PhaseVocoder::synthesize(WaveBuf &other) {
    other.resize(num_channels, size*2);
    synthesize(fft);
    frft(fft, fft, -1);
    other.fill_from<cplx>(fft, [](cplx x){return x.real();});
    window_sqrt_hann(other);
}
void PhaseVocoder::analyze(FFTBuf &other) {
    resize(other.num_channels, other.size/2);
    for (size_t i = 0; i < num_channels; i++) {
        for (size_t j = 0; j < size; j++) {
            double amp = abs(other[i][j]);
            double phase = arg(other[i][j]);
            double freq = phase_to_frequency(j, phase - (*this)[i][j].last_phase);
            (*this)[i][j].last_phase = phase;
            (*this)[i][j].amp = amp;
            (*this)[i][j].freq = freq;
        }
    }
}
void PhaseVocoder::synthesize(FFTBuf &other) {
    other.resize(num_channels, size*2);
    for (size_t i = 0; i < num_channels; i++) {
        for (size_t j = 0; j < size; j++) {
            double phase = frequency_to_phase((*this)[i][j].freq);
            (*this)[i][j].phase_sum += phase;
            other[i][j] = std::polar((*this)[i][j].amp, (*this)[i][j].phase_sum);
            other[i][size*2-j-1] = std::conj(other[i][j]);
        }
    }
}
double PhaseVocoder::phase_to_frequency(size_t bin, double phase_diff) {
    double delta = phase_diff - bin*2.0*M_PI*hop()/size;
    int qpd = (int)(delta / M_PI);
    if (qpd >= 0) qpd += qpd & 1;
    else qpd -= qpd & 1;
    return (bin + size/hop()*(delta - M_PI*qpd)/M_PI/2)*sample_rate()/size;
}
double PhaseVocoder::frequency_to_phase(double freq) {
    return fmod(2.0*M_PI*freq/sample_rate()*hop(), M_PI*2);
}
void PhaseVocoder::shift(std::function<double(double)> fn) {
    shift([&](WaveBuf& buf) {
        for (size_t i = 0; i < num_channels; i++)
            for (size_t j = 0; j < size/2; j++)
                buf[i][j] = fn(buf[i][j]);
    });
}
void PhaseVocoder::shift(std::function<void(WaveBuf&)> fn) {
    WaveBuf bins(num_channels, size);
    WaveBuf freqs(num_channels, size);
    for (size_t i = 0; i < num_channels; i++) {
        for (size_t j = 0; j < size; j++) {
            bins[i][j] = j/2.0/size*sample_rate();
            freqs[i][j] = (*this)[i][j].freq;
        }
    }
    fn(bins);
    fn(freqs);
    PVBuf tmp;
    tmp.fill_from(*this);
    for (size_t i = 0; i < num_channels; i++)
        for (size_t j = 0; j < size; j++)
            (*this)[i][j].amp = 0;
    for (size_t i = 0; i < num_channels; i++) {
        for (size_t j = 0; j < size; j++) {
            int k = bins[i][j]*2.0/sample_rate()*size;
            if (k >= 0 && k < (int)size) {
                (*this)[i][k].amp += tmp[i][j].amp;
                (*this)[i][k].freq = freqs[i][j];
            }
        }
    }
}
