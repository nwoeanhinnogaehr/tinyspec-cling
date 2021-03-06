// Rudimentary pulse width modulation (PWM)
// Synthesizes a sine pulse wave with width and frequency varying according to a bytebeat formula.
set_num_channels(0,1);
connect(CLIENT_NAME, "system");
set_process_fn([&](WaveBuf&, WaveBuf& out, double){
    static int it = 1<<16;
    for (size_t i=0; i<out.num_channels; i++)
        for (size_t j=0; j<out.size; j++)
            out[i][j] = sin(double(j)/out.size*M_PI*2-M_PI/2)/2+0.5;
    int k = ((it&it/65)^it/257)%512;
    double freq = k+40;
    double size = RATE/freq;
    double mod = fmod(k*k,128.0)/128.0;
    next_hop_samples(mod*size+1, size+1);
    it++;
});
