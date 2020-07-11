#!/bin/bash

# start jack net slave
echo "starting JACK..."
ulimit -l 65536 # workaround for bus error when starting jackd
jackd -r -d net -a 127.0.0.1 -n tinyspec &
jack_wait -w

cd tinyspec-cling
tmux new -d -s tinyspec
tmux send-keys "export TERM=xterm-256color" C-m # if your Fx keys don't work try changing this
tmux send-keys "vim hacks/readme.cpp" C-m

tmux split-window -v
tmux send-keys "./tinyspec cmd1" C-m
tmux select-pane -t 0
tmux resize-pane -D

# connect default ports
jack_connect tinyspec_cmd1:out0 system:playback_1
jack_connect tinyspec_cmd1:out1 system:playback_2
jack_connect system:capture_1 tinyspec_cmd1:in0
jack_connect system:capture_2 tinyspec_cmd1:in1

tmux a
bash
