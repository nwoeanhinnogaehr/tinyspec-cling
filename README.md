A tiny C++ live-coded frequency domain synthesizer for linux.

This is a fork of [tinyspec](https://github.com/nwoeanhinnogaehr/tinyspec) which adds live-coding support
via [cling](https://root.cern.ch/cling).

## Install
Download an appropriate cling package from https://root.cern.ch/download/cling/
and extract it to a directory called `cling_bin`.
(If you're using Arch, the Ubuntu 16 package gives some warnings but basically seems to work.)

[JACK Audio Connection Kit](http://www.jackaudio.org/) is used for audio output, and [FFTW3](http://www.fftw.org/) is required for synthesis.
After both are installed, compile tinyspec by cloning this repository and running `./compile`.

## Architecture

Running the application starts a server which listens for code written to a named pipe.
You can start as many servers as you want and route their inputs/outputs using
JACK. I recommend using a JACK GUI such as [Catia](https://kx.studio/Applications:Catia).

## Run

To run the server do:
```
$ ./tinyspec /tmp/ts-example
```
Where `/tmp/ts-example` is the pipe to create for executing code from.
If all goes well you should see
```
Playing...
```
There may be some warnings from cling afterwards, which may or may not be safe to ignore!
You won't hear anything yet, so read on to figure out how to use it.

## Editor setup
The `send` program can be invoked on the command line to execute code on the server.
It takes the path to the server's command pipe as an argument.

For example try
```
$ ./send /tmp/ts-example
cout << "Hello World!" << endl;

```

It's probably easy to set up your editor to invoke `send` with the current selection or block.
For example, the following vim commands map F2 to execute the current paragraph of code.
```
:map <F2> mcVip::w<Home>silent <End> !./send /tmp/ts-example<CR>`c
:imap <F2> <Esc>mcVip::w<Home>silent <End> !./send /tmp/ts-example<CR>`ca
```
If you configure another editor, please contribute it here so other users can benefit!

## Usage

For example, try executing the following code.

```C++
// Called periodically to fill up a new buffer.
// buf[0] is the left channel, and buf[1] is the right channel.
// You should fill both elements of buf with n complex numbers.
// t is the time in seconds since the beginning of playback.
// The 0th bin is the DC offset. Usually this should be left at a value of 0.
// The 1st bin is the lowest frequency, and the n-1th is the highest frequency.
// buf is zeroed out before this function is called.
extern "C" void process(cplx **in, int nch_in, cplx **out, int nch_out, int n, double t){
    // Loop over frequency bins. Starting at 1 skips the DC offset.
    for (int i = 1; i < n; i++) {
        cplx x = sin(t+pow(i,1+sin(t)*0.6))/2 // Fun little formula
            /i; // Scale magnitude by bin number to prevent loud high frequency noises.
        for (int c = 0; c < nch_out; c++)
            out[c][i] = x; // Fill output buffer
    }
    next_hop_ratio(1<<10); // Set FFT size for the next frame
    // i.e. the value of n in the next call will be 2^9
}
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

See the hacks directory for some other examples.
Even more examples---which may require adaptation for this fork---are available as part of [tinyspec](https://github.com/nwoeanhinnogaehr/tinyspec).

## OSC support

There is currently experimental support for sending/receiving Open Sound Control messages.
See `hacks/osc.cpp` for details.
