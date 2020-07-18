#!/bin/bash

# start jack net slave
echo "starting JACK..."
ulimit -l 65536 # workaround for bus error when starting jackd
jackd -r -d net -a 127.0.0.1 -C ${2:-2} -P ${3:-2} -n tinyspec &
jack_wait -w

filename=${1:-hacks/readme.cpp}

cd tinyspec-cling
tmux new -d -s tinyspec
tmux send-keys "export TERM=xterm-256color" C-m # if your Fx keys don't work try changing this
tmux send-keys "vim $filename" C-m

tmux split-window -v
tmux send-keys "./tinyspec cmd1" C-m
tmux select-pane -t 0
tmux resize-pane -D

tmux a
bash
