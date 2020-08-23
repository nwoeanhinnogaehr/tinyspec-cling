// Demo of excited delay line filtered with FFT
// Try feeding it with:
// $ watch oscsend localhost 9999 /play d 110

set_num_channels(0, 2);
connect(CLIENT_NAME, "system");
set_process_fn([&](WaveBuf &, WaveBuf &out, double t) {
    static int trigger_time = -99999; //prevent trigger at t=0
    static double freq = 220;
    static int head = 0;
    static FFTBuf f;
    static WaveBuf buf;
    for (auto msg : osc_recv(9999, t, "/play")) {
        osc_get(*msg, freq);
        trigger_time = head;
    }
    int bufsize = (size_t)(RATE/freq);
    buf.resize(2, bufsize);
    for (int c = 0; c < 2; c++) {
        double samp;
        if (head-trigger_time < bufsize)
            samp = sin(rand()*3434.3523); //lazy float rand
        else
            samp = buf[c][(head+1)%bufsize];
        out[c][0] = buf[c][head%bufsize] = samp;
    }
    if (head%bufsize == 0) {
        // Perform fitering
        f.fill_from(buf);
        frft(f,f,1);
        for (int c = 0; c < 2; c++) {
            for (int i = 0; i < bufsize/2; i++) {
                // Can get lots of wacky effects by changing the filter here
                f[c][i] = polar(abs(f[c][i])/pow(i+1,0.05), arg(f[c][i]));
            }
        }
        frft(f,f,-1);
        buf.fill_from(f);
    }
    head++;
    next_hop_samples(1,1);
});
