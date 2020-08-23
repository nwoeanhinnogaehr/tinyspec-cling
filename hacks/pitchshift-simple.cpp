connect(CLIENT_NAME, "system");
set_process_fn([&](WaveBuf &in, WaveBuf &out, double t) {
    double shift = sin(t)/2+1;
    window_hann(in);
    out.resize(in.num_channels, in.size/shift);
    for (size_t i = 0; i < in.num_channels; i++)
        for (size_t j = 0; j < in.size; j++)
            for (int k = j/shift; k < (j+1)/shift; k++)
                out[i][k] = in[i][j];
    next_hop_ratio(1800,0.5/shift);
});
