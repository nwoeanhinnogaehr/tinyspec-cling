// Rudimentary pulse width modulation (PWM)
// Synthesizes a sine pulse wave with width and frequency varying according to a bytebeat formula.
set_num_channels(0,1);
system("jack_connect " CLIENT_NAME ":out0 system:playback_1");
system("jack_connect " CLIENT_NAME ":out0 system:playback_2");

int it = 1<<16;

set_process_fn([&](WaveBuf& in, WaveBuf& out, int n, double t){
    for (int i=0; i<out.num_channels; i++)
        for (int j=0; j<out.size; j++)
            out[i][j] = sin(double(j)/out.size*M_PI*2-M_PI/2)/2+0.5;
    int k = ((it&it/65)^it/257)%512;
    double freq = k+40;
    int size = RATE/freq;
    double mod = fmod(k*k,128.0)/128.0;
    next_hop_samples(mod*size+1, size+1);
    it++;
});
