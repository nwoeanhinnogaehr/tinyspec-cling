#!/bin/bash

# start jack net slave
jackd -r -d net -a 127.0.0.1 -C 2 -P 2 -i 0 -o 0 -n tinyspec &

cd tinyspec-cling

echo "compiling tinyspec..."
./compile

tmux new -d -s tinyspec
tmux send-keys "export TERM=xterm-256color" C-m
tmux send-keys "vim hacks/readme.cpp" C-m

echo "starting tinyspec..."
tmux split-window -v
tmux send-keys "./tinyspec cmd1" C-m
tmux select-pane -t 0
sleep 3

echo "connecting ports..."
jack_connect tinyspec_cmd1:out0 system:playback_1
jack_connect tinyspec_cmd1:out1 system:playback_2
jack_connect system:capture_1 tinyspec_cmd1:in0
jack_connect system:capture_2 tinyspec_cmd1:in1

tmux a
bash
