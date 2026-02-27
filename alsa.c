/*
 * Copyright (C) 2026 Mark Hills <mark@xwax.org>
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

#include <limits.h>
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

    unsigned int rate;
};


struct alsa {
    struct alsa_pcm capture, playback;
    snd_pcm_uframes_t buffer, written;
};


static void alsa_error(const char *msg, int r)
{
    fprintf(stderr, "ALSA %s: %s\n", msg, snd_strerror(r));
}


static bool _check(const char *s, int r)
{
    alsa_error(s, r);
    return true;
}


/* Macro complexity is undesirable, but paying the price here makes
 * repeated handling of ALSA errors clear and concise */

#define CHECK(name, r) \
    for (bool _x = false; r < 0 && (_x || _check(name, r)); _x = true)  \
        if (_x) \
            return false; \
        else /* custom failure code run before return */


/*
 * Set ALSA "hardware" parameters
 *
 * These control buffer sizes and other things in the chain
 * beyond this application.
 *
 * A "buffer" of 0 uses the device's maximum.
 */

static bool set_hw(snd_pcm_t *pcm, snd_pcm_stream_t stream,
                   unsigned int *rate,
                   snd_pcm_uframes_t buffer)
{
    int r, dir;
    snd_pcm_hw_params_t *hw;
    snd_pcm_uframes_t frames;

    snd_pcm_hw_params_alloca(&hw);

    r = snd_pcm_hw_params_any(pcm, hw);
    CHECK("hw_params_any", r);

    r = snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_MMAP_INTERLEAVED);
    CHECK("hw_params_set_access", r);

    r = snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16);
    CHECK("hw_params_set_format", r) {
        fprintf(stderr, "16-bit signed format is not available. "
                "You may need to use a 'plughw' device.\n");
    }

    /* Prevent accidentally introducing excess resamplers. There is
     * already one on the signal path to handle pitch adjustments.
     * This is even if a 'plug' device is used, which effectively lets
     * the user unknowingly select any sample rate. */

    r = snd_pcm_hw_params_set_rate_resample(pcm, hw, 0);
    CHECK("hw_params_set_rate_resample", r);

    if (*rate) {
        r = snd_pcm_hw_params_set_rate(pcm, hw, *rate, 0);
        CHECK("hw_params_set_rate", r) {
            fprintf(stderr, "Sample rate of %dHz is not implemented by the hardware.\n",
                    *rate);
        }

    } else {
        /* The 'best' sample rate on this hardware.  Prefer 48kHz over
         * 44.1kHz because it typically allows for smaller buffers.
         * No need to match the source material; it's never playing at
         * a fixed sample rate anyway. */

        dir = -1;
        *rate = 48000;

        r = snd_pcm_hw_params_set_rate_near(pcm, hw, rate, &dir);
        CHECK("hw_params_set_rate_near", r);

        /* "rate" is set on return */
    }

    r = snd_pcm_hw_params_set_channels(pcm, hw, DEVICE_CHANNELS);
    CHECK("hw_params_set_channels", r) {
        fprintf(stderr, "%d channel audio not available on this device.\n",
                DEVICE_CHANNELS);
    }

    /* Declare buffer size first, attempting to ensure it is not
     * constrained by the period size */

    if (!buffer) {
        r = snd_pcm_hw_params_set_buffer_size_last(pcm, hw, &frames);
        CHECK("hw_params_set_buffer_size_last", r);
    } else {
        r = snd_pcm_hw_params_set_buffer_size(pcm, hw, buffer);
        CHECK("hw_params_set_buffer_size", r) {
            fprintf(stderr, "Buffer of %lu samples is probably too small; try increasing it with --buffer\n",
                    buffer);
        }
    }

    /* This is fundamentally a latency-sensitive application that is
     * likely to be the primary application running, so assume we want
     * the hardware to be giving us immediate wakeups */

    r = snd_pcm_hw_params_set_period_size_first(pcm, hw, &frames, &dir);
    CHECK("hw_params_set_buffer_time_near", r);

    r = snd_pcm_hw_params(pcm, hw);
    CHECK("hw_params", r);

    return true;
}


/*
 * Set ALSA's "software" parameters
 *
 * These control when ALSA invokes certain actions, and there are no
 * reasonable defaults documented.
 */

static bool set_sw(snd_pcm_t *pcm)
{
    int r;
    snd_pcm_sw_params_t *sw;

    snd_pcm_sw_params_alloca(&sw);

    r = snd_pcm_sw_params_current(pcm, sw);
    CHECK("sw_params_current", r);

    /*
     * Using the start threshold would be preferred but appears to be
     * ignored when mmap is used.
     *
     * Since the behaviour is not documented, be explicit and make
     * later calls to snd_pcm_start()
     *
     * https://lore.kernel.org/alsa-devel/dbcc61b6-a5be-b2c3-381c-63d5a8a9a843@xwax.org/T/#u
     */

    r = snd_pcm_sw_params_set_start_threshold(pcm, sw, LONG_MAX);
    CHECK("sw_params_set_start_threshold", r);

    r = snd_pcm_sw_params_set_avail_min(pcm, sw, 1);
    CHECK("sw_params_set_avail_min", r);

    r = snd_pcm_sw_params(pcm, sw);
    CHECK("sw_params", r);

    return true;
}


/* "rate" of zero means automatically select an appropriate rate.
 * "buffer" size in frames */

static int pcm_open(struct alsa_pcm *alsa, const char *device_name,
                    snd_pcm_stream_t stream, unsigned int rate, int buffer)
{
    int r;

    r = snd_pcm_open(&alsa->pcm, device_name, stream, SND_PCM_NONBLOCK);
    if (r < 0) {
        alsa_error("open", r);
        return -1;
    }

    alsa->rate = rate;

    if (!set_hw(alsa->pcm, stream, &alsa->rate, buffer))
        return -1;

    if (!set_sw(alsa->pcm))
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
    snd_pcm_uframes_t frames, offset;
    const snd_pcm_channel_area_t *area;
    struct alsa *alsa = (struct alsa*)dv->local;

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

    /* The start threshold is switched off; maintain our
     * own count of when to tell the hardware to start */

    if (alsa->written < alsa->buffer) {
        alsa->written += frames;

        if (alsa->written >= alsa->buffer) {
            r = snd_pcm_start(alsa->playback.pcm);
            if (r < 0)
                return r;
        }
    }

    return 0;
}


/* Pull audio from the device's buffer for capture, and pass it
 * through to the timecoder */

static int capture(struct device *dv)
{
    int r;
    snd_pcm_uframes_t frames, offset;
    const snd_pcm_channel_area_t *area;
    struct alsa *alsa = (struct alsa*)dv->local;

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

                alsa->written = 0;

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

    alsa->buffer = buffer;
    alsa->written = 0;

    if (pcm_open(&alsa->capture, device_name, SND_PCM_STREAM_CAPTURE,
                rate, 0) < 0)
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
