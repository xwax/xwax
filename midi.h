#ifndef MIDI_H
#define MIDI_H

#include <alsa/asoundlib.h>

/*
 * State information for an open, non-blocking MIDI device
 */

struct midi_t {
    snd_rawmidi_t *in, *out;
};

int midi_open(struct midi_t *m, const char *name);
void midi_close(struct midi_t *m);

ssize_t midi_pollfds(struct midi_t *m, struct pollfd *pe, size_t len);
ssize_t midi_read(struct midi_t *m, void *buf, size_t len);

#endif
