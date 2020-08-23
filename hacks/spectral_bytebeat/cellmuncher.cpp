next_hop_ratio(1<<11, 0.5);
set_num_channels(0, 2);
connect(CLIENT_NAME, "system");
set_process_fn([&](WaveBuf&, WaveBuf &out, int, double t) {
    static FFTBuf buf;
    buf.resize(out.num_channels, out.size*2);
    int n = out.size;
    for (size_t c = 0; c < out.num_channels; c++) {
        for (int i = 1; i < n; i++) {
            int r = int(t*16)*n+i;
            buf[c][n*2-i]=buf[c][i]=sin(2.0*M_PI/128.*(c+((int)pow(r,1.1))^r/13^r>>8|r>>6^r>>14))/pow(i,0.8)*25;
        }
    }
    frft(buf, buf, -1);
    for (size_t c = 0; c < out.num_channels; c++)
        for (int i = 0; i < n; i++)
            out[c][i] = buf[c][i].real() * (1.0-abs(1.0-i/(n/2.0)));
});
