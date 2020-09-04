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
    other.resize(num_channels, size);
    synthesize(fft);
    frft(fft, fft, -1);
    other.fill_from<cplx>(fft, [](cplx x){return x.real();});
    window_sqrt_hann(other);
}
void PhaseVocoder::analyze(FFTBuf &other) {
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
void PhaseVocoder::synthesize(FFTBuf &other) {
    other.resize(num_channels, size);
    for (size_t i = 0; i < num_channels; i++) {
        for (size_t j = 0; j < size; j++) {
            double phase = frequency_to_phase(data[i][j].freq);
            phase_sum[i][j] += phase;
            other[i][j] = std::polar(data[i][j].amp, phase_sum[i][j]);
        }
    }
}
double PhaseVocoder::phase_to_frequency(size_t bin, double phase_diff) {
    double delta = phase_diff - bin*2.0*M_PI*hop()/size;
    int qpd = (int)(delta / M_PI);
    if (qpd >= 0) qpd += qpd & 1;
    else qpd -= qpd & 1;
    return (bin + size/hop()*(delta - M_PI*qpd)/2.0/M_PI)*RATE/size;
}
double PhaseVocoder::frequency_to_phase(double freq) {
    return 2.0*M_PI*freq/RATE*hop();
}
void PhaseVocoder::shift(std::function<double(double)> fn) {
    shift([&](WaveBuf& buf) {
        for (size_t i = 0; i < num_channels; i++)
            for (size_t j = 0; j < size/2; j++)
                buf[i][j] = fn(buf[i][j]);
    });
}
void PhaseVocoder::shift(std::function<void(WaveBuf&)> fn) {
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
