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

#include <stdio.h>

#include "rig.h"
#include "thread.h"
#include "track.h"

/*
 * Self-contained manual test of a track import operation
 */

int main(int argc, char *argv[])
{
    struct track *track;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <command> <path>\n", argv[0]);
        return -1;
    }

    if (thread_global_init() == -1)
        return -1;

    rig_init();

    track = track_get_by_import(argv[1], argv[2]);
    if (track == NULL)
        return -1;

    rig_main();

    track_put(track);
    rig_clear();
    thread_global_clear();

    return 0;
}
