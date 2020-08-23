next_hop_ratio(1<<8, 0.5);
set_num_channels(0, 2);
connect(CLIENT_NAME, "system");
set_process_fn([&](WaveBuf&, WaveBuf &out, int, double t) {
    static FFTBuf buf;
    static Buf<int> sum;
    sum.resize(out.num_channels, out.size);
    buf.resize(out.num_channels, out.size*2);
    int n = out.size;
    for (size_t c = 0; c < out.num_channels; c++) {
        for (int i = 1; i < n; i++) {
            int r = (int)(t*8)*n/2+i+sum[c][i];
            sum[c][i] += r/64|(r/128&r/512);
            sum[c][i] %= (r/63^(r/128&r/512))+32+c;
            buf[c][i] = (exp(cplx(0.0,2.0*M_PI*sin(M_PI*sum[c][i]/double(n-i)))))/pow(i+1, 1.0)*2.0;
            buf[c][n*2-i] = conj(buf[c][i]);
        }
    }
    frft(buf, buf, -1);
    for (size_t c = 0; c < out.num_channels; c++)
        for (int i = 0; i < n; i++)
            out[c][i] = buf[c][i].real() * (1.0-abs(1.0-i/(n/2.0)));
});
