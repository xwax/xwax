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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/poll.h>

#include "list.h"
#include "mutex.h"
#include "realtime.h"
#include "rig.h"

#define EVENT_WAKE 0
#define EVENT_QUIT 1

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

static int event[2]; /* pipe to wake up service thread */
static struct list tracks = LIST_INIT(tracks);
mutex lock;

int rig_init()
{
    /* Create a pipe which will be used to wake us from other threads */

    if (pipe(event) == -1) {
        perror("pipe");
        return -1;
    }

    if (fcntl(event[0], F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        if (close(event[1]) == -1)
            abort();
        if (close(event[0]) == -1)
            abort();
        return -1;
    }

    mutex_init(&lock);

    return 0;
}

void rig_clear()
{
    mutex_clear(&lock);

    if (close(event[0]) == -1)
        abort();
    if (close(event[1]) == -1)
        abort();
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

int rig_main()
{
    struct pollfd pt[4];

    /* ppoll() is not widely available, so use a pipe between this
     * thread and the outside. A single byte wakes up poll() to
     * inform us that new file descriptors need to be polled */

    pt[0].fd = event[0];
    pt[0].revents = 0;
    pt[0].events = POLLIN;

    mutex_lock(&lock);

    for (;;) { /* exit via EVENT_QUIT */
        int r;
        struct pollfd *pe;
        struct track_t *track;

        pe = &pt[1];

        /* Fetch file descriptors to monitor from each track */

        list_for_each(track, &tracks, rig) {
            track_pollfd(track, pe);
            pe++;

            /* If we ran out of file descriptors, ignore the
             * oldest until we have space in the poll table */

            if (pe >= pt + ARRAY_SIZE(pt))
                break;
        }

        mutex_unlock(&lock);

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
                char e;
                size_t z;

                z = read(event[0], &e, 1);
                if (z == -1) {
                    if (errno == EAGAIN) {
                        break;
                    } else {
                        perror("read");
                        return -1;
                    }
                }

                switch (e) {
                case EVENT_WAKE:
                    break;

                case EVENT_QUIT:
                    goto finish;

                default:
                    abort();
                }
            }
        }

        mutex_lock(&lock);

        /* Do any reading and writing on all tracks */

        list_for_each(track, &tracks, rig) {
            if (track_handle(track)) {
                list_del(&track->rig);
                track_put(track);
            }
        }
    }
 finish:

    return 0;
}

/*
 * Post a simple event into the rig event loop
 */

static int post_event(char e)
{
    rt_not_allowed();

    if (write(event[1], &e, 1) == -1) {
        perror("write");
        return -1;
    }

    return 0;
}

/*
 * Ask the rig to exit from another thread or signal handler
 */

int rig_quit()
{
    return post_event(EVENT_QUIT);
}

/*
 * Add a track to be handled until import has completed
 */

void rig_post_track(struct track_t *t)
{
    track_get(t);

    mutex_lock(&lock);
    list_add(&t->rig, &tracks);
    mutex_unlock(&lock);

    post_event(EVENT_WAKE);
}
