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

#include "midi.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(*(x)))

int main(int argc, char *argv[])
{
    struct midi m;
    struct pollfd pt[8];

    if (argc != 2) {
        fprintf(stderr, "usage: %s <device>\n", argv[0]);
        return -1;
    }

    if (midi_open(&m, argv[1]) == -1)
        return -1;

    for (;;) {
        ssize_t z;
        char buf[32];

        z = midi_pollfds(&m, pt, ARRAY_SIZE(pt));
        if (z == -1)
            return -1;

        fputs("poll...\n", stderr);

        if (poll(pt, z, -1) == -1) {
            perror("poll");
            return -1;
        }

        fputs("... return\n", stderr);

        for (;;) {
            size_t n;
            ssize_t z;

            z = midi_read(&m, buf, sizeof buf);
            if (z == -1)
                return -1;
            if (z == 0)
                break;

            for (n = 0; n < z; n++)
                printf(" %02hhx", buf[n]);

            z = midi_write(&m, buf, z);
            printf(" =%d", z);
            if (z == -1)
                return -1;
        }
        printf("\n");

        fflush(stdout);
    }

    midi_close(&m);
}
