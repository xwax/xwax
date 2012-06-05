#include <stdio.h>

#include "status.h"

void notify(void)
{
    printf("notify: %s\n", status());
}

int main(int argc, char *argv[])
{
    printf("initial: %s\n", status());

    status_set(STATUS_VERBOSE, "lemon");
    printf("%s\n", status());

    status_printf(STATUS_INFO, "%s", "carrot");
    printf("%s\n", status());

    status_notify(notify);

    status_set(STATUS_ERROR, "apple");
    status_set(STATUS_ERROR, "orange");

    return 0;
}
