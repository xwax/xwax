/*
 * Copyright (C) 2008 Mark Hills <mark@pogo.org.uk>
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
#include <jack/jack.h>

#include "device.h"
#include "jack.h"

struct jack_t {
    int index;
};

static jack_client_t *client = NULL;

static int process_callback(jack_nframes_t nframes, void *arg)
{
    return 0;
}

static void shutdown_callback(void *arg)
{
}

static int start_jack_client(void)
{
    const char *server_name;
    jack_status_t status;

    client = jack_client_open("xwax", JackNullOption, &status, &server_name);
    if(client == NULL) {
        if (status & JackServerFailed)
            fprintf(stderr, "JACK: Failed to connect\n");
        else
            fprintf(stderr, "jack_client_open: Failed (0x%x)\n", status);
        return -1;
    }

    if(jack_set_process_callback(client, process_callback, 0) != 0) {
        fprintf(stderr, "JACK: Failed to set process callback\n");
        return -1;
    }

    jack_on_shutdown(client, shutdown_callback, 0);

    fprintf(stderr, "JACK: %dHz\n", jack_get_sample_rate(client));

    return 0;
}

static int start(struct device_t *dv)
{
    return 0;
}

static int stop(struct device_t *dv)
{
    return 0;
}

static int clear(struct device_t *dv)
{
    free(dv->local);
    return 0;
}

int jack_init(struct device_t *dv, const char *name)
{
    struct jack_t *jack;

    /* If this is the first JACK deck, initialise the global JACK services */

    if(client == NULL) {
        if(start_jack_client() == -1)
            return -1;
    }

    jack = malloc(sizeof(struct jack_t));
    if(!jack) {
        perror("malloc");
        return -1;
    }

    dv->local = jack;

    dv->pollfds = NULL;
    dv->handle = NULL;
    dv->start = start;
    dv->stop = stop;
    dv->clear = clear;

    return 0;
}
