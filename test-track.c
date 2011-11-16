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

#include <stdio.h>
#include <sys/poll.h>

#include "track.h"

/*
 * Self-contained manual test of a track import operation
 */

int main(int argc, char *argv[])
{
    struct pollfd pe;
    struct track track;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <command> <path>\n", argv[0]);
        return -1;
    }

    track_init(&track, argv[1]);

    if (track_import(&track, argv[2]) == -1)
        return -1;

    for (;;) {
        ssize_t nfds;

        nfds = track_pollfd(&track, &pe);
        if (nfds == 0)
            break;

        if (poll(&pe, nfds, -1) == -1) {
            perror("poll");
            break;
        }

        track_handle(&track);
    }

    track_clear(&track);

    return 0;
}
