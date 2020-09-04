# Files for running tinyspec-cling in a docker container

Using docker you can get tinyspec up and running with a minimal amount of effort and fuss.
It can be very difficult to get cling to compile natively on many systems (especially mac/windows)
but with this container you can run everything in a virtual ubuntu environment so
everything should work more reliably.

Currently this is untested on mac and windows though. Please let me know if it works or not!

## Usage

Install [Docker](https://www.docker.com) and [JACK](http://jackaudio.org) on your local machine, and start both the JACK server and the docker daemon.

Next, clone this repo. Then, from the hacks directory, do
```
$ ../docker/run.sh
```
The script needs to be run from the hacks directory in order to correctly find your hacks.
This will open a tmux session with vim on the top and tinyspec on the bottom.
You can execute a block of code from the editor by pressing F1.

You can optionally pass a filename to load and input/output port counts like so
```
$ ../docker/run.sh hacks/FILENAME.cpp NUM_INPUT_PORTS NUM_OUTPUT_PORTS
```
## Details

The `hacks` directory from your local tinyspec repo will be mounted in the container to allow
persisting your work. Be aware that any files saved outside of the hacks directory will not
be persisted!

The way that sound gets from your container out to your system is via the JACK `net` bridge.
The entire container will show up as a single JACK client on your host system.

Be aware that latency and reliability may be very bad. If this is a problem for you
consider installing tinyspec-cling natively as detailed in the top level readme.
