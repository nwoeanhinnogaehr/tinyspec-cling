A tiny C++ live-coded frequency domain synthesizer for linux.

This is a fork of [tinyspec](https://github.com/nwoeanhinnogaehr/tinyspec) which adds live-coding support
via [cling](https://root.cern.ch/cling).

## Install
Download an appropriate cling package from https://root.cern.ch/download/cling/
and extract it to a directory called `cling_bin`.

If you're using Arch, the Ubuntu 16 package gives some warnings but basically seems to work.

Then, compile tinyspec with `./compile`.

## Run

To run the server do:
```
$ ./play
```
If all goes well you should see
```
Playing raw data 'stdin' : Float 32 bit Little Endian, Rate 44100 Hz, Stereo
```
There may be some warnings from cling afterwards, which may or may not be safe to ignore!
If your `aplay` is not working, you might want edit the play script to send the output to a different audio player.
`sox` can be used for format conversion.

## Editor setup
The `send` program can be invoked on the command line to execute code on the server.
It must be invoked with the root of this repo as the working directory.
It's probably easy to set up your editor to invoke `send` with the current selection or block.
For example, the following vim commands map F2 to execute the current paragraph of code.
```
:map <F2> mcVip::w<Home>silent <End> !./send<CR>`c
:imap <F2> <Esc>mcVip::w<Home>silent <End> !./send<CR>`ca
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
extern "C" int synth_main(cplx *buf[2], int n, double t) {
    // Loop over frequency bins. Starting at 1 skips the DC offset.
    for (int i = 1; i < n; i++) {
        buf[0][i] = buf[1][i] = // Set both left and right channels
            sin(t+pow(i,1+sin(t)*0.6))/2 // Fun little formula
            /i; // Scale magnitude by bin number to prevent loud high frequency noises.
    }
    return 10; // Return value is FFT size for next frame.
}
```
See the hacks directory for some other examples.
Even more examples---which may require adaptation for this fork---are available as part of [tinyspec](https://github.com/nwoeanhinnogaehr/tinyspec).
