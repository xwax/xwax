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

#include "deck.h"
#include "rig.h"

/*
 * Initialise a deck
 *
 * A deck is a logical grouping of the varius components which
 * reflects the user's view on a deck in the system.
 *
 * Pre: deck->device, deck->timecoder and deck->track are valid
 */

int deck_init(struct deck_t *deck, struct rt_t *rt)
{
    unsigned int sample_rate;

    rig_add_track(&deck->track);
    if (rt_add_device(rt, &deck->device) == -1)
        return -1;

    sample_rate = device_sample_rate(&deck->device);
    player_init(&deck->player, sample_rate, &deck->track, &deck->timecoder);

    /* The timecoder and player are driven by requests from
     * the audio device */

    device_connect_timecoder(&deck->device, &deck->timecoder);
    device_connect_player(&deck->device, &deck->player);

    return 0;
}

void deck_clear(struct deck_t *deck)
{
    /* FIXME: remove from rig and rt */
    player_clear(&deck->player);
    track_clear(&deck->track);
    timecoder_clear(&deck->timecoder);
    device_clear(&deck->device);
}
