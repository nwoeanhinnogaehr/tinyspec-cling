#!/bin/bash

docker info > /dev/null 2>&1
if [[ $? -ne 0 ]]; then
    echo "Docker is not running"
    exit 1
fi

# load jack network module on host
jack_load netmanager

# start container
docker pull nwoeanhinnogaehr/tinyspec
netdate=$(date -d $(docker inspect -f '{{ .Created }}' nwoeanhinnogaehr/tinyspec) +%s)
localdate=$(date -d $(docker inspect -f '{{ .Created }}' tinyspec) +%s 2>/dev/null)
if [ ${localdate:-0} -ge $netdate ];
then
    echo "Using local build"
    container=tinyspec
else
    echo "Using docker hub build"
    container=nwoeanhinnogaehr/tinyspec
fi
docker run -it --detach --user=ts --name=tinyspec --network=host --rm --mount type=bind,source=$(pwd)/$(dirname $0)/../hacks,target=/home/ts/tinyspec-cling/hacks $container $@
echo "waiting for jack..."
nc -l -p 44100

# connect slave ports to speakers/mic
jack_connect tinyspec:from_slave_1 system:playback_1
jack_connect tinyspec:from_slave_2 system:playback_2
jack_connect tinyspec:to_slave_1 system:capture_1
jack_connect tinyspec:to_slave_2 system:capture_2

docker attach tinyspec
