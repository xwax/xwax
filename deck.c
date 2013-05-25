/*
 * Copyright (C) 2013 Mark Hills <mark@xwax.org>
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

#include "controller.h"
#include "cues.h"
#include "deck.h"
#include "status.h"
#include "rig.h"

/*
 * An empty record, is used briefly until a record is loaded
 * to a deck
 */

static const struct record no_record = {
    .artist = "",
    .title = ""
};

/*
 * Initialise a deck
 *
 * A deck is a logical grouping of the various components which
 * reflects the user's view on a deck in the system.
 *
 * Pre: deck->device, deck->timecoder, deck->importer are valid
 */

int deck_init(struct deck *deck, struct rt *rt)
{
    unsigned int rate;

    assert(deck->importer != NULL);

    if (rt_add_device(rt, &deck->device) == -1)
        return -1;

    deck->ncontrol = 0;
    deck->record = &no_record;
    deck->punch = NO_PUNCH;
    rate = device_sample_rate(&deck->device);
    player_init(&deck->player, rate, track_get_empty(), &deck->timecoder);
    cues_reset(&deck->cues);

    /* The timecoder and player are driven by requests from
     * the audio device */

    device_connect_timecoder(&deck->device, &deck->timecoder);
    device_connect_player(&deck->device, &deck->player);

    return 0;
}

void deck_clear(struct deck *deck)
{
    /* FIXME: remove from rig and rt */
    player_clear(&deck->player);
    timecoder_clear(&deck->timecoder);
    device_clear(&deck->device);
}

bool deck_is_locked(const struct deck *deck)
{
    return (deck->protect && player_is_active(&deck->player));
}

/*
 * Load a record from the library to a deck
 */

void deck_load(struct deck *deck, struct record *record)
{
    struct track *t;

    if (deck_is_locked(deck)) {
        status_printf(STATUS_WARN, "Stop deck to load a different track");
        return;
    }

    t = track_get_by_import(deck->importer, record->pathname);
    if (t == NULL)
        return;

    deck->record = record;
    player_set_track(&deck->player, t); /* passes reference */
}

void deck_recue(struct deck *deck)
{
    if (deck_is_locked(deck)) {
        status_printf(STATUS_WARN, "Stop deck to recue");
        return;
    }

    player_recue(&deck->player);
}

void deck_clone(struct deck *deck, const struct deck *from)
{
    deck->record = from->record;
    player_clone(&deck->player, &from->player);
}

/*
 * Clear the cue point, ready to be set again
 */

void deck_unset_cue(struct deck *d, unsigned int label)
{
    cues_unset(&d->cues, label);
}

/*
 * Seek the current playback position to a cue point position,
 * or set the cue point if unset
 */

void deck_cue(struct deck *d, unsigned int label)
{
    double p;

    p = cues_get(&d->cues, label);
    if (p == CUE_UNSET)
        cues_set(&d->cues, label, player_get_elapsed(&d->player));
    else
        player_seek_to(&d->player, p);
}

/*
 * Seek to a cue point ready to return from it later. Overrides an
 * existing punch operation.
 */

void deck_punch_in(struct deck *d, unsigned int label)
{
    double p, e;

    e = player_get_elapsed(&d->player);
    p = cues_get(&d->cues, label);
    if (p == CUE_UNSET) {
        cues_set(&d->cues, label, e);
        return;
    }

    if (d->punch != NO_PUNCH)
        e -= d->punch;

    player_seek_to(&d->player, p);
    d->punch = p - e;
}

/*
 * Return from a cue point
 */

void deck_punch_out(struct deck *d)
{
    double e;

    if (d->punch == NO_PUNCH)
        return;

    e = player_get_elapsed(&d->player);
    player_seek_to(&d->player, e - d->punch);
    d->punch = NO_PUNCH;
}
