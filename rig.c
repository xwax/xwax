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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/poll.h>

#include "rig.h"
#include "track.h"

#define MAX_POLLFDS 3

/*
 * Main thread which handles input and output
 */

int rig_main(struct rig_t *rig, struct track_t track[], size_t ntrack)
{
    int r;
    char buf;
    size_t n;
    struct pollfd pt[MAX_POLLFDS], *pe;

    assert(ntrack <= MAX_POLLFDS);

    /* Register ourselves with the tracks we are looking after */

    for (n = 0; n < ntrack; n++)
        track[n].rig = rig;

    /* Create a pipe which will be used to wake us from other threads */

    if (pipe(rig->event) == -1) {
        perror("pipe");
        return -1;
    }

    rig->finished = false;

    while (!rig->finished) {
        pe = pt;

        /* ppoll() is not widely available, so use a pipe between this
         * thread and the outside. A single byte wakes up poll() to
         * inform us that new file descriptors need to be polled */

        pe->fd = rig->event[0];
        pe->revents = 0;
        pe->events = POLLIN | POLLHUP | POLLERR;
        pe++;

        /* Fetch file descriptors to monitor from each track */

        for (n = 0; n < ntrack; n++)
            pe += track_pollfd(&track[n], pe);

        r = poll(pt, pe - pt, -1);
        if (r == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                perror("poll");
                return -1;
            }
        }

        /* If we were awakened, take the top byte off the event pipe */

        if (pt[0].revents) {
            if (read(rig->event[0], &buf, 1) == -1) {
                perror("read");
                return -1;
            }
        }

        /* Do any reading and writing on all tracks */

        for (n = 0; n < ntrack; n++)
            track_handle(&track[n]);
    }

    return 0;
}

/*
 * Wake up the rig when a track controlled by it has changed
 */

int rig_awaken(struct rig_t *rig)
{
    /* Send a single byte down the event pipe to a waiting poll() */

    if (write(rig->event[1], "\0", 1) == -1) {
        perror("write");
        return -1;
    }

    return 0;
}
