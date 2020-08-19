connect(CLIENT_NAME, "system");
PhaseVocoder pv;

set_process_fn([&](WaveBuf &in, WaveBuf &out, int n, double t) {
    pv.analyze(in);
    pv.shift([](double x){ return x*2; });
    pv.synthesize(out);
    next_hop_ratio(2048,0.25);
});

pv.reset();

