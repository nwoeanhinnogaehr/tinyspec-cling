// OSC demo.
//
// You can use OSC to send commands to external programs
// like synthesizers or VJ software.
//
// To use this, install and start SuperDirt first.
// See here for instructions: https://github.com/musikinformatik/SuperDirt
// The simple setup will suffice.

// Then execute this and you should hear some sounds!
// These sounds are not produced by tinyspec directly but
// are coming from superdirt.
extern "C" void process(cplx **in, int nch_in, cplx **out, int nch_out, int n, double t){
    int s = t*16; // note counter
    // First two args are the OSC server address and port (UDP)
    // Third arg is the time it should be triggered at.
    // Fourth is the OSC path.
    // Everything following is OSC params, which can be int32, int64, float, double, or string.
    osc_send("localhost", 57120, t+0.5, "/play2", "sound", "blip", "speed", ((s&s/8)%16)/8.0+0.5);
    next_hop_hz(0,16); // set rate
}
