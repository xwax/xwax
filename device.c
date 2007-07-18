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


#define PLAYBACK_BUFFER 4096

static void alsa_error(int r)
{
    fputs("ALSA: ", stderr);
    fputs(snd_strerror(r), stderr);
    fputc('\n', stderr);
}


static int alsa_open(snd_pcm_t **snd, const char *device_name,
                     snd_pcm_stream_t stream)
{
    int p, r, dir;
    snd_pcm_hw_params_t *hw_params;
    
    r = snd_pcm_open(snd, device_name, stream, SND_PCM_NONBLOCK);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }
    
    snd_pcm_hw_params_malloc(&hw_params);
    
    r = snd_pcm_hw_params_any(*snd, hw_params);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }

    r = snd_pcm_hw_params_set_access(*snd, hw_params,
                                     SND_PCM_ACCESS_RW_INTERLEAVED);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }
    
    r = snd_pcm_hw_params_set_format(*snd, hw_params,
                                     SND_PCM_FORMAT_S16_LE);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }

    r = snd_pcm_hw_params_set_rate(*snd, hw_params, DEVICE_RATE, 0);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }
    
    r = snd_pcm_hw_params_set_channels(*snd, hw_params, DEVICE_CHANNELS);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }
    
    if(stream == SND_PCM_STREAM_PLAYBACK) {
        
        p = PLAYBACK_BUFFER; /* microseconds */
        dir = 0;
        r = snd_pcm_hw_params_set_buffer_time_near(*snd, hw_params, &p, &dir);
        if(r < 0) {
            alsa_error(r);
            return -1;
        }
        
        fprintf(stderr, "Buffer time is %d\n", p);
    }
    
    r = snd_pcm_hw_params(*snd, hw_params);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }

    snd_pcm_hw_params_free(hw_params);
    
    r = snd_pcm_prepare(*snd);
    if(r < 0) {
        alsa_error(r);
        return -1;
    }

    return 0;
}


/* Open ALSA device. Do not operate on audio until device_start() */

int device_open(struct device_t *dv, const char *device_name,
                unsigned short buffers, unsigned short fragment)
{
    fprintf(stderr, "Opening ALSA device '%s'...\n", device_name);

    if(alsa_open(&dv->snd_cap, device_name, SND_PCM_STREAM_CAPTURE) < 0) {
        fputs("Failed to open device for capture.\n", stderr);
        return -1;
    }

    if(alsa_open(&dv->snd_play, device_name, SND_PCM_STREAM_PLAYBACK) < 0) {
        fputs("Failed to open device for playback.\n", stderr);
        return -1;
    }

    fputs("Device opened successfully.\n", stderr);

    return 0;
}


/* Start the audio buffers */

int device_start(struct device_t *dv)
{
    snd_pcm_start(dv->snd_cap);
    snd_pcm_start(dv->snd_play);
    return 0;
}


/* Close ALSA device */

int device_close(struct device_t *dv)
{
    snd_pcm_close(dv->snd_cap);
    snd_pcm_close(dv->snd_play);
    return 0;
}


/* Register this device's interest in a set of pollfd file
 * descriptors */

int device_fill_pollfd(struct device_t *dv, struct pollfd *pe, int pe_size)
{
    int n, r;
    
    /* File descriptors for capture device */
    
    n = snd_pcm_poll_descriptors_count(dv->snd_cap);

    if(n > pe_size)
        return -1;
    
    if(n > 0) {
        r = snd_pcm_poll_descriptors(dv->snd_cap, pe, n);
        
        if(r < 0) {
            alsa_error(r);
            return -1;
        }
        
        dv->pe_cap = pe;
    }

    dv->pn_cap = n;
    
    pe += n;
    pe_size -= n;

    /* File descriptors for playback device */
    
    n = snd_pcm_poll_descriptors_count(dv->snd_play);
    
    if(n > pe_size)
        return -1;
    
    if(n > 0) {
        r = snd_pcm_poll_descriptors(dv->snd_play, pe, n);

        if(r < 0) {
            alsa_error(r);
            return -1;
        }

        dv->pe_play = pe;
    }

    dv->pn_play = n;
    
    return dv->pn_cap + dv->pn_play;
}
    

/* Collect audio from the player and push it into the device's buffer,
 * for playback */

static int playback(struct device_t *dv)
{
    int r, samples;
    signed short pcm[DEVICE_FRAME * DEVICE_CHANNELS];

    /* Always push some audio to the soundcard, even if it means
     * silence. This has shown itself to be much more reliable than
     * constantly starting and stopping -- which can affect other
     * devices to the one which is doing the stopping. */
    
    r = snd_pcm_avail_update(dv->snd_play);
    
    if(r < 0)
        return r;

    if(r == 0) {
        fprintf(stderr, "device_push: premature.\n");
        return 0;
    }

    samples = r;
    
    if(samples > DEVICE_FRAME)
        samples = DEVICE_FRAME;
    
    if(dv->player && dv->player->playing)
        player_collect(dv->player, pcm, samples);
    else
        memset(pcm, 0, samples * DEVICE_CHANNELS * sizeof(short));
    
    r = snd_pcm_writei(dv->snd_play, pcm, samples);
    
    if(r < 0)
        return r;
        
    if(r < samples)
        fprintf(stderr, "device_push: underrun %d/%d.\n", r, samples);

    return 0;
}


/* Pull audio from the device's buffer for capture, and pass it
 * through to the timecoder */

static int capture(struct device_t *dv)
{
    int r, samples;
    signed short pcm[DEVICE_FRAME * DEVICE_CHANNELS];

    r = snd_pcm_avail_update(dv->snd_cap);
    
    if(r < 0)
        return r;

    if(r == 0) {
        fprintf(stderr, "device_pull: premature.\n");
        return 0;
    }

    samples = r;
            
    if(samples > DEVICE_FRAME)
        samples = DEVICE_FRAME;
                
    r = snd_pcm_readi(dv->snd_cap, pcm, samples);

    if(r < 0)
        return r;

    if(r < samples)
        fprintf(stderr, "device_pull: underrun %d/%d.\n", r, samples);

    samples = r;
    
    if(dv->timecoder) {
        timecoder_submit(dv->timecoder, pcm, samples);
        
        if(dv->player)
            player_sync(dv->player);
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
    
    r = snd_pcm_poll_descriptors_revents(dv->snd_cap, dv->pe_cap,
                                         dv->pn_cap, &revents);
    
    if(r < 0) {
        alsa_error(r);
        return -1;
    }
    
    if(revents & POLLIN) { 
        r = capture(dv);
        
        if(r < 0) {
            if(r == -EPIPE) {
                fprintf(stderr, "device_handle: capture xrun.\n");
                snd_pcm_prepare(dv->snd_cap);
            } else
                return -1;
        } 
    }
    
    /* Check the output buffer for playback */
    
    snd_pcm_poll_descriptors_revents(dv->snd_play, dv->pe_play,
                                     dv->pn_play, &revents);

    if(r < 0) {
        alsa_error(r);
        return -1;
    }
    
    if(revents & POLLOUT) {
        r = playback(dv);
        
        if(r < 0) {
            if(r == -EPIPE) {
                fprintf(stderr, "device_handle: playback xrun.\n");
                snd_pcm_prepare(dv->snd_play);
            } else
                return -1;
        }
    }

    return 0;
}
