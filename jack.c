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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <jack/jack.h>

#include "device.h"
#include "jack.h"

#define MAX_BLOCK 512 /* samples */
#define SCALE 32768

struct jack {
    bool started;
    jack_port_t *input_port[DEVICE_CHANNELS],
        *output_port[DEVICE_CHANNELS];
};

static jack_client_t *client = NULL;
static unsigned rate,
    ndeck = 0,
    nstarted = 0;
static struct device *device[4];

/* Interleave samples from a set of JACK buffers into a local buffer */

static void interleave(signed short *buf, jack_default_audio_sample_t *jbuf[],
                       jack_nframes_t nframes)
{
    while (nframes--) {
        int n;

        for (n = 0; n < DEVICE_CHANNELS; n++) {
            *buf = (signed short)(*jbuf[n] * SCALE);
            buf++;
            jbuf[n]++;
        }
    }
}

/* Uninterleave samples from a local buffer into a set of JACK buffers */

static void uninterleave(jack_default_audio_sample_t *jbuf[],
                         signed short *buf, jack_nframes_t nframes)
{
    while (nframes--) {
        int n;

        for (n = 0; n < DEVICE_CHANNELS; n++) {
            *jbuf[n] = (jack_default_audio_sample_t)*buf / SCALE;
            buf++;
            jbuf[n]++;
        }
    }
}

/* Process the given number of frames of audio on input and output
 * of the given JACK device */

static void process_deck(struct device *dv, jack_nframes_t nframes)
{
    int n;
    jack_default_audio_sample_t *in[DEVICE_CHANNELS], *out[DEVICE_CHANNELS];
    jack_nframes_t remain;
    struct jack *jack = (struct jack*)dv->local;

    assert(dv->timecoder != NULL);
    assert(dv->player != NULL);

    for (n = 0; n < DEVICE_CHANNELS; n++) {
        in[n] = jack_port_get_buffer(jack->input_port[n], nframes);
        assert(in[n] != NULL);
        out[n] = jack_port_get_buffer(jack->output_port[n], nframes);
        assert(out[n] != NULL);
    }

    /* For large values of nframes, communicate with the timecoder and
     * player in smaller blocks */

    remain = nframes;
    while (remain > 0) {
        signed short buf[MAX_BLOCK * DEVICE_CHANNELS];
        jack_nframes_t block;

        if (remain < MAX_BLOCK)
            block = remain;
        else
            block = MAX_BLOCK;

        /* Timecode input */

        interleave(buf, in, block);
        device_submit(dv, buf, block);

        /* Audio output is handle in the inner loop, so that
         * we get the timecoder applied in small steps */

        device_collect(dv, buf, block);
        uninterleave(out, buf, block);

	remain -= block;
    }
}

/* Process callback, which triggers the processing of audio on all
 * decks controlled by this file */

static int process_callback(jack_nframes_t nframes, void *local)
{
    size_t n;

    for (n = 0; n < ndeck; n++) {
        struct jack *jack;

        jack = (struct jack*)device[n]->local;
        if (jack->started)
            process_deck(device[n], nframes);
    }

    return 0;
}

/* Shutdown callback */

static void shutdown_callback(void *local)
{
}

/* Initialise ourselves as a JACK client, called once per xwax
 * session, not per deck */

static int start_jack_client(void)
{
    const char *server_name;
    jack_status_t status;

    client = jack_client_open("xwax", JackNullOption, &status, &server_name);
    if (client == NULL) {
        if (status & JackServerFailed)
            fprintf(stderr, "JACK: Failed to connect\n");
        else
            fprintf(stderr, "jack_client_open: Failed (0x%x)\n", status);
        return -1;
    }

    if (jack_set_process_callback(client, process_callback, NULL) != 0) {
        fprintf(stderr, "JACK: Failed to set process callback\n");
        return -1;
    }

    jack_on_shutdown(client, shutdown_callback, NULL);

    rate = jack_get_sample_rate(client);
    fprintf(stderr, "JACK: %dHz\n", rate);

    return 0;
}

/* Close the JACK client, at which happens when all decks have been
 * cleared */

static int stop_jack_client(void)
{
    if (jack_client_close(client) != 0) {
        fprintf(stderr, "jack_client_close: Failed\n");
        return -1;
    }
    client = NULL;
    return 0;
}

/* Register the JACK ports needed for a single deck */

static int register_ports(struct jack *jack, const char *name)
{
    size_t n;

    assert(DEVICE_CHANNELS == 2);
    for (n = 0; n < DEVICE_CHANNELS; n++) {
        static const char channel[] = { 'L', 'R' };
        char port_name[32];

	sprintf(port_name, "%s_timecode_%c", name, channel[n]);
        jack->input_port[n] = jack_port_register(client, port_name,
                                                 JACK_DEFAULT_AUDIO_TYPE,
                                                 JackPortIsInput, 0);
	if (jack->input_port[n] == NULL) {
	    fprintf(stderr, "JACK: Failed to register timecode input port\n");
	    return -1;
	}
	sprintf(port_name, "%s_playback_%c", name, channel[n]);
	jack->output_port[n] = jack_port_register(client, port_name,
                                                  JACK_DEFAULT_AUDIO_TYPE,
                                                  JackPortIsOutput, 0);
	if (jack->output_port[n] == NULL) {
	    fprintf(stderr, "JACK: Failed to register audio playback port\n");
	    return -1;
	}
    }
    return 0;
}

/* Return the sample rate */

static unsigned int sample_rate(struct device *dv)
{
    return rate; /* the same rate is used for all decks */
}

/* Start audio rolling on this deck */

static void start(struct device *dv)
{
    struct jack *jack = (struct jack*)dv->local;

    assert(dv->timecoder != NULL);
    assert(dv->player != NULL);

    /* On the first call to start, start audio rolling for all decks */

    if (nstarted == 0) {
        if (jack_activate(client) != 0)
            abort();
    }

    nstarted++;
    jack->started = true;
}

/* Stop audio rolling on this deck */

static void stop(struct device *dv)
{
    struct jack *jack = (struct jack*)dv->local;

    jack->started = false;
    nstarted--;

    /* On the final stop call, stop JACK rolling */

    if (nstarted == 0) {
        if (jack_deactivate(client) != 0)
            abort();
    }
}

/* Close JACK deck and any allocations */

static void clear(struct device *dv)
{
    struct jack *jack = (struct jack*)dv->local;
    int n;

    /* Unregister ports */

    for (n = 0; n < DEVICE_CHANNELS; n++) {
        if (jack_port_unregister(client, jack->input_port[n]) != 0)
            abort();
        if (jack_port_unregister(client, jack->output_port[n]) != 0)
            abort();
    }

    free(dv->local);

    /* Remove this from the global list, so that potentially xwax could
     * continue to run even if a deck is removed */

    for (n = 0; n < ndeck; n++) {
        if (device[n] == dv)
            break;
    }
    assert(n != ndeck);

    if (ndeck == 1) { /* this is the last remaining deck */
        stop_jack_client();
        ndeck = 0;
    } else {
        device[n] = device[ndeck - 1]; /* compact the list */
        ndeck--;
    }
}

static struct device_ops jack_ops = {
    .sample_rate = sample_rate,
    .start = start,
    .stop = stop,
    .clear = clear
};

/* Initialise a new JACK deck, creating a new JACK client if required,
 * and the approporiate input and output ports */

int jack_init(struct device *dv, const char *name)
{
    struct jack *jack;

    /* If this is the first JACK deck, initialise the global JACK services */

    if (client == NULL) {
        if (start_jack_client() == -1)
            return -1;
    }

    jack = malloc(sizeof *jack);
    if (jack == NULL) {
        perror("malloc");
        return -1;
    }

    jack->started = false;
    if (register_ports(jack, name) == -1)
        goto fail;

    device_init(dv, &jack_ops);
    dv->local = jack;

    assert(ndeck < sizeof device);
    device[ndeck] = dv;
    ndeck++;

    return 0;

 fail:
    free(jack);
    return -1;
}
