#ifndef EXCRATE_H
#define EXCRATE_H

#include <sys/poll.h>
#include <sys/types.h>

#include "external.h"
#include "list.h"
#include "library.h"

struct excrate {
    struct list excrates;
    unsigned int refcount;
    struct listing listing;

    /* State of the external scan process */

    struct list rig;
    pid_t pid;
    int fd;
    struct pollfd *pe;

    /* State of reader */

    struct rb rb;
};

struct excrate* excrate_acquire_by_scan(const char *script, const char *search);

void excrate_acquire(struct excrate *e);
void excrate_release(struct excrate *e);

/* Used by the rig and main thread */

void excrate_pollfd(struct excrate *tr, struct pollfd *pe);
void excrate_handle(struct excrate *tr);

#endif
