// Rudimentary pulse width modulation (PWM)
// Synthesizes a pulse wave with width varying by a sine LFO.
// Achieves the effect by using a fixed hop size and varying the window size.

set_num_channels(0,1);
connect(CLIENT_NAME, "system");
set_process_fn([&](WaveBuf&, WaveBuf& out, double t){
    for (size_t i=0; i<out.num_channels; i++)
        for (size_t j=0; j<out.size; j++)
            out[i][j] = 1;
    double freq = 45;
    double size = RATE/freq;
    double mod = sin(t)*0.5+0.5;
    next_hop_samples(mod*size+1, size+1);
});
