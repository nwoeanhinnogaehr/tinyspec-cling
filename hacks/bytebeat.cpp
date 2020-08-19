// Simple bytebeat synth achieved by setting both frame size and hop to 1 sample.
set_num_channels(0,1);
connect(CLIENT_NAME, "system");

set_process_fn([&](WaveBuf&, WaveBuf& out, int, double ts){
    double t = ts*2000;
    int y = t;
    int s = int(fmod(t,(1+(t/(1.0+(y&(y>>9^y>>11)))))));
    out[0][0] = s%256/128.0-1;
    next_hop_samples(1,1);
});
