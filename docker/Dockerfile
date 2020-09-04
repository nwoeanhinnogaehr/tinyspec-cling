FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y git clang libc6-dev binutils jackd2 libjack-jackd2-dev libfftw3-dev libzip-dev vim tmux curl netcat && \
    useradd -ms /bin/bash ts && \
    gpasswd -a ts audio
USER ts
WORKDIR /home/ts
COPY data/get_cling.sh /home/ts/
RUN git clone https://github.com/nwoeanhinnogaehr/tinyspec-cling && \
    cd tinyspec-cling && \
    ../get_cling.sh && \
    ./compile
COPY data/startup.sh data/.vimrc data/.tmux.conf /home/ts/

ENTRYPOINT [ "/home/ts/startup.sh" ]
