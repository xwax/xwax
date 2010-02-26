/*
 * Copyright (C) 2010 Mark Hills <mark@pogo.org.uk>
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL.h> /* may override main() */

#include "alsa.h"
#include "device.h"
#include "interface.h"
#include "jack.h"
#include "library.h"
#include "oss.h"
#include "player.h"
#include "rig.h"
#include "timecoder.h"
#include "track.h"
#include "xwax.h"

#define MAX_DECKS 3

#define DEFAULT_OSS_BUFFERS 8
#define DEFAULT_OSS_FRAGMENT 7

#define DEFAULT_ALSA_BUFFER 8 /* milliseconds */

#define DEFAULT_RATE 44100

#define DEFAULT_IMPORTER EXECDIR "/xwax-import"
#define DEFAULT_SCANNER EXECDIR "/xwax-scan"
#define DEFAULT_TIMECODE "serato_2a"


/* We don't use the full flexibility of a rig, and just have a direct
 * correspondence between a device, track, player and timecoder */

struct deck_t {
    struct device_t device;
    struct track_t track;
    struct player_t player;
    struct timecoder_t timecoder;
};


static int deck_init(struct deck_t *deck, const char *timecode,
                     const char *importer, unsigned int sample_rate)
{
    if (timecoder_init(&deck->timecoder, timecode, sample_rate) == -1)
        return -1;
    track_init(&deck->track, importer);
    player_init(&deck->player);
    player_connect_track(&deck->player, &deck->track);    
    return 0;
}


static void deck_clear(struct deck_t *deck)
{
    track_clear(&deck->track);
    timecoder_clear(&deck->timecoder);
    player_clear(&deck->player);
    device_clear(&deck->device);
}


static void connect_deck_to_interface(struct interface_t *iface, int n,
                                      struct deck_t *deck)
{
    iface->timecoder[n] = &deck->timecoder;
    iface->player[n] = &deck->player;
}


static void connect_deck_to_rig(struct rig_t *rig, int n, struct deck_t *deck)
{
    rig->device[n] = &deck->device;
    rig->track[n] = &deck->track;
    rig->player[n] = &deck->player;
    rig->timecoder[n] = &deck->timecoder;
}


void usage(FILE *fd)
{
    fprintf(fd, "Usage: xwax [<options>]\n\n"
      "  -l <directory> Directory to scan for audio tracks\n"
      "  -t <name>      Timecode name\n"
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

    fprintf(fd, "Device options, -t and -i apply to subsequent devices.\n"
      "Option -s applies to subsequent directories.\n"
      "Decks and audio directories can be specified multiple times.\n\n"
      "Available timecodes (for use with -t):\n"
      "  serato_2a (default), serato_2b, serato_cd,\n"
      "  traktor_a, traktor_b, mixvibes_v2, mixvibes_7inch\n\n");
}


int main(int argc, char *argv[])
{
    int r, n, decks, oss_fragment, oss_buffers, rate, alsa_buffer;
    char *endptr, *timecode, *importer, *scanner;

    struct deck_t deck[MAX_DECKS];
    struct rig_t rig;
    struct interface_t iface;
    struct library_t library;
    struct device_t *device;
    
    fprintf(stderr, BANNER "\n\n" NOTICE "\n\n");
    
    interface_init(&iface);
    rig_init(&rig);
    library_init(&library);
    
    decks = 0;
    oss_fragment = DEFAULT_OSS_FRAGMENT;
    oss_buffers = DEFAULT_OSS_BUFFERS;
    rate = DEFAULT_RATE;
    alsa_buffer = DEFAULT_ALSA_BUFFER;
    importer = DEFAULT_IMPORTER;
    scanner = DEFAULT_SCANNER;
    timecode = DEFAULT_TIMECODE;

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

            /* Create a deck */

            if (argc < 2) {
                fprintf(stderr, "-%c requires a device name as an argument.\n",
                        argv[0][1]);
                return -1;
            }

            if (decks == MAX_DECKS) {
                fprintf(stderr, "Too many decks (maximum %d); aborting.\n",
                        MAX_DECKS);
                return -1;
            }
            
            fprintf(stderr, "Initialising deck %d (%s)...\n", decks, argv[1]);

            /* Work out which device type we are using, and initialise
             * an appropriate device. */

            device = &deck[decks].device;

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

            if (deck_init(&deck[decks], timecode, importer, sample_rate) == -1)
                return -1;

            /* The timecoder and player are driven by requests from
             * the audio device */
            
            device_connect_timecoder(device, &deck[decks].timecoder);
            device_connect_player(device, &deck[decks].player);

            /* The rig and interface keep track of everything whilst
             * the program is running */

            connect_deck_to_interface(&iface, decks, &deck[decks]);
            connect_deck_to_rig(&rig, decks, &deck[decks]);

            decks++;
            
            argv += 2;
            argc -= 2;

        } else if (!strcmp(argv[0], "-t")) {

            /* Set the timecode definition to use */

            if (argc < 2) {
                fprintf(stderr, "-t requires a name as an argument.\n");
                return -1;
            }

            timecode = argv[1];
            
            argv += 2;
            argc -= 2;
            
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

        } else {
            fprintf(stderr, "'%s' argument is unknown; try -h.\n", argv[0]);
            return -1;
        }
    }

    if (decks == 0) {
        fprintf(stderr, "You need to give at least one audio device to use "
                "as a deck; try -h.\n");
        return -1;
    }

    iface.players = decks;
    iface.timecoders = decks;

    /* Connect everything up. Do this after selecting a timecode and
     * built the lookup tables. */

    for (n = 0; n < decks; n++)
        player_connect_timecoder(&deck[n].player, &deck[n].timecoder);

    fprintf(stderr, "Starting threads...\n");
    if (rig_start(&rig) == -1)
        return -1;

    fprintf(stderr, "Entering interface...\n");
    iface.library = &library;
    interface_run(&iface);
    
    fprintf(stderr, "Exiting cleanly...\n");

    if (rig_stop(&rig) == -1)
        return -1;
    
    for (n = 0; n < decks; n++)
        deck_clear(&deck[n]);
    
    timecoder_free_lookup();
    library_clear(&library);
    
    fprintf(stderr, "Done.\n");
    
    return 0;
}
