#include <string>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

using namespace std;

int main(int argc, char** argv) {
    mkfifo("cmd", 0777);
    int fd = open("cmd", O_WRONLY);
    int c;
    const char *meta = "<<<>>>";
    write(fd, meta, 3);
    while(~(c = getchar())) {
        write(fd, &c, 1);
    }
    write(fd, meta+3, 3);
}

