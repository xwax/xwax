/*
 * Copyright (C) 2024 Mark Hills <mark@xwax.org>
 *
 * This file is part of "xwax".
 *
 * "xwax" is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * "xwax" is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
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

    track = track_acquire_by_import(argv[1], argv[2]);
    if (track == NULL)
        return -1;

    rig_main();

    track_release(track);
    rig_clear();
    thread_global_clear();

    return 0;
}
