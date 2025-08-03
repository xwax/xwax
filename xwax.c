/*
 * Copyright (C) 2025 Mark Hills <mark@xwax.org>
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

#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h> /* mlockall() */

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

#define DEFAULT_ALSA_BUFFER 240 /* samples */

#define DEFAULT_PRIORITY 80

#define DEFAULT_IMPORTER EXECDIR "/xwax-import"
#define DEFAULT_SCANNER EXECDIR "/xwax-scan"
#define DEFAULT_TIMECODE "serato_2a"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

char *banner = "xwax " VERSION \
    " (C) Copyright 2025 Mark Hills <mark@xwax.org>";

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
      "  --lock-ram          Lock real-time memory into RAM\n"
      "  --rtprio <n>        Real-time priority (0 for no priority, default %d)\n"
      "  --geometry <s>      Set display geometry (see man page)\n"
      "  --no-decor          Request a window with no decorations\n"
      "  -h, --help          Display this message to stdout and exit\n\n",
      DEFAULT_PRIORITY);

    fprintf(fd, "Music library options:\n"
      "  -l, --crate <path>  Location to scan for audio tracks\n"
      "  --scan <program>    Library scanner (default '%s')\n\n",
      DEFAULT_SCANNER);

    fprintf(fd, "Deck options:\n"
      "  --timecode <name>   Timecode name\n"
      "  --33                Use timecode at 33.3RPM (default)\n"
      "  --45                Use timecode at 45RPM\n"
      "  --[no-]protect      Protect against certain operations while playing\n"
      "  --line              Line level signal (default)\n"
      "  --phono             Tolerate cartridge level signal ('software pre-amp')\n"
      "  --import <program>  Track importer (default '%s')\n"
      "  --dummy             Build a dummy deck with no audio device\n\n",
      DEFAULT_IMPORTER);

#ifdef WITH_OSS
    fprintf(fd, "OSS device options:\n"
      "  --oss <device>      Build a deck connected to OSS audio device\n"
      "  --rate <hz>         Sample rate (default 48000Hz)\n"
      "  --oss-buffers <n>   Number of buffers (default %d)\n"
      "  --oss-fragment <n>  Buffer size to request (2^n bytes, default %d)\n\n",
      DEFAULT_OSS_BUFFERS, DEFAULT_OSS_FRAGMENT);
#endif

#ifdef WITH_ALSA
    fprintf(fd, "ALSA device options:\n"
      "  --alsa <device>     Build a deck connected to ALSA audio device\n"
      "  --rate <hz>         Sample rate (default is automatic)\n"
      "  --buffer <n>        Buffer size (default %d samples)\n\n",
      DEFAULT_ALSA_BUFFER);
#endif

#ifdef WITH_JACK
    fprintf(fd, "JACK device options:\n"
      "  --jack <name>       Create a JACK deck with the given name\n\n");
#endif

#ifdef WITH_ALSA
    fprintf(fd, "MIDI control:\n"
      "  --dicer <device>    Novation Dicer\n\n");
#endif

    fprintf(fd,
      "The ordering of options is important. Options apply to subsequent\n"
      "music libraries or decks, which can be given multiple times. See the\n"
      "manual for details.\n\n"
      "Available timecodes (for use with -t):\n"
      "  serato_2a (default), serato_2b, serato_cd,\n"
      "  pioneer_a, pioneer_b,\n"
      "  traktor_a, traktor_b,\n"
      "  mixvibes_v2, mixvibes_7inch\n\n"
      "See the xwax(1) man page for full information and examples.\n");
}

static void deprecated(const char **arg, const char *old, const char *new)
{
    if (strcmp(*arg, old))
        return;

    fprintf(stderr, "Command line flag '%s' is deprecated; using '%s'\n", old, new);
    *arg = new;
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

int main(int argc, const char *argv[])
{
    int rc = -1, n, priority;
    const char *scanner, *geo;
    char *endptr;
    bool use_mlock, decor;

    struct library library;

#if defined WITH_OSS || WITH_ALSA
    unsigned int rate;  /* or 0 for 'automatic' */
#endif

#ifdef WITH_OSS
    int oss_buffers, oss_fragment;
#endif

#ifdef WITH_ALSA
    unsigned int alsa_buffer;
#endif

    fprintf(stderr, "%s\n\n" NOTICE "\n\n", banner);

    if (setlocale(LC_ALL, "") == NULL) {
        fprintf(stderr, "Could not honour the local encoding\n");
        return -1;
    }

    /* Explicit formatting for numbers; parsing and printing.  Match
     * the user's expectations, and the documentation */

    if (setlocale(LC_NUMERIC, "POSIX") == NULL) {
        fprintf(stderr, "Could not set numeric encoding\n");
        return -1;
    }

    if (thread_global_init() == -1)
        return -1;
    if (library_global_init() == -1)
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
    rate = 0; /* automatic */
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
        deprecated(&argv[0], "-33", "--33");
        deprecated(&argv[0], "-45", "--45");
        deprecated(&argv[0], "-a", "--alsa");
        deprecated(&argv[0], "-c", "--protect");
        deprecated(&argv[0], "-d", "--oss");
        deprecated(&argv[0], "-g", "--geometry");
        deprecated(&argv[0], "-i", "--import");
        deprecated(&argv[0], "-j", "--jack");
        deprecated(&argv[0], "-k", "--lock-ram");
        deprecated(&argv[0], "-q", "--rtprio");
        deprecated(&argv[0], "-s", "--scan");
        deprecated(&argv[0], "-t", "--timecode");
        deprecated(&argv[0], "-u", "--no-protect");
#ifdef WITH_OSS
        deprecated(&argv[0], "-b", "--oss-buffers");
        deprecated(&argv[0], "-f", "--oss-fragment");
#endif

        if (!strcmp(argv[0], "-h") || !strcmp(argv[0], "--help")) {
            usage(stdout);
            return 0;

#ifdef WITH_OSS
        } else if (!strcmp(argv[0], "--oss-fragment")) {

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

        } else if (!strcmp(argv[0], "--oss-buffers")) {

            /* Set number of buffers for subsequent devices */

            if (argc < 2) {
                fprintf(stderr, "%s requires an integer argument.\n", argv[0]);
                return -1;
            }

            oss_buffers = strtol(argv[1], &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "%s requires an integer argument.\n", argv[0]);
                return -1;
            }

            argv += 2;
            argc -= 2;
#endif

#if defined WITH_OSS || WITH_ALSA
        } else if (!strcmp(argv[0], "--rate") || !strcmp(argv[0], "-r")) {

            if (!strcmp(argv[0], "-r"))
                fprintf(stderr, "-r will be removed in future, use --rate instead\n");

            /* Set sample rate for subsequence devices */

            if (argc < 2) {
                fprintf(stderr, "--rate requires an integer argument.\n");
                return -1;
            }

            rate = strtoul(argv[1], &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "--rate requires an integer argument.\n");
                return -1;
            }

            if (rate < 8000) {
                fprintf(stderr, "--rate must be a positive integer, in Hz.\n");
                return -1;
            }

            argv += 2;
            argc -= 2;
#endif

#ifdef WITH_ALSA
        } else if (!strcmp(argv[0], "-m")) {
            fprintf(stderr, "-m is no longer available, check the man page for --buffer in samples\n");
            return -1;

        } else if (!strcmp(argv[0], "--buffer")) {

            /* Set size of ALSA buffer for subsequence devices */

            if (argc < 2) {
                fprintf(stderr, "--buffer requires an integer argument.\n");
                return -1;
            }

            alsa_buffer = strtoul(argv[1], &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "--buffer requires an integer argument.\n");
                return -1;
            }

            argv += 2;
            argc -= 2;
#endif

        } else if (!strcmp(argv[0], "--oss") || !strcmp(argv[0], "--alsa") ||
                  !strcmp(argv[0], "--jack"))
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

            switch(argv[0][2]) {

#ifdef WITH_OSS
            case 'o':
                r = oss_init(device, argv[1], rate ? rate : 48000,
                             oss_buffers, oss_fragment);
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
                fprintf(stderr, "Device '%s' is not supported by this "
                        "distribution of xwax.\n", argv[0]);
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

        } else if (!strcmp(argv[0], "--timecode")) {

            /* Set the timecode definition to use */

            if (argc < 2) {
                fprintf(stderr, "%s requires a name as an argument.\n", argv[0]);
                return -1;
            }

            timecode = timecoder_find_definition(argv[1]);
            if (timecode == NULL) {
                fprintf(stderr, "Timecode '%s' is not known.\n", argv[1]);
                return -1;
            }

            argv += 2;
            argc -= 2;

        } else if (!strcmp(argv[0], "--33")) {

            speed = 1.0;

            argv++;
            argc--;

        } else if (!strcmp(argv[0], "--45")) {

            speed = 1.35;

            argv++;
            argc--;

        } else if (!strcmp(argv[0], "--protect")) {

            protect = true;

            argv++;
            argc--;

        } else if (!strcmp(argv[0], "--no-protect")) {

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

        } else if (!strcmp(argv[0], "--lock-ram")) {

            use_mlock = true;
            track_use_mlock();

            argv++;
            argc--;

        } else if (!strcmp(argv[0], "--rtprio")) {

            if (argc < 2) {
                fprintf(stderr, "--rtprio requires an integer argument.\n");
                return -1;
            }

            priority = strtol(argv[1], &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "--rtprio requires an integer argument.\n");
                return -1;
            }

            if (priority < 0) {
                fprintf(stderr, "Priority (%d) must be zero or positive.\n",
                        priority);
                return -1;
            }

            argv += 2;
            argc -= 2;

        } else if (!strcmp(argv[0], "--geometry")) {

            if (argc < 2) {
                fprintf(stderr, "%s requires an argument.\n", argv[0]);
                return -1;
            }

            geo = argv[1];

            argv += 2;
            argc -= 2;

        } else if (!strcmp(argv[0], "--no-decor")) {

            decor = false;

            argv++;
            argc--;

        } else if (!strcmp(argv[0], "--import")) {

            /* Importer script for subsequent decks */

            if (argc < 2) {
                fprintf(stderr, "%s requires an executable path "
                        "as an argument.\n", argv[0]);
                return -1;
            }

            importer = argv[1];

            argv += 2;
            argc -= 2;

        } else if (!strcmp(argv[0], "--scan")) {

            /* Scan script for subsequent libraries */

            if (argc < 2) {
                fprintf(stderr, "%s requires an executable path "
                        "as an argument.\n", argv[0]);
                return -1;
            }

            scanner = argv[1];

            argv += 2;
            argc -= 2;

        } else if (!strcmp(argv[0], "-l") || !strcmp(argv[0], "--crate")) {

            /* Load in a music library */

            if (argc < 2) {
                fprintf(stderr, "%s requires a pathname as an argument.\n", argv[0]);
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
    library_global_clear();
    thread_global_clear();

    if (rc == EXIT_SUCCESS)
        fprintf(stderr, "Done.\n");

    return rc;
}
