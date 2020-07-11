#!/bin/bash

# load jack network module on host
jack_load netmanager

# start container
docker pull nwoeanhinnogaehr/tinyspec
docker run -it --detach --user=ts --name=tinyspec --network=host --rm nwoeanhinnogaehr/tinyspec
echo "waiting for jack..."
sleep 3

# connect slave ports to speakers/mic
jack_connect tinyspec:from_slave_1 system:playback_1
jack_connect tinyspec:from_slave_2 system:playback_2
jack_connect tinyspec:to_slave_1 system:capture_1
jack_connect tinyspec:to_slave_2 system:capture_2

docker attach tinyspec
