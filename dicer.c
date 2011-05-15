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

#include <stdlib.h>

#include "controller.h"
#include "debug.h"
#include "deck.h"
#include "dicer.h"
#include "midi.h"
#include "realtime.h"

#define CUE  0
#define LOOP 1
#define ROLL 2

static const char *actions[] = {
    "CUE",
    "LOOP",
    "ROLL"
};

struct dicer {
    struct midi midi;
    struct deck *left, *right;
};

/*
 * Add a deck to the dicer or pair of dicer
 */

static int add_deck(struct controller *c, struct deck *k)
{
    struct dicer *d = c->local;

    debug("dicer: add deck %p\n", d);

    if (d->left != NULL && d->right != NULL)
        return -1;

    if (d->left == NULL) {
        d->left = k;
    } else {
        d->right = k;
    }

    return 0;
}

static void event_decoded(struct deck *d, unsigned char action,
                          bool shift, unsigned char button, bool on)
{
    if (!on)
        return;

    if (action != CUE)
        return;

    if (shift) {
        deck_set_cue(d, button);
    } else {
        deck_seek_to_cue(d, button);
    }
}

static void event(struct dicer *d, unsigned char buf[3])
{
    struct deck *deck;
    unsigned char action, button;
    bool on, shift;

    switch (buf[0]) {
    case 0x9a:
    case 0x9b:
    case 0x9c:
        deck = d->left;
        action = buf[0] - 0x9a;
        break;

    case 0x9d:
    case 0x9e:
    case 0x9f:
        deck = d->right;
        action = buf[0] - 0x9d;
        break;

    default:
        abort();
    }

    if (deck == NULL)
        return;

    switch (buf[1]) {
    case 0x3c:
    case 0x3d:
    case 0x3e:
    case 0x3f:
    case 0x40:
        button = buf[1] - 0x3c;
        shift = false;
        break;

    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
        button = buf[1] - 0x41;
        shift = true;
        break;

    default:
        abort();
    }

    switch (buf[2]) {
    case 0x00:
        on = false;
        break;

    case 0x7f:
        on = true;
        break;

    default:
        abort();
    }

    debug("dicer: %s button %s%hhd %s, deck %p\n",
          actions[action],
          shift ? "SHIFT-" : "",
          button, on ? "ON" : "OFF",
          deck);

    event_decoded(deck, action, shift, button, on);
}

static int realtime(struct controller *c)
{
    struct dicer *d = c->local;

    for (;;) {
        unsigned char buf[3];
        ssize_t z;

        z = midi_read(&d->midi, buf, sizeof buf);
        if (z == -1)
            return -1;
        if (z == 0)
            return 0;

        debug("dicer: got event\n");

        event(d, buf);
    }
}

static void clear(struct controller *c)
{
    struct dicer *d = c->local;

    debug("dicer: clear\n");
    midi_close(&d->midi);
    free(c->local);
}

static struct controller_ops dicer_ops = {
    .add_deck = add_deck,
    .realtime = realtime,
    .clear = clear,
};

int dicer_init(struct controller *c, struct rt *rt, const char *hw)
{
    struct dicer *d;

    debug("dicer: init\n");

    d = malloc(sizeof *d);
    if (d == NULL) {
        perror("malloc");
        return -1;
    }

    if (midi_open(&d->midi, hw) == -1)
        goto fail;

    d->left = NULL;
    d->right = NULL;

    controller_init(c, &dicer_ops);
    c->local = d;

    if (rt_add_controller(rt, c) == -1)
        abort(); /* FIXME */

    return 0;

 fail:
    free(d);
    return -1;
}
