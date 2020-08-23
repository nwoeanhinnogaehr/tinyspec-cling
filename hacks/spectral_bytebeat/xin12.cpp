next_hop_ratio(1<<11, 0.5);
set_num_channels(0, 2);
connect(CLIENT_NAME, "system");
set_process_fn([&](WaveBuf&, WaveBuf &out, int, double t) {
    static FFTBuf buf;
    static Buf<double> sum;
    sum.resize(out.num_channels, out.size);
    buf.resize(out.num_channels, out.size*2);
    int n = out.size;
    for (size_t c = 0; c < out.num_channels; c++) {
        for (int i = 1; i < n; i++) {
            int factor = 600;
            int r = (int)(t*8)*n/2+i;
            sum[c][i] += fmod(r,1+(double)r/(1+(((r/factor)&(r/101/factor)|r/247/factor)&(i+c))))/(n-i);
            buf[c][i] = (fmod(sum[c][i],1))/pow(i+1, 1.0)*32.0;
            sum[c][i] *= 0.1+sin(i/(double)n*M_PI*2.0+t)*0.1;
            buf[c][n*2-i] = conj(buf[c][i]);
        }
    }
    frft(buf, buf, -1);
    for (size_t c = 0; c < out.num_channels; c++)
        for (int i = 0; i < n; i++)
            out[c][i] = buf[c][i].real() * (1.0-abs(1.0-i/(n/2.0)));
});
