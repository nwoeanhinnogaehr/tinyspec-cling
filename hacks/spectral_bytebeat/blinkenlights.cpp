next_hop_ratio(1<<9, 0.5);
set_num_channels(0, 2);
connect(CLIENT_NAME, "system");
set_process_fn([&](WaveBuf&, WaveBuf &out, int, double t) {
    static FFTBuf buf;
    static int x;
    buf.resize(out.num_channels, out.size*2);
    int n = out.size;
    for (size_t c = 0; c < out.num_channels; c++) {
        for (int i = 1; i < n; i++) {
            int r = (int)(t*8)*n+i;
            int y = int(t/2);
            int a = y/3^y/5;
            int b = y/2^y/7;
            x += (r/(3+a%17)&r/(5+b%11));
            x ^= r;
            buf[c][i] = (exp(cplx(0.0,M_PI/16.0*x)))/pow(i+1, 1.0)*4.0;
            buf[c][n*2-i] = conj(buf[c][i]);
        }
    }
    frft(buf, buf, -1);
    for (size_t c = 0; c < out.num_channels; c++)
        for (int i = 0; i < n; i++)
            out[c][i] = buf[c][i].real() * (1.0-abs(1.0-i/(n/2.0)));
});
