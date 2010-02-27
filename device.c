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

#include "device.h"


void device_connect_timecoder(struct device_t *dv, struct timecoder_t *tc)
{
    dv->timecoder = tc;
}


void device_connect_player(struct device_t *dv, struct player_t *pl)
{
    dv->player = pl;
}


/* Return the sample rate of the device in Hz */

unsigned int device_sample_rate(struct device_t *dv)
{
    return dv->type->sample_rate(dv);
}


/* Start the device inputting and outputting audio */

int device_start(struct device_t *dv)
{
    if (dv->type->start)
        return dv->type->start(dv);
    else
        return 0;
}


/* Stop the device */

int device_stop(struct device_t *dv)
{
    if (dv->type->stop)
        return dv->type->stop(dv);
    else
        return 0;
}


/* Clear (destruct) the device. The corresponding constructor is
 * specific to each particular audio system. */

void device_clear(struct device_t *dv)
{
    dv->type->clear(dv);
}


/* Return file descriptors which should be watched for this device.
 * Do not return anything for callback-based audio systems. If this
 * function returns any file descriptors, there must be a handle()
 * function available.
 *
 * Returns the number of pollfd filled, or -1 on error. */

ssize_t device_pollfds(struct device_t *dv, struct pollfd *pe, size_t z)
{
    if (dv->type->pollfds)
        return dv->type->pollfds(dv, pe, z);
    else
        return 0;
}


/* Handle any available input or output on the device. This function
 * is called when there is activity on any fd given by pollfds() for
 * any devices in the system. */

int device_handle(struct device_t *dv)
{
    if (dv->type->handle)
        return dv->type->handle(dv);
    else
        return 0;
}
