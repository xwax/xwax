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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/poll.h>

#include "rig.h"
#include "track.h"

#define MAX_POLLFDS 3

#define EVENT_WAKE 0
#define EVENT_QUIT 1

int rig_init(struct rig_t *rig)
{
    /* Create a pipe which will be used to wake us from other threads */

    if (pipe(rig->event) == -1) {
        perror("pipe");
        return -1;
    }

    if (fcntl(rig->event[0], F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        if (close(rig->event[1]) == -1)
            abort();
        if (close(rig->event[0]) == -1)
            abort();
        return -1;
    }

    rig->ntrack = 0;

    return 0;
}

void rig_clear(struct rig_t *rig)
{
    if (close(rig->event[0]) == -1)
        abort();
    if (close(rig->event[1]) == -1)
        abort();
}

/*
 * Add a track to be monitored
 *
 * This function is not thread or signal safe to rig_main() as
 * is only used as part of initialisation.
 *
 * Pre: less than MAX_TRACKS tracks already added to this rig
 */

void rig_add_track(struct rig_t *rig, struct track_t *track)
{
    assert(rig->ntrack < MAX_TRACKS);

    rig->track[rig->ntrack] = track;
    rig->ntrack++;
}

/*
 * Main thread which handles input and output
 *
 * The rig is the main thread of execution. It is responsible for all
 * non-priority event driven operations (eg. everything but audio).
 *
 * The SDL interface requires its own event loop, and so whilst the
 * rig is technically responsible for managing it, it does very little
 * on its behalf. In future if there are other interfaces or
 * controllers (which expected to use more traditional file-descriptor
 * I/O), the rig will also be responsible for them.
 */

int rig_main(struct rig_t *rig)
{
    int r;
    size_t n;
    struct pollfd pt[MAX_POLLFDS], *pe;

    assert(rig->ntrack <= MAX_POLLFDS);

    for (;;) { /* exit via EVENT_QUIT */
        pe = pt;

        /* ppoll() is not widely available, so use a pipe between this
         * thread and the outside. A single byte wakes up poll() to
         * inform us that new file descriptors need to be polled */

        pe->fd = rig->event[0];
        pe->revents = 0;
        pe->events = POLLIN;
        pe++;

        /* Fetch file descriptors to monitor from each track */

        for (n = 0; n < rig->ntrack; n++)
            pe += track_pollfd(rig->track[n], pe);

        r = poll(pt, pe - pt, -1);
        if (r == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                perror("poll");
                return -1;
            }
        }

        /* Process all events on the event pipe */

        if (pt[0].revents != 0) {
            for (;;) {
                char event;
                size_t z;

                z = read(rig->event[0], &event, 1);
                if (z == -1) {
                    if (errno == EAGAIN) {
                        break;
                    } else {
                        perror("read");
                        return -1;
                    }
                }

                switch (event) {
                case EVENT_WAKE:
                    break;

                case EVENT_QUIT:
                    goto finish;

                default:
                    abort();
                }
            }
        }

        /* Do any reading and writing on all tracks */

        for (n = 0; n < rig->ntrack; n++)
            track_handle(rig->track[n]);
    }
 finish:

    return 0;
}

/*
 * Post a simple event into the rig event loop
 */

static int post_event(struct rig_t *rig, char event)
{
    if (write(rig->event[1], &event, 1) == -1) {
        perror("write");
        return -1;
    }
    return 0;
}

/*
 * Wake up the rig to inform it that the poll table has changed
 */

int rig_awaken(struct rig_t *rig)
{
    return post_event(rig, EVENT_WAKE);
}

/*
 * Ask the rig to exit from another thread or signal handler
 */

int rig_quit(struct rig_t *rig)
{
    return post_event(rig, EVENT_QUIT);
}
