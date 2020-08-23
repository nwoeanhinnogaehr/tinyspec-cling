next_hop_ratio(1<<11, 0.5);
set_num_channels(0, 2);
connect(CLIENT_NAME, "system");
set_process_fn([&](WaveBuf&, WaveBuf &out, int, double t) {
    static FFTBuf buf;
    static Buf<double> phase_sum;
    phase_sum.resize(out.num_channels, out.size);
    buf.resize(out.num_channels, out.size*2);
    int n = out.size;
    for (size_t c = 0; c < out.num_channels; c++) {
        for (int i = 1; i < n; i++) {
            buf[c][i] = 0;
            int r = int(t*1024)+(1<<20);
            for (int nn = 1; nn <= 16; nn++) {
                int f = int(pow(i/16+r/255&r/63^r/1023,1+2/double(nn+c)))%256+1;
                int h = i/f;
                int k = i%f;
                phase_sum[c][i] += 2*M_PI*i;
                buf[c][i]+=polar(
                    (double)((k==nn%f)*(h&1?1:-1))/pow(max(1,i-f+1), 0.6),
                    phase_sum[c][i]+c*M_PI/2);
            }
            buf[c][n*2-i] = conj(buf[c][i]);
        }
    }
    frft(buf, buf, -1);
    for (size_t c = 0; c < out.num_channels; c++)
        for (int i = 0; i < n; i++)
            out[c][i] = buf[c][i].real() * (1.0-abs(1.0-i/(n/2.0)));
});
