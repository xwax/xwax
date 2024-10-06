/*
 * Copyright (C) 2024 Mark Hills <mark@xwax.org>
 *
 * This file is part of "xwax".
 *
 * "xwax" is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * "xwax" is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>

#include "alsa.h"


/* This structure doesn't have corresponding functions to be an
 * abstraction of the ALSA calls; it is merely a container for these
 * variables. */

struct alsa_pcm {
    snd_pcm_t *pcm;

    struct pollfd *pe;
    size_t pe_count; /* number of pollfd entries */

    int rate;
};


struct alsa {
    struct alsa_pcm capture, playback;
    bool playing;
};


static void alsa_error(const char *msg, int r)
{
    fprintf(stderr, "ALSA %s: %s\n", msg, snd_strerror(r));
}


static bool chk(const char *s, int r)
{
    if (r < 0) {
        alsa_error(s, r);
        return false;
    } else {
        return true;
    }
}


/* "rate" of zero means automatically select an appropriate rate */

static int pcm_open(struct alsa_pcm *alsa, const char *device_name,
                    snd_pcm_stream_t stream, unsigned int rate, int buffer)
{
    int r, dir;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_uframes_t frames;

    r = snd_pcm_open(&alsa->pcm, device_name, stream, SND_PCM_NONBLOCK);
    if (!chk("open", r))
        return -1;

    snd_pcm_hw_params_alloca(&hw_params);

    r = snd_pcm_hw_params_any(alsa->pcm, hw_params);
    if (!chk("hw_params_any", r))
        return -1;

    r = snd_pcm_hw_params_set_access(alsa->pcm, hw_params,
                                     SND_PCM_ACCESS_MMAP_INTERLEAVED);
    if (!chk("hw_params_set_access", r))
        return -1;

    r = snd_pcm_hw_params_set_format(alsa->pcm, hw_params, SND_PCM_FORMAT_S16);
    if (!chk("hw_params_set_format", r)) {
        fprintf(stderr, "16-bit signed format is not available. "
                "You may need to use a 'plughw' device.\n");
        return -1;
    }

    /* Prevent accidentally introducing excess resamplers. There is
     * already one on the signal path to handle pitch adjustments.
     * This is even if a 'plug' device is used, which effectively lets
     * the user unknowingly select any sample rate. */

    r = snd_pcm_hw_params_set_rate_resample(alsa->pcm, hw_params, 0);
    if (!chk("hw_params_set_rate_resample", r))
        return -1;

    if (rate) {
        r = snd_pcm_hw_params_set_rate(alsa->pcm, hw_params, rate, 0);
        if (!chk("hw_params_set_rate", r)) {
            fprintf(stderr, "Sample rate of %dHz is not implemented by the hardware.\n",
                    rate);
            return -1;
        }

    } else {
        /* The 'best' sample rate on this hardware.  Prefer 48kHz over
         * 44.1kHz because it typically allows for smaller buffers.
         * No need to match the source material; it's never playing at
         * a fixed sample rate anyway. */

        dir = -1;
        rate = 48000;

        r = snd_pcm_hw_params_set_rate_near(alsa->pcm, hw_params, &rate, &dir);
        if (!chk("hw_params_set_rate_near", r))
            return -1;

        /* "rate" is set on return */
    }

    alsa->rate = rate;

    r = snd_pcm_hw_params_set_channels(alsa->pcm, hw_params, DEVICE_CHANNELS);
    if (!chk("hw_params_set_channels", r)) {
        fprintf(stderr, "%d channel audio not available on this device.\n",
                DEVICE_CHANNELS);
        return -1;
    }

    /* This is fundamentally a latency-sensitive application that is
     * likely to be the primary application running, so assume we want
     * the hardware to be giving us immediate wakeups */

    r = snd_pcm_hw_params_set_period_size_first(alsa->pcm, hw_params, &frames, &dir);
    if (!chk("hw_params_set_buffer_time_near", r))
        return -1;

    switch (stream) {
    case SND_PCM_STREAM_CAPTURE:
        /* Maximum buffer to minimise drops */
        r = snd_pcm_hw_params_set_buffer_size_last(alsa->pcm, hw_params, &frames);
        if (!chk("hw_params_set_buffer_size_last", r))
            return -1;
        break;

    case SND_PCM_STREAM_PLAYBACK:
        /* Smallest possible buffer to keep latencies low */
        r = snd_pcm_hw_params_set_buffer_size(alsa->pcm, hw_params, buffer);
        if (!chk("hw_params_set_buffer_size", r)) {
            fprintf(stderr, "Buffer of %u samples is probably too small; try increasing it with -m\n",
                    buffer);
            return -1;
        }
        break;

    default:
        abort();
    }

    r = snd_pcm_hw_params(alsa->pcm, hw_params);
    if (!chk("hw_params", r))
        return -1;

    return 0;
}


static void pcm_close(struct alsa_pcm *alsa)
{
    if (snd_pcm_close(alsa->pcm) < 0)
        abort();
}


static ssize_t pcm_pollfds(struct alsa_pcm *alsa, struct pollfd *pe,
			   size_t z)
{
    int r, count;

    count = snd_pcm_poll_descriptors_count(alsa->pcm);
    if (count > z)
        return -1;

    if (count == 0)
        alsa->pe = NULL;
    else {
        r = snd_pcm_poll_descriptors(alsa->pcm, pe, count);
        if (r < 0) {
            alsa_error("poll_descriptors", r);
            return -1;
        }
        alsa->pe = pe;
    }

    alsa->pe_count = count;
    return count;
}


static int pcm_revents(struct alsa_pcm *alsa, unsigned short *revents) {
    int r;

    r = snd_pcm_poll_descriptors_revents(alsa->pcm, alsa->pe, alsa->pe_count,
                                         revents);
    if (r < 0) {
        alsa_error("poll_descriptors_revents", r);
        return -1;
    }

    return 0;
}



/* Start the audio device capture and playback */

static void start(struct device *dv)
{
    struct alsa *alsa = (struct alsa*)dv->local;

    if (snd_pcm_start(alsa->capture.pcm) < 0)
        abort();
}


/* Register this device's interest in a set of pollfd file
 * descriptors */

static ssize_t pollfds(struct device *dv, struct pollfd *pe, size_t z)
{
    int total, r;
    struct alsa *alsa = (struct alsa*)dv->local;

    total = 0;

    r = pcm_pollfds(&alsa->capture, pe, z);
    if (r < 0)
        return -1;

    pe += r;
    z -= r;
    total += r;

    r = pcm_pollfds(&alsa->playback, pe, z);
    if (r < 0)
        return -1;

    total += r;

    return total;
}


/* Access the interleaved area presented by the ALSA library.  The
 * device is opened SND_PCM_FORMAT_S16 which is in the local endianess
 * and therefore is "signed short" */

static signed short *buffer(const snd_pcm_channel_area_t *area,
                          snd_pcm_uframes_t offset)
{
    assert(area->first % 8 == 0);
    assert(area->step == 32);  /* 2 channel 16-bit interleaved */

    return area->addr + area->first / 8 + offset * area->step / 8;
}


/* Collect audio from the player and push it into the device's buffer,
 * for playback */

static int playback(struct device *dv)
{
    int r;
    snd_pcm_state_t state;
    snd_pcm_uframes_t frames, offset;
    const snd_pcm_channel_area_t *area;
    struct alsa *alsa = (struct alsa*)dv->local;

    state = snd_pcm_state(alsa->capture.pcm);
    if (state == SND_PCM_STATE_XRUN)
        return -EPIPE;

    frames = snd_pcm_avail_update(alsa->playback.pcm);
    if (frames < 0)
        return (int)frames;

    r = snd_pcm_mmap_begin(alsa->playback.pcm, &area, &offset, &frames);
    if (r < 0)
        return r;

    assert(frames > 0);  /* otherwise we were woken unnecessarily */

    device_collect(dv, buffer(&area[0], offset), frames);

    r = snd_pcm_mmap_commit(alsa->playback.pcm, offset, frames);
    if (r < 0)
        return r;

    /* If this is the initial write, assume the buffer gets filled to
     * the maximum and it's time to consume the buffer */

    if (!alsa->playing) {
        r = snd_pcm_start(alsa->playback.pcm);
        if (r < 0)
            return r;

        alsa->playing = true;
    }

    return 0;
}


/* Pull audio from the device's buffer for capture, and pass it
 * through to the timecoder */

static int capture(struct device *dv)
{
    int r;
    snd_pcm_state_t state;
    snd_pcm_uframes_t frames, offset;
    const snd_pcm_channel_area_t *area;
    struct alsa *alsa = (struct alsa*)dv->local;

    state = snd_pcm_state(alsa->capture.pcm);
    if (state == SND_PCM_STATE_XRUN)
        return -EPIPE;

    frames = snd_pcm_avail(alsa->capture.pcm);
    if (frames < 0)
        return (int)frames;

    r = snd_pcm_mmap_begin(alsa->capture.pcm, &area, &offset, &frames);
    if (r < 0)
        return r;

    assert(frames > 0);  /* otherwise we were woken unnecessarily */

    device_submit(dv, buffer(&area[0], offset), frames);

    r = snd_pcm_mmap_commit(alsa->capture.pcm, offset, frames);
    if (r < 0)
        return r;

    return 0;
}


/* After poll() has returned, instruct a device to do all it can at
 * the present time. Return zero if success, otherwise -1 */

static int handle(struct device *dv)
{
    int r;
    unsigned short revents;
    struct alsa *alsa = (struct alsa*)dv->local;

    /* Check input buffer for timecode capture */

    r = pcm_revents(&alsa->capture, &revents);
    if (r < 0)
        return -1;

    if (revents & POLLIN) {
        r = capture(dv);

        if (r < 0) {
            if (r == -EPIPE) {
                fputs("ALSA: capture xrun.\n", stderr);

                r = snd_pcm_prepare(alsa->capture.pcm);
                if (r < 0) {
                    alsa_error("prepare", r);
                    return -1;
                }

                r = snd_pcm_start(alsa->capture.pcm);
                if (r < 0) {
                    alsa_error("start", r);
                    return -1;
                }

            } else {
                alsa_error("capture", r);
                return -1;
            }
        }
    }

    /* Check the output buffer for playback */

    r = pcm_revents(&alsa->playback, &revents);
    if (r < 0)
        return -1;

    if (revents & POLLOUT) {
        r = playback(dv);

        if (r < 0) {
            if (r == -EPIPE) {
                fputs("ALSA: playback xrun.\n", stderr);

                r = snd_pcm_prepare(alsa->playback.pcm);
                if (r < 0) {
                    alsa_error("prepare", r);
                    return -1;
                }

                alsa->playing = false;

                /* POLLOUT events will be generated now, and we
                 * explicitly start the device when writing */

            } else {
                alsa_error("playback", r);
                return -1;
            }
        }
    }

    return 0;
}


static unsigned int sample_rate(struct device *dv)
{
    struct alsa *alsa = (struct alsa*)dv->local;

    return alsa->capture.rate;
}


/* Close ALSA device and clear any allocations */

static void clear(struct device *dv)
{
    struct alsa *alsa = (struct alsa*)dv->local;

    pcm_close(&alsa->capture);
    pcm_close(&alsa->playback);
    free(dv->local);
}


static struct device_ops alsa_ops = {
    .pollfds = pollfds,
    .handle = handle,
    .sample_rate = sample_rate,
    .start = start,
    .clear = clear
};


/* Open ALSA device. Do not operate on audio until device_start() */

int alsa_init(struct device *dv, const char *device_name,
              unsigned int rate, unsigned int buffer)
{
    struct alsa *alsa;

    alsa = malloc(sizeof *alsa);
    if (alsa == NULL) {
        perror("malloc");
        return -1;
    }

    alsa->playing = false;

    if (pcm_open(&alsa->capture, device_name, SND_PCM_STREAM_CAPTURE,
                rate, buffer) < 0)
    {
        fputs("Failed to open device for capture.\n", stderr);
        goto fail;
    }

    if (pcm_open(&alsa->playback, device_name, SND_PCM_STREAM_PLAYBACK,
                rate, buffer) < 0)
    {
        fputs("Failed to open device for playback.\n", stderr);
        goto fail_capture;
    }

    device_init(dv, &alsa_ops);
    dv->local = alsa;

    return 0;

 fail_capture:
    pcm_close(&alsa->capture);
 fail:
    free(alsa);
    return -1;
}


/* ALSA caches information when devices are open. Provide a call
 * to clear these caches so that valgrind output is clean. */

void alsa_clear_config_cache(void)
{
    int r;

    r = snd_config_update_free_global();
    if (r < 0)
        alsa_error("config_update_free_global", r);
}
