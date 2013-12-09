/*
 * Copyright (C) 2012 Mark Hills <mark@xwax.org>
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

#include <signal.h>
#include <stdio.h>

#include "library.h"
#include "rig.h"
#include "thread.h"

void handle(int signum)
{
    rig_quit();
}

/*
 * Manual test of the record library. Mainly for use with
 * valgrind to check for memory bugs etc.
 */

int main(int argc, char *argv[])
{
    const char *scan;
    size_t n;
    struct library lib;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <scan> <path> [...]\n", argv[0]);
        return -1;
    }

    scan = argv[1];

    if (thread_global_init() == -1)
        return -1;

    if (rig_init() == -1)
        return -1;

    if (signal(SIGINT, handle) == SIG_ERR) {
        perror("signal");
        return -1;
    }

    if (library_init(&lib) == -1)
        return -1;

    for (n = 2; n < argc; n++) {
        if (library_import(&lib, scan, argv[n]) == -1)
            return -1;
    }

    rig_main();

    library_clear(&lib);
    rig_clear();
    thread_global_clear();

    return 0;
}
