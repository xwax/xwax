/*
 * Copyright (C) 2007 Mark Hills <mark@pogo.org.uk>
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
#include <string.h>
#include <sys/poll.h>
#include <alsa/asoundlib.h>

#include "device.h"
#include "timecoder.h"
#include "player.h"


#define PLAYBACK_BUFFER 4000 /* microseconds */

static void alsa_error(int r)
{
    fputs("ALSA: ", stderr);
    fputs(snd_strerror(r), stderr);
    fputc('\n', stderr);
}


static int alsa_open(struct alsa_pcm_t *alsa, const char *device_name,
                     snd_pcm_stream_t stream)
{
    int r, p, dir;
    snd_pcm_hw_params_t *hw_params;
    
    r = snd_pcm_open(&alsa->pcm, device_name, stream, SND_PCM_NONBLOCK);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }
    
    snd_pcm_hw_params_malloc(&hw_params);
    
    r = snd_pcm_hw_params_any(alsa->pcm, hw_params);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }
    
    r = snd_pcm_hw_params_set_access(alsa->pcm, hw_params,
                                     SND_PCM_ACCESS_RW_INTERLEAVED);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }
    
    r = snd_pcm_hw_params_set_format(alsa->pcm, hw_params,
                                     SND_PCM_FORMAT_S16_LE);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }

    r = snd_pcm_hw_params_set_rate(alsa->pcm, hw_params, DEVICE_RATE, 0);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }
    
    r = snd_pcm_hw_params_set_channels(alsa->pcm, hw_params, DEVICE_CHANNELS);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }

    p = PLAYBACK_BUFFER; /* microseconds */
    dir = 1;
    r = snd_pcm_hw_params_set_buffer_time_near(alsa->pcm, hw_params, &p, &dir);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }
    
    fprintf(stderr, "Buffer time is %d\n", p);

    r = snd_pcm_hw_params(alsa->pcm, hw_params);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }
    
    dir = 0;
    r = snd_pcm_hw_params_get_period_size(hw_params, &alsa->period, &dir);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }
    fprintf(stderr, "Period size is %ld\n", alsa->period);

    snd_pcm_hw_params_free(hw_params);
    
    r = snd_pcm_prepare(alsa->pcm);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }

    alsa->buf = malloc(alsa->period * DEVICE_CHANNELS * sizeof(signed short));
    if(!alsa->buf) {
        perror("malloc");
        return -1;
    }

    return 0;
}


/* Open ALSA device. Do not operate on audio until device_start() */

int device_open(struct device_t *dv, const char *device_name,
                unsigned short buffers, unsigned short fragment)
{
    fprintf(stderr, "Opening ALSA device '%s'...\n", device_name);

    if(alsa_open(&dv->capture, device_name, SND_PCM_STREAM_CAPTURE) < 0) {
        fputs("Failed to open device for capture.\n", stderr);
        return -1;
    }
    
    if(alsa_open(&dv->playback, device_name, SND_PCM_STREAM_PLAYBACK) < 0) {
        fputs("Failed to open device for playback.\n", stderr);
        return -1;
    }

    fputs("Device opened successfully.\n", stderr);

    return 0;
}


/* Start the audio buffers */

int device_start(struct device_t *dv)
{
    snd_pcm_start(dv->capture.pcm);
    snd_pcm_start(dv->playback.pcm);
    return 0;
}


/* Close ALSA device */

int device_close(struct device_t *dv)
{
    snd_pcm_close(dv->capture.pcm);
    snd_pcm_close(dv->playback.pcm);
    return 0;
}


static int alsa_fill_pollfd(struct alsa_pcm_t *alsa, struct pollfd *pe, int n)
{
    int r, count;

    count = snd_pcm_poll_descriptors_count(alsa->pcm);
    if(count > n)
        return -1;

    if(count == 0) 
        alsa->pe = NULL;
    else {
        r = snd_pcm_poll_descriptors(alsa->pcm, pe, count);
        
        if(r < 0) {
            alsa_error(r);
            return -1;
        }

        alsa->pe = pe;
    }

    alsa->pe_count = count;
    return count;
}


/* Register this device's interest in a set of pollfd file
 * descriptors */

int device_fill_pollfd(struct device_t *dv, struct pollfd *pe, int pe_size)
{
    int total, r;

    total = 0;

    r = alsa_fill_pollfd(&dv->capture, pe, pe_size);
    if(r < 0)
        return -1;
    
    pe += r;
    pe_size -= r;
    total += r;
    
    r = alsa_fill_pollfd(&dv->playback, pe, pe_size);
    if(r < 0)
        return -1;
    
    total += r;
    
    return total;
}
    

/* Collect audio from the player and push it into the device's buffer,
 * for playback */

static int playback(struct device_t *dv)
{
    int r;

    /* Always push some audio to the soundcard, even if it means
     * silence. This has shown itself to be much more reliable than
     * constantly starting and stopping -- which can affect other
     * devices to the one which is doing the stopping. */
    
    if(dv->player && dv->player->playing)
        player_collect(dv->player, dv->playback.buf, dv->playback.period);
    else {
        memset(dv->playback.buf, 0,
               dv->playback.period * DEVICE_CHANNELS * sizeof(short));
    }    

    r = snd_pcm_writei(dv->playback.pcm, dv->playback.buf,
                       dv->playback.period);
    if(r < 0)
        return r;
        
    if(r < dv->playback.period)
        fprintf(stderr, "device_push: underrun %d/%ld.\n", r,
                dv->playback.period);

    return 0;
}


/* Pull audio from the device's buffer for capture, and pass it
 * through to the timecoder */

static int capture(struct device_t *dv)
{
    int r;

    r = snd_pcm_readi(dv->capture.pcm, dv->capture.buf, dv->capture.period);
    if(r < 0)
        return r;
    
    if(r < dv->capture.period) {
        fprintf(stderr, "device_pull: underrun %d/%ld.\n",
                r, dv->capture.period);
    }
    
    if(dv->timecoder) {
        timecoder_submit(dv->timecoder, dv->capture.buf, r);
        
        if(dv->player)
            player_sync(dv->player);
    }

    return 0;
}


static int alsa_revents(struct alsa_pcm_t *alsa, unsigned short *revents) {
    int r;

    r = snd_pcm_poll_descriptors_revents(alsa->pcm, alsa->pe, alsa->pe_count,
                                         revents);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }
    
    return 0;
}


/* After poll() has returned, instruct a device to do all it can at
 * the present time. Return zero if success, otherwise -1 */

int device_handle(struct device_t *dv)
{
    int r;
    unsigned short revents;
    
    /* Check input buffer for timecode capture */
    
    r = alsa_revents(&dv->capture, &revents);
    if(r < 0)
        return -1;
    
    if(revents & POLLIN) { 
        r = capture(dv);
        
        if(r < 0) {
            if(r == -EPIPE) {
                fprintf(stderr, "device_handle: capture xrun.\n");
                snd_pcm_prepare(dv->capture.pcm);
            } else
                return -1;
        } 
    }
    
    /* Check the output buffer for playback */
    
    r = alsa_revents(&dv->playback, &revents);
    if(r < 0)
        return -1;
    
    if(revents & POLLOUT) {
        r = playback(dv);
        
        if(r < 0) {
            if(r == -EPIPE) {
                fprintf(stderr, "device_handle: playback xrun.\n");
                snd_pcm_prepare(dv->playback.pcm);
            } else
                return -1;
        }
    }

    return 0;
}
