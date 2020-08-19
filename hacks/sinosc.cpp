set_num_channels(0,1);
connect(CLIENT_NAME, "system");
set_process_fn([&](WaveBuf& in, WaveBuf& out, int n, double t){
    out[0][0] = sin(t*2*M_PI*440);
    next_hop_samples(1, 1);
});
