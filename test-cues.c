#include <assert.h>
#include <stdio.h>

#include "cues.h"

/*
 * Self-contained test of cue points
 */

int main(int argc, char *argv[])
{
    struct cues_t q;

    cues_init(&q);

    assert(cues_get(&q, 0) == CUE_UNSET);

    cues_set(&q, 0, 100.0);
    assert(cues_get(&q, 0) == 100.0);

    cues_clear(&q);

    return 0;
}
