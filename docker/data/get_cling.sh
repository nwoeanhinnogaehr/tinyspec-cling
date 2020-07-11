#!/bin/bash

# https://stackoverflow.com/questions/25010369/wget-curl-large-file-from-google-drive
fileid="1oX-muhinB7Cwa6i9d32G1G9_r396InbL"
filename="cling_bin"
fileext=".tar.bz2"
curl -c /tmp/cookie -s -L "https://drive.google.com/uc?export=download&id=${fileid}" > /dev/null
curl -Lb /tmp/cookie "https://drive.google.com/uc?export=download&confirm=`awk '/download/ {print $NF}' /tmp/cookie`&id=${fileid}" -o cling_bin.tar.bz2
tar xvf cling_bin.tar.bz2
rm cling_bin.tar.bz2
mv cling_2020-04-02_ubuntu18 cling_bin
