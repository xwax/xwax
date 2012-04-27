/*
 * Copyright (C) 2012 Mark Hills <mark@pogo.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#define _BSD_SOURCE /* vfork() */
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "external.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

/*
 * Fork a child process, attaching stdout to the given pipe
 *
 * Return: -1 on error, or pid on success
 * Post: on success, *fd is file handle for reading
 */

static pid_t do_fork(int pp[2], const char *path, char *argv[])
{
    pid_t pid;

    pid = vfork();
    if (pid == -1) {
        perror("vfork");
        return -1;
    }

    if (pid == 0) { /* child */
        if (close(pp[0]) != 0)
            abort();

        if (dup2(pp[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            _exit(EXIT_FAILURE); /* vfork() was used */
        }

        if (close(pp[1]) != 0)
            abort();

        if (execv(path, argv) == -1) {
            perror(path);
            _exit(EXIT_FAILURE); /* vfork() was used */
        }

        abort(); /* execv() does not return */
    }

    if (close(pp[1]) != 0)
        abort();

    return pid;
}

/*
 * Wrapper on do_fork which uses va_list
 *
 * The caller passes in the pipe for use, rather us handing one
 * back. This is because if the caller wishes to have a non-blocking
 * pipe, then the cleanup is messy if the process has already been
 * forked.
 */

static pid_t vext(int pp[2], const char *path, char *arg, va_list ap)
{
    char *args[16];
    size_t n;

    args[0] = arg;
    n = 1;

    /* Convert to an array; there's no va_list variant of exec() */

    for (;;) {
        char *x;

        x = va_arg(ap, char*);
        assert(n < ARRAY_SIZE(args));
        args[n++] = x;

        if (x == NULL)
            break;
    }

    return do_fork(pp, path, args);
}

/*
 * Fork a child process with stdout connected to this process
 * via a pipe
 *
 * Return: PID on success, otherwise -1
 * Post: on success, *fd is file descriptor for reading
 */

pid_t fork_pipe(int *fd, const char *path, char *arg, ...)
{
    int pp[2];
    pid_t r;
    va_list va;

    if (pipe(pp) == -1) {
        perror("pipe");
        return -1;
    }

    va_start(va, arg);
    r = vext(pp, path, arg, va);
    va_end(va);

    if (r == -1) {
        if (close(pp[0]) != 0)
            abort();
        if (close(pp[1]) != 0)
            abort();
    }

    *fd = pp[0];
    return r;
}

/*
 * Make the given file descriptor non-blocking
 *
 * Return: 0 on success, otherwise -1
 * Post: if 0 is returned, file descriptor is non-blocking
 */

static int make_non_blocking(int fd)
{
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        return -1;
    }

    return 0;
}

/*
 * Fork a child process with stdout connected to this process
 * via a non-blocking pipe
 *
 * Return: PID on success, otherwise -1
 * Post: on success, *fd is non-blocking file descriptor for reading
 */

pid_t fork_pipe_nb(int *fd, const char *path, char *arg, ...)
{
    int pp[2];
    pid_t r;
    va_list va;

    if (pipe(pp) == -1) {
        perror("pipe");
        return -1;
    }

    if (make_non_blocking(pp[0]) == -1)
        goto fail;

    va_start(va, arg);
    r = vext(pp, path, arg, va);
    va_end(va);

    assert(r != 0);
    if (r < 0)
        goto fail;

    *fd = pp[0];
    return r;

fail:
    if (close(pp[0]) != 0)
        abort();
    if (close(pp[1]) != 0)
        abort();

    return -1;
}
