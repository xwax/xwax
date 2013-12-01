#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/wait.h>

#include "external.h"

int main(int argc, char *argv[])
{
    int fd, status;
    unsigned cycles, strings;
    pid_t pid, p;
    struct pollfd pe;
    struct rb rb;

    pid = fork_pipe_nb(&fd, "/usr/bin/find", "find", NULL);
    if (pid == -1)
        return -1;

    pe.fd = fd;
    pe.revents = POLLIN;

    rb_reset(&rb);

    cycles = 0;
    strings = 0;

    for (;;) {
        char *s;
        ssize_t z;

        cycles++;

        if (poll(&pe, 1, -1) == -1) {
            perror("poll");
            return -1;
        }

        z = get_line(fd, &rb, &s);
        if (z == -1) {
            if (errno == EAGAIN)
                continue;
            break;
        }
        if (z == 0)
            break;

        strings++;
        fprintf(stderr, "(%u, %u) %s\n", cycles, strings, s);
        free(s);
    }

    fprintf(stderr, "%u cycles, %u strings\n", cycles, strings);

    if (close(fd) == -1)
        abort();

    p = wait(&status);
    assert(p == pid);

    return 0;
}
