// OSC demo.
//
// You can use OSC to send commands to external programs
// like synthesizers or VJ software.
//
// To use this, install and start SuperDirt first.
// See here for instructions: https://github.com/musikinformatik/SuperDirt
// The simple setup will suffice.

// When superdirt is running, execute this:
osc_init("localhost", 57120); // superdirt runs on port 57120.

// Then execute this and you should hear some sounds!
// These sounds are not produced by tinyspec directly but
// are coming from superdirt.
extern "C" void synth_main(cplx*buf[2],int n,double t){
    int s = t*16; // note counter
    // First arg is the time it should be triggered at.
    // Second is the OSC path.
    // Everything else is OSC params, which can be int32, int64, float, double, or string.
    osc_send(t+0.5, "/play2", "sound", "blip", "speed", ((s&s/8)%16)/8.0+0.5);
    set_next_size(0); // 0 disables synthesis
    set_hop_hz(16); // set rate
}
