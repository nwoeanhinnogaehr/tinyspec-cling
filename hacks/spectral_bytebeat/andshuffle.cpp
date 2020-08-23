next_hop_ratio(1<<17, 0.5);
set_num_channels(0, 2);
connect(CLIENT_NAME, "system");
set_process_fn([&](WaveBuf&, WaveBuf &out, int, double t) {
    static FFTBuf buf;
    buf.resize(out.num_channels, out.size*2);
    int n = out.size;
    for (size_t c = 0; c < out.num_channels; c++) {
        for (int i = 1; i < n; i++) {
            int r = int(t*8)*n/2+n-i;
            int j = r/128+c;
            double mag = sin((1+fmod(i*i, (1+(double)(((j/5&j/7&(j/64^j/127))^j>>16)%128)))))/pow(i, 0.75)*32;
            buf[c][i] = cplx(mag,mag);
            buf[c][n*2-i] = conj(buf[c][i]);
        }
    }
    frft(buf, buf, -1);
    for (size_t c = 0; c < out.num_channels; c++)
        for (int i = 0; i < n; i++)
            out[c][i] = buf[c][i].real() * (1.0-abs(1.0-i/(n/2.0)));
});
