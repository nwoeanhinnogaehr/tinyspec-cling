next_hop_ratio(1<<11, 0.5);
set_num_channels(0, 2);
connect(CLIENT_NAME, "system");
set_process_fn([&](WaveBuf&, WaveBuf &out, double t) {
    static FFTBuf buf;
    buf.resize(out.num_channels, out.size*2);
    int n = out.size;
    for (size_t c = 0; c < out.num_channels; c++) {
        for (int i = 1; i < n; i++) {
            int r = int(t*32)*n+i;
            int k = (r>>14^r>>17)%64+2;
            double mag=sin(pow(i,0.1)*k*16+
                    ((double)((r&r>>13)%k))
                    /(1+(double)((r/3&r/1023)%k)))
                /pow(i+1, 1.0)*32.0;
            buf[c][i]=cplx(mag,mag);
            buf[c][n*2-i] = conj(buf[c][i]);
        }
    }
    frft(buf, buf, -1);
    for (size_t c = 0; c < out.num_channels; c++)
        for (int i = 0; i < n; i++)
            out[c][i] = buf[c][i].real() * (1.0-abs(1.0-i/(n/2.0)));
});
