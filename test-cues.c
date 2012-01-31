#include <assert.h>
#include <stdio.h>

#include "cues.h"

/*
 * Self-contained test of cue points
 */

int main(int argc, char *argv[])
{
    struct cues q;

    cues_reset(&q);

    assert(cues_get(&q, 0) == CUE_UNSET);

    cues_set(&q, 0, 100.0);
    assert(cues_get(&q, 0) == 100.0);

    return 0;
}
