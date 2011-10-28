/*
 * Copyright (C) 2011 Mark Hills <mark@pogo.org.uk>
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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "import.h"
#include "track.h"

/*
 * Start an import operation, launching the external decoder process
 * and feeding data into the given track
 *
 * Post: import is valid until import_stop()
 */

int import_start(struct import_t *im, struct track_t *track,
                 const char *cmd, const char *path)
{
    int pstdout[2];
    char rate[16];
    pid_t pid;

    sprintf(rate, "%d", track->rate);

    if (pipe(pstdout) == -1) {
        perror("pipe");
        return -1;
    }

    if (fcntl(pstdout[0], F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        return -1;
    }

    pid = vfork();

    if (pid == -1) {
        perror("vfork");
        return -1;

    } else if (pid == 0) { /* child */

        /* Reconnect stdout to this process, leave stderr to terminal */

        if (close(pstdout[0]) != 0) {
            perror("close");
            abort();
        }

        if (dup2(pstdout[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            _exit(EXIT_FAILURE); /* vfork() was used */
        }

        if (close(pstdout[1]) != 0) {
            perror("close");
            abort();
        }

        if (execl(cmd, "import", path, rate, NULL) == -1) {
            perror("execl");
            fprintf(stderr, "Failed to launch importer %s\n", cmd);
            _exit(EXIT_FAILURE); /* vfork() was used */
        }

        abort(); /* execl() never returns */
    }

    if (close(pstdout[1]) != 0) {
        perror("close");
        abort();
    }

    im->pid = pid;
    im->fd = pstdout[0];
    im->track = track;

    return 0;

}

/*
 * Synchronise with the import process and complete it
 *
 * Pre: import is valid
 * Post: import is invalid
 */

void import_stop(const struct import_t *im)
{
    int status;

    assert(im->pid != 0);

    if (close(im->fd) == -1)
        abort();

    if (waitpid(im->pid, &status, 0) == -1)
        abort();

    if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
        fputs("Track import completed.\n", stderr);
    } else {
        fputs("Track import did not complete successfully.\n", stderr);
    }
}

/*
 * Assign the poll entry for the import operation, used to wake the
 * rig when data is available
 *
 * Post: pe contains the poll table entry
 */

void import_pollfd(struct import_t *im, struct pollfd *pe)
{
    assert(im->pid != 0);

    pe->fd = im->fd;
    pe->events = POLLIN;

    im->pe = pe;
}

/*
 * Read the next block of data from the file handle into the track's
 * PCM data
 *
 * Return: -1 on completion, otherwise zero
 */

static int read_from_pipe(int fd, struct track_t *tr)
{
    for (;;) {
        void *pcm;
        size_t len;
        ssize_t z;

        pcm = track_access_pcm(tr, &len);
        if (pcm == NULL)
            return -1;

        z = read(fd, pcm, len);
        if (z == -1) {
            if (errno == EAGAIN) {
                return 0;
            } else {
                perror("read");
                return -1;
            }
        }

        if (z == 0) /* EOF */
            break;

        track_commit(tr, z);
    }

    return -1; /* completion without error */
}

/*
 * Handle any available data for this import operation
 *
 * Return: -1 on completion, otherwise zero
 */

int import_handle(struct import_t *im)
{
    assert(im->pid != 0);
    assert(im->pe != NULL);

    if (im->pe->revents == 0)
        return 0;
    else
        return read_from_pipe(im->fd, im->track);
}

/*
 * Request premature termination of an import operation
 */

void import_terminate(struct import_t *im)
{
    assert(im->pid != 0);

    if (kill(im->pid, SIGTERM) == -1)
        abort();
}
