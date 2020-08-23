next_hop_ratio(1<<9, 0.5);
set_num_channels(0, 2);
connect(CLIENT_NAME, "system");
set_process_fn([&](WaveBuf&, WaveBuf &out, double t) {
    static FFTBuf buf;
    static Buf<int> feed;
    buf.resize(out.num_channels, out.size*2);
    feed.resize(out.num_channels, out.size);
    int n = out.size;
    for (size_t c = 0; c < out.num_channels; c++) {
        for (int i = 1; i < n; i++) {
            int r = (int)(t*12)*n/2+i+feed[c][i];
            feed[c][i] += r/16^r/63^r/1023^c;
            if (int mod = (r/16^r/65^r/1024))
                feed[c][i] %= mod;
            buf[c][i] = (exp(cplx(0.0,M_PI*sin(M_PI*feed[c][i]/double(n-i)))))/pow(i, 1.0)*8.0;
            buf[c][n*2-i] = conj(buf[c][i]);
        }
    }
    frft(buf, buf, -1);
    for (size_t c = 0; c < out.num_channels; c++)
        for (int i = 0; i < n; i++)
            out[c][i] = buf[c][i].real() * (1.0-abs(1.0-i/double(n/2)));
});
