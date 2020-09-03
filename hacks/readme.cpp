// Connect to the speakers
set_num_channels(0, 2); // no inputs, stereo output
connect(CLIENT_NAME, "system");
// Called periodically to fill up a new buffer.
// in and out are audio sample buffers
// n is the number of samples in the frame
// t is the time in seconds since the beginning of playback.
set_process_fn([&](WaveBuf& in, WaveBuf& out, double t){
    FFTBuf fft;
    fft.resize(out.num_channels, out.size);
    // Loop over frequency bins. Starting at 1 skips the DC offset.
    for (size_t c = 0; c < out.num_channels; c++) {
        for (size_t i = 1; i < out.size; i++) {
            cplx x = sin(i*pow(1.0/(i+1), 1.0+sin(t*i*M_PI/8+c)*0.5))*25 // Some random formula
                /pow(i,0.9); // Scale magnitude to prevent loud high frequency noises.
            fft[c][i] = x; // Fill output buffer
        }
    }
    frft(fft, fft, -1.0); // Perform in-place inverse FFT
    out.fill_from<cplx>(fft, [](cplx x){return x.real();}); // Copy real part to output
    window_hann(out); // Apply Hann window
    next_hop_ratio(4096); // Set FFT size for the next frame
});
