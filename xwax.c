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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL.h> /* may override main() */

#include "alsa.h"
#include "deck.h"
#include "device.h"
#include "interface.h"
#include "jack.h"
#include "library.h"
#include "oss.h"
#include "realtime.h"
#include "rig.h"
#include "timecoder.h"
#include "track.h"
#include "xwax.h"

#define DEFAULT_OSS_BUFFERS 8
#define DEFAULT_OSS_FRAGMENT 7

#define DEFAULT_ALSA_BUFFER 8 /* milliseconds */

#define DEFAULT_RATE 44100

#define DEFAULT_IMPORTER EXECDIR "/xwax-import"
#define DEFAULT_SCANNER EXECDIR "/xwax-scan"
#define DEFAULT_TIMECODE "serato_2a"

char *banner = "xwax " VERSION \
    " (C) Copyright 2011 Mark Hills <mark@pogo.org.uk>";

static void usage(FILE *fd)
{
    fprintf(fd, "Usage: xwax [<options>]\n\n"
      "  -l <path>      Location to scan for audio tracks\n"
      "  -p <path>      Ordered playlist for audio tracks\n"
      "  -t <name>      Timecode name\n"
      "  -33            Use timecode at 33.3RPM (default)\n"
      "  -45            Use timecode at 45RPM\n"
      "  -i <program>   Importer (default '%s')\n"
      "  -s <program>   Library scanner (default '%s')\n"
      "  -h             Display this message\n\n",
      DEFAULT_IMPORTER, DEFAULT_SCANNER);

#ifdef WITH_OSS
    fprintf(fd, "OSS device options:\n"
      "  -d <device>    Build a deck connected to OSS audio device\n"
      "  -r <hz>        Sample rate (default %dHz)\n"
      "  -b <n>         Number of buffers (default %d)\n"
      "  -f <n>         Buffer size to request (2^n bytes, default %d)\n\n",
      DEFAULT_RATE, DEFAULT_OSS_BUFFERS, DEFAULT_OSS_FRAGMENT);
#endif

#ifdef WITH_ALSA
    fprintf(fd, "ALSA device options:\n"
      "  -a <device>    Build a deck connected to ALSA audio device\n"
      "  -r <hz>        Sample rate (default %dHz)\n"
      "  -m <ms>        Buffer time (default %dms)\n\n",
      DEFAULT_RATE, DEFAULT_ALSA_BUFFER);
#endif

#ifdef WITH_JACK
    fprintf(fd, "JACK device options:\n"
      "  -j <name>      Create a JACK deck with the given name\n\n");
#endif

    fprintf(fd,
      "The ordering of options is important. Options apply to subsequent\n"
      "music libraries or decks, which can be given multiple times. See the\n"
      "manual for details.\n\n"
      "Available timecodes (for use with -t):\n"
      "  serato_2a (default), serato_2b, serato_cd,\n"
      "  traktor_a, traktor_b, mixvibes_v2, mixvibes_7inch\n\n"
      "See the xwax(1) man page for full information and examples.\n");
}

int main(int argc, char *argv[])
{
    int r, n, decks;
    char *importer, *scanner;
    double speed;
    struct timecode_def_t *timecode;

    struct deck_t deck[3];
    struct rt_t rt;
    struct interface_t iface;
    struct library_t library;

#if defined WITH_OSS || WITH_ALSA
    char *endptr;
    int rate;
#endif

#ifdef WITH_OSS
    int oss_buffers, oss_fragment;
#endif

#ifdef WITH_ALSA
    int alsa_buffer;
#endif

    fprintf(stderr, "%s\n\n" NOTICE "\n\n", banner);

    if (rig_init() == -1)
        return -1;
    rt_init(&rt);
    library_init(&library);

    decks = 0;
    importer = DEFAULT_IMPORTER;
    scanner = DEFAULT_SCANNER;
    timecode = NULL;
    speed = 1.0;

#if defined WITH_OSS || WITH_ALSA
    rate = DEFAULT_RATE;
#endif

#ifdef WITH_ALSA
    alsa_buffer = DEFAULT_ALSA_BUFFER;
#endif

#ifdef WITH_OSS
    oss_fragment = DEFAULT_OSS_FRAGMENT;
    oss_buffers = DEFAULT_OSS_BUFFERS;
#endif

    /* Skip over command name */

    argv++;
    argc--;

    while (argc > 0) {

        if (!strcmp(argv[0], "-h")) {
            usage(stdout);
            return 0;

#ifdef WITH_OSS
        } else if (!strcmp(argv[0], "-f")) {

            /* Set fragment size for subsequent devices */

            if (argc < 2) {
                fprintf(stderr, "-f requires an integer argument.\n");
                return -1;
            }

            oss_fragment = strtol(argv[1], &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "-f requires an integer argument.\n");
                return -1;
            }

            /* Fragment sizes greater than the default aren't useful
             * as they are dependent on DEVICE_FRAME */

            if (oss_fragment < DEFAULT_OSS_FRAGMENT) {
                fprintf(stderr, "Fragment size must be %d or more; aborting.\n",
                        DEFAULT_OSS_FRAGMENT);
                return -1;
            }

            argv += 2;
            argc -= 2;

        } else if (!strcmp(argv[0], "-b")) {

            /* Set number of buffers for subsequent devices */

            if (argc < 2) {
                fprintf(stderr, "-b requires an integer argument.\n");
                return -1;
            }

            oss_buffers = strtol(argv[1], &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "-b requires an integer argument.\n");
                return -1;
            }

            argv += 2;
            argc -= 2;
#endif

#if defined WITH_OSS || WITH_ALSA
        } else if (!strcmp(argv[0], "-r")) {

            /* Set sample rate for subsequence devices */

            if (argc < 2) {
                fprintf(stderr, "-r requires an integer argument.\n");
                return -1;
            }

            rate = strtol(argv[1], &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "-r requires an integer argument.\n");
                return -1;
            }

            argv += 2;
            argc -= 2;
#endif

#ifdef WITH_ALSA
        } else if (!strcmp(argv[0], "-m")) {

            /* Set size of ALSA buffer for subsequence devices */

            if (argc < 2) {
                fprintf(stderr, "-m requires an integer argument.\n");
                return -1;
            }

            alsa_buffer = strtol(argv[1], &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "-m requires an integer argument.\n");
                return -1;
            }

            argv += 2;
            argc -= 2;
#endif

        } else if (!strcmp(argv[0], "-d") || !strcmp(argv[0], "-a") ||
		  !strcmp(argv[0], "-j"))
	{
            unsigned int sample_rate;
            struct deck_t *ld;
            struct device *device;
            struct track_t *track;
            struct timecoder_t *timecoder;

            /* Create a deck */

            if (argc < 2) {
                fprintf(stderr, "-%c requires a device name as an argument.\n",
                        argv[0][1]);
                return -1;
            }

            if (decks == sizeof deck) {
                fprintf(stderr, "Too many decks; aborting.\n");
                return -1;
            }

            fprintf(stderr, "Initialising deck %d (%s)...\n", decks, argv[1]);

            ld = &deck[decks];
            device = &ld->device;
            timecoder = &ld->timecoder;
            track = &ld->track;

            /* Work out which device type we are using, and initialise
             * an appropriate device. */

            switch(argv[0][1]) {

#ifdef WITH_OSS
            case 'd':
                r = oss_init(device, argv[1], rate, oss_buffers, oss_fragment);
                break;
#endif
#ifdef WITH_ALSA
            case 'a':
                r = alsa_init(device, argv[1], rate, alsa_buffer);
                break;
#endif
#ifdef WITH_JACK
            case 'j':
                r = jack_init(device, argv[1]);
                break;
#endif
            default:
                fprintf(stderr, "Device type is not supported by this "
                        "distribution of xwax.\n");
                return -1;
            }

            if (r == -1)
                return -1;

	    sample_rate = device_sample_rate(device);

            /* Default timecode decoder where none is specified */

            if (timecode == NULL) {
                timecode = timecoder_find_definition(DEFAULT_TIMECODE);
                assert(timecode != NULL);
            }

            timecoder_init(timecoder, timecode, speed, sample_rate);
            track_init(track, importer);

            /* Connect up the elements to make an operational deck */

            r = deck_init(ld, &rt);
            if (r == -1)
                return -1;

            decks++;

            argv += 2;
            argc -= 2;

        } else if (!strcmp(argv[0], "-t")) {

            /* Set the timecode definition to use */

            if (argc < 2) {
                fprintf(stderr, "-t requires a name as an argument.\n");
                return -1;
            }

            timecode = timecoder_find_definition(argv[1]);
            if (timecode == NULL) {
                fprintf(stderr, "Timecode '%s' is not known.\n", argv[1]);
                return -1;
            }

            argv += 2;
            argc -= 2;

        } else if (!strcmp(argv[0], "-33")) {

            speed = 1.0;

            argv++;
            argc--;

        } else if (!strcmp(argv[0], "-45")) {

            speed = 1.35;

            argv++;
            argc--;

        } else if (!strcmp(argv[0], "-i")) {

            /* Importer script for subsequent decks */

            if (argc < 2) {
                fprintf(stderr, "-i requires an executable path "
                        "as an argument.\n");
                return -1;
            }

            importer = argv[1];

            argv += 2;
            argc -= 2;

        } else if (!strcmp(argv[0], "-s")) {

            /* Scan script for subsequent libraries */

            if (argc < 2) {
                fprintf(stderr, "-s requires an executable path "
                        "as an argument.\n");
                return -1;
            }

            scanner = argv[1];

            argv += 2;
            argc -= 2;

        } else if (!strcmp(argv[0], "-l") || !strcmp(argv[0], "-p")) {

            bool sort;

            /* Load in a music library */

            if (argc < 2) {
                fprintf(stderr, "-%c requires a pathname as an argument.\n",
                        argv[0][1]);
                return -1;
            }

            if (argv[0][1] == 'p') {
                sort = false;
            } else {
                sort = true;
            }

            if (library_import(&library, sort, scanner, argv[1]) == -1)
                return -1;

            argv += 2;
            argc -= 2;

        } else {
            fprintf(stderr, "'%s' argument is unknown; try -h.\n", argv[0]);
            return -1;
        }
    }

#ifdef WITH_ALSA
    alsa_clear_config_cache();
#endif

    if (decks == 0) {
        fprintf(stderr, "You need to give at least one audio device to use "
                "as a deck; try -h.\n");
        return -1;
    }

    if (interface_start(&iface, deck, decks, &library) == -1)
        return -1;
    if (rt_start(&rt) == -1)
        return -1;

    if (rig_main() == -1)
        return -1;

    fprintf(stderr, "Exiting cleanly...\n");

    rt_stop(&rt);
    interface_stop(&iface);

    for (n = 0; n < decks; n++)
        deck_clear(&deck[n]);

    timecoder_free_lookup();
    library_clear(&library);
    rt_clear(&rt);
    rig_clear();

    fprintf(stderr, "Done.\n");

    return 0;
}
