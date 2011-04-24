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

#include "controller.h"
#include "cues.h"
#include "deck.h"

/*
 * Initialise a deck
 *
 * A deck is a logical grouping of the varius components which
 * reflects the user's view on a deck in the system.
 *
 * Pre: deck->device, deck->timecoder and deck->track are valid
 */

int deck_init(struct deck_t *deck, struct rt_t *rt, struct rig_t *rig)
{
    deck->ncontrol = 0;

    rig_add_track(rig, &deck->track);
    if (rt_add_device(rt, &deck->device) == -1)
        return -1;

    cues_reset(&deck->cues);
    player_init(&deck->player, &deck->track, &deck->timecoder);

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

/*
 * Set a cue point to the current playback position
 */

void deck_set_cue(struct deck_t *d, unsigned int label)
{
    cues_set(&d->cues, label, player_position(&d->player));
}

/*
 * Seek the current playback position to a cue point position
 */

void deck_seek_to_cue(struct deck_t *d, unsigned int label)
{
    double p;

    p = cues_get(&d->cues, label);
    if (p != CUE_UNSET)
        player_seek_to(&d->player, p);
}
