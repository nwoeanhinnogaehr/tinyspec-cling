void window_hann(WaveBuf &data) {
    for (size_t i = 0; i < data.size; i++) {
        double w = 0.5*(1-cos(2*M_PI*i/data.size)); // Hann
        for (size_t j = 0; j < data.num_channels; j++)
            data[j][i] *= w;
    }
}
void window_sqrt_hann(WaveBuf &data) {
    for (size_t i = 0; i < data.size; i++) {
        double w = sqrt(0.5*(1-cos(2*M_PI*i/data.size))); // Hann
        for (size_t j = 0; j < data.num_channels; j++)
            data[j][i] *= w;
    }
}
