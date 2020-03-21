set_process_fn([&](WaveBuf &in, WaveBuf &out, int n, double t) {
    double shift = sin(t)/2+1;
    window_hann(in);
    out.resize(2, n/shift);
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < n; j++)
            for (int k = j/shift; k < (j+1)/shift; k++)
                out[i][k] = in[i][j];
    next_hop_ratio(1800,0.5/shift);
});
