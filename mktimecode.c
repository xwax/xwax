/*
 * Copyright (C) 2016 Mark Hills <mark@xwax.org>
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

/*
 * Experimental program to generate a timecode signal for use
 * with xwax.
 */

#define _GNU_SOURCE /* sincos() */
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define BANNER "xwax timecode generator " \
    "(C) Copyright 2016 Mark Hills <mark@xwax.org>"

#define RATE 44100
#define RESOLUTION 4000
#define SEED 0x00001
#define TAPS 0x00002
#define BITS 22

#define MAX(x,y) ((x)>(y)?(x):(y))

typedef unsigned int bits_t;

/*
 * Calculate the next bit in the LFSR sequence
 */

static inline bits_t lfsr(bits_t code, bits_t taps)
{
    bits_t taken;
    int xrs;

    taken = code & taps;
    xrs = 0;
    while (taken != 0x0) {
        xrs += taken & 0x1;
        taken >>= 1;
    }

    return xrs & 0x1;
}

/*
 * LFSR in the forward direction
 */

static inline bits_t fwd(bits_t current, bits_t taps, unsigned int nbits)
{
    bits_t l;

    /* New bits are added at the MSB; shift right by one */

    l = lfsr(current, taps | 0x1);
    return (current >> 1) | (l << (nbits - 1));
}

static inline double dither(void)
{
    return (double)(rand() % 32768) / 32768.0 - 0.5;
}

int main(int argc, char *argv[])
{
    unsigned int s;
    bits_t b;
    int length;

    fputs(BANNER, stderr);
    fputc('\n', stderr);

    fprintf(stderr, "Generating %d-bit %dHz timecode sampled at %dKhz\n",
            BITS, RESOLUTION, RATE);

    b = SEED;
    length = 0;

    for (s = 0; ; s++) {
        double time, cycle, angle, modulate, x, y;
        signed short c[2];

        time = (double)s / RATE;
        cycle = time * RESOLUTION;
        angle = cycle * M_PI * 2;

        sincos(angle, &x, &y);

        /* Modulate the waveform according to the bitstream */

        modulate = 1.0 - (-cos(angle) + 1.0) * 0.25 * ((b & 0x1) == 0);
        x *= modulate;
        y *= modulate;

        /* Write out 16-bit PCM data */

        c[0] = -y * SHRT_MAX * 0.5 + dither();
        c[1] = x * SHRT_MAX * 0.5 + dither();

        fwrite(c, sizeof(signed short), 2, stdout);

        /* Advance the bitstream if required */

        if ((int)cycle > length) {
            assert((int)cycle - length == 1);
            b = fwd(b, TAPS, BITS);
            if (b == SEED) /* LFSR period reached */
                break;
            length = cycle;
        }
    }

    fprintf(stderr, "Generated %0.1f seconds of timecode\n",
            (double)length / RESOLUTION);

    fprintf(stderr, "\n");
    fprintf(stderr, "    {\n");
    fprintf(stderr, "        .resolution = %d,\n", RESOLUTION);
    fprintf(stderr, "        .bits = %d,\n", BITS);
    fprintf(stderr, "        .seed = 0x%08x,\n", SEED);
    fprintf(stderr, "        .taps = 0x%08x,\n", TAPS);
    fprintf(stderr, "        .length = %d,\n", length);
    fprintf(stderr, "        .safe = %d,\n", MAX(0, length - 4 * RESOLUTION));
    fprintf(stderr, "    }\n");

    return 0;
}
