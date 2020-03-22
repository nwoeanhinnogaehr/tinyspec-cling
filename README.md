A tiny C++ live-coded overlap-add (re)synthesizer for Linux.
Things that you can do easily with tinyspec include:

 - create novel audio effects using FFT, phase vocoders and more, and control them with OSC
 - create synthesizers
 - granular synthesis
 - bytebeats (time and frequency domain)
 - control other software with OSC
 - use these synthesizers and effects with DAWs, other synthesizers, etc using JACK
 - do all of this in a live performance (with some caveats)

This is a fork of [tinyspec](https://github.com/nwoeanhinnogaehr/tinyspec), which was an experiment in trying to make the smallest useful FFT synthesizer.
This version adds live-coding support via [cling](https://root.cern.ch/cling), among many other handy features.
Cling is quite large and only officially supports Ubuntu (but probably works elsewhere),
so if you don't want to use it, you can still use this
to compile your code into a standalone executable, without live-coding support.

## Install
Download an appropriate cling package from https://root.cern.ch/download/cling/
and extract it to a directory called `cling_bin`.
(If you're using Arch, the Ubuntu 18 package gives some warnings but basically seems to work.)
This step is not necessary if you only want to use standalone mode (see below).

[JACK Audio Connection Kit](http://www.jackaudio.org/) is used for audio output, and [FFTW3](http://www.fftw.org/) is required for synthesis.

## Standalone mode
The easiest way to get started is using standalone mode. For example, try
```
$ ./standalone hacks/readme.cpp
```
Then in a separate terminal, connect the program to your speakers by running
```
$ jack_connect tinyspec_cmd:out0 system:playback_1
$ jack_connect tinyspec_cmd:out1 system:playback_2
```
See `hacks/connect.cpp` for examples of how you can make your program automatically connect inputs/outputs.
You can also do this through a JACK GUI such as [Catia](https://kx.studio/Applications:Catia).

## Live-coding mode
Running the application in live coding mode starts a server which listens for code written to a named pipe.
You can start as many servers as you want and route their inputs/outputs using
JACK. To do this, you can use a JACK GUI such as [Catia](https://kx.studio/Applications:Catia).
Alternatively see `hacks/connect.cpp` for examples of how you can make your program automatically connect inputs/outputs.

Compile tinyspec by running `./compile`.

To run the server do:
```
$ ./tinyspec /tmp/ts-example
```
Where `/tmp/ts-example` is the pipe to create for executing code from.
If you run multiple instances they must have unique pipe files.
If all goes well you should see
```
Playing...
```
There may be some warnings from cling afterwards, which may or may not be safe to ignore! (cling is very buggy)
You won't hear anything yet for two reasons: the application is not
connected to your speakers (do this through JACK) and we haven't sent it any code yet.

### Editor setup
The `send` program can be invoked on the command line to execute code on the server.
It takes the path to the server's command pipe as an argument.

For example try
```
$ ./send /tmp/ts-example
cout << "Hello World!" << endl;
^D
```

It's probably easy to set up your editor to invoke `send` with the current selection or block.
For example, the following vim commands map F2 to execute the current paragraph of code.
```
:map <F2> mcVip::w<Home>silent <End> !./send /tmp/ts-example<CR>`c
:imap <F2> <Esc>mcVip::w<Home>silent <End> !./send /tmp/ts-example<CR>`ca
```
Check out `doc/vscode.txt` for a basic method of using tinyspec with vscode.

If you configure another editor, please contribute it here so other users can benefit!

### Usage

For example, try executing the following code. You can also find this file as hacks/readme.cpp

```C++
// Called periodically to fill up a new buffer.
// in and out are audio sample buffers
// n is the number of samples in the frame
// t is the time in seconds since the beginning of playback.
set_process_fn([&](WaveBuf& in, WaveBuf& out, int n, double t){
    FFTBuf fft;
    fft.resize(out.num_channels, n);
    // Loop over frequency bins. Starting at 1 skips the DC offset.
    for (int c = 0; c < 2; c++) {
        for (int i = 1; i < n; i++) {
            cplx x = sin(i*pow(1.0/(i+1), 1.0+sin(t*i*M_PI/8+c)*0.5))*25 // Some random formula
                /pow(i,0.9); // Scale magnitude to prevent loud high frequency noises.
            fft[c][i] = x; // Fill output buffer
        }
    }
    frft(fft, fft, -1.0); // Perform in-place inverse FFT
    out.fill_from(fft); // Copy real part to output
    window_hann(out); // Apply Hann window
    next_hop_ratio(4096); // Set FFT size for the next frame
});
```

Here, `cplx` is just an alias for `std::complex<double>`.

Note the call to `next_hop_ratio` at the end---this is one of 3 built in functions that can be used to
manipulate the parameters of the FFT synthesizer.

`void next_hop_ratio(uint32_t n, double hop_ratio=0.25)`:
Set the FFT size for the next frame to `n` and advance the output by `n*hop_ratio` samples.

`void next_hop_samples(uint32_t n, uint32_t hop)`:
Set the FFT size for the next frame to `n` and advance the output by `hop` samples.

`void next_hop_hz(uint32_t n, double hz)`:
Set the FFT size for the next frame to `n` and advance the output such that the rate of
frame generation is `hz` Hz. That is, advance by `RATE/hz` samples.

You can also call `void set_num_channels(size_t in, size_t out);`
to set the number of input and output channels to use.

Finally, if you notice a lot of latency, you can force
the synthesizer to discard buffered data and skip to the present moment
by calling `void skip_to_now();`

See the hacks directory for some other examples.
Even more examples---which require adaptation for this fork---are available as part of [tinyspec](https://github.com/nwoeanhinnogaehr/tinyspec).

## OSC support

There is currently experimental support for sending/receiving Open Sound Control messages.
See `hacks/osc.cpp` and `hacks/tidal.cpp` for details.
