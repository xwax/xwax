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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h> /* mlockall() */

#include <SDL.h> /* may override main() */

#include "alsa.h"
#include "controller.h"
#include "device.h"
#include "dicer.h"
#include "dummy.h"
#include "interface.h"
#include "jack.h"
#include "library.h"
#include "oss.h"
#include "realtime.h"
#include "thread.h"
#include "rig.h"
#include "timecoder.h"
#include "track.h"
#include "xwax.h"

#define DEFAULT_OSS_BUFFERS 8
#define DEFAULT_OSS_FRAGMENT 7

#define DEFAULT_ALSA_BUFFER 8 /* milliseconds */

#define DEFAULT_RATE 44100
#define DEFAULT_PRIORITY 80

#define DEFAULT_IMPORTER EXECDIR "/xwax-import"
#define DEFAULT_SCANNER EXECDIR "/xwax-scan"
#define DEFAULT_TIMECODE "serato_2a"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

char *banner = "xwax " VERSION \
    " (C) Copyright 2016 Mark Hills <mark@xwax.org>";

size_t ndeck;
struct deck deck[3];

static size_t nctl;
static struct controller ctl[2];

static struct rt rt;

static double speed;
static bool protect, phono;
static const char *importer;
static struct timecode_def *timecode;

static void usage(FILE *fd)
{
    fprintf(fd, "Usage: xwax [<options>]\n\n");

    fprintf(fd, "Program-wide options:\n"
      "  -k             Lock real-time memory into RAM\n"
      "  -q <n>         Real-time priority (0 for no priority, default %d)\n"
      "  -g <s>         Set display geometry (see man page)\n"
      "  --no-decor     Request a window with no decorations\n"
      "  -h             Display this message to stdout and exit\n\n",
      DEFAULT_PRIORITY);

    fprintf(fd, "Music library options:\n"
      "  -l <path>      Location to scan for audio tracks\n"
      "  -s <program>   Library scanner (default '%s')\n\n",
      DEFAULT_SCANNER);

    fprintf(fd, "Deck options:\n"
      "  -t <name>      Timecode name\n"
      "  -33            Use timecode at 33.3RPM (default)\n"
      "  -45            Use timecode at 45RPM\n"
      "  -c             Protect against certain operations while playing\n"
      "  -u             Allow all operations when playing\n"
      "  --line         Line level signal (default)\n"
      "  --phono        Tolerate cartridge level signal ('software pre-amp')\n"
      "  -i <program>   Importer (default '%s')\n"
      "  --dummy        Build a dummy deck with no audio device\n\n",
      DEFAULT_IMPORTER);

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

#ifdef WITH_ALSA
    fprintf(fd, "MIDI control:\n"
      "  --dicer <dev>   Novation Dicer\n\n");
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

static struct device* start_deck(const char *desc)
{
    fprintf(stderr, "Initialising deck %zd (%s)...\n", ndeck, desc);

    if (ndeck == ARRAY_SIZE(deck)) {
        fprintf(stderr, "Too many decks.\n");
        return NULL;
    }

    return &deck[ndeck].device;
}

static int commit_deck(void)
{
    int r;
    struct deck *d;
    size_t n;

    /* Fallback to a default timecode. Don't initialise this at the
     * front of the program to avoid buildling unnecessary LUTs */

    if (timecode == NULL) {
        timecode = timecoder_find_definition(DEFAULT_TIMECODE);
        assert(timecode != NULL);
    }

    d = &deck[ndeck];

    r = deck_init(d, &rt, timecode, importer, speed, phono, protect);
    if (r == -1)
        return -1;

    /* Connect this deck to available controllers */

    for (n = 0; n < nctl; n++)
        controller_add_deck(&ctl[n], d);

    ndeck++;

    return 0;
}

int main(int argc, char *argv[])
{
    int rc = -1, n, priority;
    const char *scanner, *geo;
    char *endptr;
    bool use_mlock, decor;

    struct library library;

#if defined WITH_OSS || WITH_ALSA
    int rate;
#endif

#ifdef WITH_OSS
    int oss_buffers, oss_fragment;
#endif

#ifdef WITH_ALSA
    int alsa_buffer;
#endif

    fprintf(stderr, "%s\n\n" NOTICE "\n\n", banner);

    if (thread_global_init() == -1)
        return -1;

    if (rig_init() == -1)
        return -1;
    rt_init(&rt);
    library_init(&library);

    ndeck = 0;
    geo = "";
    decor = true;
    nctl = 0;
    priority = DEFAULT_PRIORITY;
    importer = DEFAULT_IMPORTER;
    scanner = DEFAULT_SCANNER;
    timecode = NULL;
    speed = 1.0;
    protect = false;
    phono = false;
    use_mlock = false;

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
            int r;
            struct device *device;

            /* Create a deck */

            if (argc < 2) {
                fprintf(stderr, "-%c requires a device name as an argument.\n",
                        argv[0][1]);
                return -1;
            }

            device = start_deck(argv[1]);
            if (device == NULL)
                return -1;

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

            commit_deck();

            argv += 2;
            argc -= 2;

        } else if (!strcmp(argv[0], "--dummy")) {

            struct device *v;

            v = start_deck("dummy");
            if (v == NULL)
                return -1;

            dummy_init(v);
            commit_deck();

            argv++;
            argc--;

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

        } else if (!strcmp(argv[0], "-c")) {

            protect = true;

            argv++;
            argc--;

        } else if (!strcmp(argv[0], "-u")) {

            protect = false;

            argv++;
            argc--;

        } else if (!strcmp(argv[0], "--line")) {

            phono = false;

            argv++;
            argc--;

        } else if (!strcmp(argv[0], "--phono")) {

            phono = true;

            argv++;
            argc--;

        } else if (!strcmp(argv[0], "-k")) {

            use_mlock = true;
            track_use_mlock();

            argv++;
            argc--;

        } else if (!strcmp(argv[0], "-q")) {

            if (argc < 2) {
                fprintf(stderr, "-q requires an integer argument.\n");
                return -1;
            }

            priority = strtol(argv[1], &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "-q requires an integer argument.\n");
                return -1;
            }

            if (priority < 0) {
                fprintf(stderr, "Priority (%d) must be zero or positive.\n",
                        priority);
                return -1;
            }

            argv += 2;
            argc -= 2;

        } else if (!strcmp(argv[0], "-g")) {

            if (argc < 2) {
                fprintf(stderr, "-g requires an argument.\n");
                return -1;
            }

            geo = argv[1];

            argv += 2;
            argc -= 2;

        } else if (!strcmp(argv[0], "--no-decor")) {

            decor = false;

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

        } else if (!strcmp(argv[0], "-l")) {

            /* Load in a music library */

            if (argc < 2) {
                fprintf(stderr, "-l requires a pathname as an argument.\n");
                return -1;
            }

            if (library_import(&library, scanner, argv[1]) == -1)
                return -1;

            argv += 2;
            argc -= 2;

#ifdef WITH_ALSA
        } else if (!strcmp(argv[0], "--dicer")) {

            struct controller *c;

            if (nctl == sizeof ctl) {
                fprintf(stderr, "Too many controllers; aborting.\n");
                return -1;
            }

            c = &ctl[nctl];

            if (argc < 2) {
                fprintf(stderr, "Dicer requires an ALSA device name.\n");
                return -1;
            }

            if (dicer_init(c, &rt, argv[1]) == -1)
                return -1;

            nctl++;

            argv += 2;
            argc -= 2;
#endif

        } else {
            fprintf(stderr, "'%s' argument is unknown; try -h.\n", argv[0]);
            return -1;
        }
    }

#ifdef WITH_ALSA
    alsa_clear_config_cache();
#endif

    if (ndeck == 0) {
        fprintf(stderr, "You need to give at least one audio device to use "
                "as a deck; try -h.\n");
        return -1;
    }

    rc = EXIT_FAILURE; /* until clean exit */

    /* Order is important: launch realtime thread first, then mlock.
     * Don't mlock the interface, use sparingly for audio threads */

    if (rt_start(&rt, priority) == -1)
        return -1;

    if (use_mlock && mlockall(MCL_CURRENT) == -1) {
        perror("mlockall");
        goto out_rt;
    }

    if (interface_start(&library, geo, decor) == -1)
        goto out_rt;

    if (rig_main() == -1)
        goto out_interface;

    rc = EXIT_SUCCESS;
    fprintf(stderr, "Exiting cleanly...\n");

out_interface:
    interface_stop();
out_rt:
    rt_stop(&rt);

    for (n = 0; n < ndeck; n++)
        deck_clear(&deck[n]);

    for (n = 0; n < nctl; n++)
        controller_clear(&ctl[n]);

    timecoder_free_lookup();
    library_clear(&library);
    rt_clear(&rt);
    rig_clear();
    thread_global_clear();

    if (rc == EXIT_SUCCESS)
        fprintf(stderr, "Done.\n");

    return rc;
}
