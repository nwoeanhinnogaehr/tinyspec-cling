# tinyspec-cling
A tiny C++ live-coded overlap-add (re)synthesizer for Linux, which uses cling to add REPL-like functionality for C++ code.
Things that you can do easily with tinyspec include:

 - create novel audio effects using FFT, phase vocoders and more, and control them with OSC
 - create synthesizers
 - granular synthesis
 - bytebeats (time and frequency domain)
 - control other software with OSC
 - use these synthesizers and effects with DAWs, other synthesizers, etc using JACK
 - do all of this in a live performance (with some caveats)

### What is an overlap-add (re)synthesizer?
You define a function which is called periodically to process a *frame* of audio.
The rate at which the function is called, as well as the length of a frame can be dynamically adjusted.
If multiple frames overlap in time, some of the input that they receive will be the same, and their outputs will be mixed.

For example, you could set the frame size to 1 and the hop size to 1 if you want to process a single sample at a time.
You can do granular synthesis of a wave at a specific frequency by e.g. setting the frame size to 100 and the hop to 440Hz.
Or, you could set the frame size to 1024 and the hop size to 256 (ratio 1/4) for standard use of a phase vocoder.
It's even possible to dynamically adjust the frame size and the hop according to some function. The amount of latency is automatically
adjusted by inserting silence to the input as needed.

Visually, in the following diagram the hop is 3 and the frame size is 7.
The audio output from the frames which overlap vertically will be added together before sending it out via JACK.
```
Time    ---------------------->
Frame 1 ~~~~~~~
Frame 2    ~~~~~~~
Frame 3       ~~~~~~~
Frame 4          ~~~~~~~
Frame 5             ~~~~~~~
Frame 6                ~~~~~~~
...
```

### Relation to tinyspec 
This is a fork of [tinyspec](https://github.com/nwoeanhinnogaehr/tinyspec), which was an experiment in trying to make the smallest useful FFT synthesizer.
This version adds live-coding support via [cling](https://root.cern.ch/cling), among many other handy features, and at this point it
barely resembles the original tinyspec project.
Cling is quite large and only officially supports a few distros (but probably works elsewhere),
so if you don't want to use it, you can still use this repo
to compile your hacks into a standalone executable, without live-coding support.

## Setup (easy)
The easiest way to get tinyspec up and running is by using Docker.

## Setup (advanced)
More adventurous users can install tinyspec-cling natively for improved latency and flexibility.

## Prereqs
Download an appropriate cling package from https://root.cern.ch/download/cling/
and extract it to a directory called `cling_bin`.
This official download page is occasionally inaccessible, so I also mirror a few builds on Google Drive [here](https://drive.google.com/drive/u/0/folders/1rrKMifdHjtKAVMU_HgxeNZyY4uBvwpvp).
(If you're using Arch, the Ubuntu 20.04 package gives some warnings but basically seems to work.)
This step is not necessary if you don't want to use the live-coding support.

You will also need 
[JACK Audio Connection Kit](http://www.jackaudio.org/) for audio output, and [FFTW3](http://www.fftw.org/) for synthesis.

On ubuntu you can install prereqs with
```
# apt-get install git clang libc6-dev binutils jackd2 libjack-jackd2-dev libfftw3-dev libzip-dev
```
JACK requires some configuration to get running, which can be quite significant depending on your setup.
There are many tutorials online to help you get it set up.

## Running without live-coding support
This is the easiest way to get started and doesn't require a functioning cling installation.
```
$ ./compile --no-cling hacks/readme.cpp
```
This compiles and runs an executable called `hacks/.readme.cpp.out`.
The `hacks` directory contains more examples you can try.

## Live-coding mode
Running the application in live coding mode starts a server which listens for code written to a named pipe.
You can start as many servers as you want and route their inputs/outputs using
JACK. To do this, see `hacks/connect.cpp` for examples of how you can make your program automatically connect inputs/outputs.
Alternatively you can use a JACK GUI such as [Catia](https://kx.studio/Applications:Catia).

First, compile tinyspec by running `./compile`.

Now, to run the server do:
```
$ ./tinyspec /tmp/ts-cmd
```
Here `/tmp/ts-cmd` is the pipe to create for executing code from.
If you run multiple instances they must have unique pipe files.
If all goes well you should see
```
Playing...
```
There may be some warnings from cling, which may or may not be safe to ignore.
If you think a cling warning is causing problems for you, please open an issue here.
You won't hear anything yet because we haven't sent the server any code to execute.

### Executing code
Currently the below is a bit clunky/user unfriendly. A fancy new WebUI is coming soon!

The `tools/send` program can be invoked on the command line to execute code on the server.
It takes the path to the server's command pipe as an argument.

For example try
```
$ ./send /tmp/ts-cmd
cout << "Hello World!" << endl;
^D
```

It should be easy to set up your editor to invoke `send` with the current selection or block.
For example, the following vim commands map F2 to execute the current paragraph of code.
```
:map <F2> mcVip::w<Home>silent <End> !tools/send /tmp/ts-cmd<CR>`c
:imap <F2> <Esc>mcVip::w<Home>silent <End> !tools/send /tmp/ts-cmd<CR>`ca
```
Check out `docs/vscode.txt` for a basic method of using tinyspec with vscode.

If you configure another editor, please contribute it here so other users can benefit!

### API

Currently this project is lacking API documentation, but it has a number of examples
in the `hacks` directory which you can study. API docs coming soon!

## Project structure

`rtinc` stores header files accessible by user code.
`rtlib` stores the respective implementations. Both directories contain a `root.hpp` file which is included by default.

