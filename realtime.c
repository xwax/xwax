#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "device.h"
#include "realtime.h"

#define REALTIME_PRIORITY 80

/*
 * Raise the priority of the current thread
 *
 * Return: -1 if priority could not be satisfactorily raised, otherwise 0
 */

static int raise_priority()
{
    int max_pri;
    struct sched_param sp;

    fprintf(stderr, "Setting scheduler priority...\n");

    if (sched_getparam(0, &sp)) {
        perror("sched_getparam");
        return -1;
    }

    max_pri = sched_get_priority_max(SCHED_FIFO);
    sp.sched_priority = REALTIME_PRIORITY;

    if (sp.sched_priority > max_pri) {
        fprintf(stderr, "Invalid scheduling priority (maximum %d).\n", max_pri);
        return -1;
    }

    if (sched_setscheduler(0, SCHED_FIFO, &sp)) {
        perror("sched_setscheduler");
        fprintf(stderr, "Failed to set scheduler. Run as root otherwise you "
                "may get wow and skips!\n");
    }

    return 0;
}

/*
 * The realtime thread
 */

static void rt_main(struct rt_t *rt)
{
    int r;
    size_t n;

    if (raise_priority() == -1)
        return;

    while (!rt->finished) {
        r = poll(rt->pt, rt->npt, -1);
        if (r == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                perror("poll");
                return;
            }
        }

        for (n = 0; n < rt->ndv; n++)
            device_handle(rt->dv[n]);
    }
}

static void* launch(void *p)
{
    rt_main(p);
    return NULL;
}

/*
 * Start realtime handling of the given devices
 *
 * This forks the realtime thread if it is required (eg. ALSA). Some
 * devices (eg. JACK) start their own thread.
 */

int rt_start(struct rt_t *rt, struct device_t *dv, size_t ndv)
{
    size_t n;

    rt->finished = false;
    rt->ndv = 0;
    rt->npt = 0;

    /* The requested poll events never change, so populate the poll
     * entry table before entering the realtime thread */

    for (n = 0; n < ndv; n++) {
        ssize_t z;

        z = device_pollfds(&dv[n], &rt->pt[rt->npt],
                           MAX_DEVICE_POLLFDS - rt->npt);
        if (z == -1) {
            fprintf(stderr, "Device failed to return file descriptors.\n");
            return -1;
        }

        rt->npt += z;

        rt->dv[rt->ndv] = &dv[n];
        rt->ndv++;
    }

    /* If there are any devices which returned file descriptors for
     * poll() then launch the realtime thread to handle them */

    if (rt->npt > 0) {
        fprintf(stderr, "Launching realtime thread to handle devices...\n");

        if (pthread_create(&rt->ph, NULL, launch, (void*)rt)) {
            perror("pthread_create");
            return -1;
        }
    }

    /* FIXME: To avoid audio drop on startup, devices should be
     * started after synchronising with the realtime thread */

    for (n = 0; n < ndv; n++)
        device_start(&dv[n]);

    return 0;
}

void rt_stop(struct rt_t *rt)
{
    size_t n;

    rt->finished = true;

    if (rt->npt > 0) {
        if (pthread_join(rt->ph, NULL) != 0)
            abort();
    }

    /* Stop audio rolling on devices */

    for (n = 0; n < rt->ndv; n++)
        device_stop(rt->dv[n]);
}
