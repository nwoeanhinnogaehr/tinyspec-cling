#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

int main(int argc, char** argv) {
    const char *filename = argc > 1 ? argv[1] : "cmd";
    int fd = open(filename, O_WRONLY);
    int c;
    const char *meta = "<<<>>>";
    write(fd, meta, 3);
    while(~(c = getchar())) {
        write(fd, &c, 1);
    }
    write(fd, meta+3, 3);
}

