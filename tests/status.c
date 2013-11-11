#include <stdio.h>

#include "status.h"

void callback(struct observer *o, void *x)
{
    const char *s = x;

    printf("notify: %s -> %s\n", s, status());
}

int main(int argc, char *argv[])
{
    struct observer o;

    printf("initial: %s\n", status());

    status_set(STATUS_VERBOSE, "lemon");
    printf("%s\n", status());

    status_printf(STATUS_INFO, "%s", "carrot");
    printf("%s\n", status());

    watch(&o, &status_changed, callback);

    status_set(STATUS_ALERT, "apple");
    status_set(STATUS_ALERT, "orange");

    return 0;
}
