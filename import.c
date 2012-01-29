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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "external.h"
#include "import.h"
#include "track.h"

/*
 * Start an import operation, launching the external decoder process
 * and feeding data into the given track
 *
 * Post: import is valid until import_stop()
 */

int import_start(struct import *im, struct track *track,
                 const char *cmd, const char *path)
{
    int fd;
    char rate[16];
    pid_t pid;

    fprintf(stderr, "Importing '%s'...\n", path);

    sprintf(rate, "%d", track->rate);

    pid = fork_pipe_nb(&fd, cmd, "import", path, rate, NULL);
    if (pid == -1)
        return -1;

    im->pid = pid;
    im->fd = fd;
    im->track = track;

    return 0;
}

/*
 * Synchronise with the import process and complete it
 *
 * Pre: import is valid
 * Post: import is invalid
 */

void import_stop(const struct import *im)
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

void import_pollfd(struct import *im, struct pollfd *pe)
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

static int read_from_pipe(int fd, struct track *tr)
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

int import_handle(struct import *im)
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

void import_terminate(struct import *im)
{
    assert(im->pid != 0);

    if (kill(im->pid, SIGTERM) == -1)
        abort();
}
