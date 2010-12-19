#ifndef REALTIME_H
#define REALTIME_H

#include <stdbool.h>

#define MAX_DEVICES 4
#define MAX_DEVICE_POLLFDS 32

/*
 * State data for the realtime thread, maintained during rt_start and
 * rt_stop
 */

struct rt_t {
    pthread_t ph;
    bool finished;

    size_t ndv;
    struct device_t *dv[MAX_DEVICES];

    size_t npt;
    struct pollfd pt[MAX_DEVICE_POLLFDS];
};

int rt_start(struct rt_t *rt, struct device_t *dv, size_t ndv);
void rt_stop(struct rt_t *rt);

#endif
