#include <stdio.h>

#include "observer.h"

void callback(struct observer *t, void *x)
{
    fprintf(stderr, "observer %p fired with argument %p\n", t, x);
}

int main(int argc, char *argv[])
{
    struct event s;
    struct observer t, u;

    event_init(&s);

    watch(&t, &s, callback);
    watch(&u, &s, callback);

    fire(&s, (void*)0xbeef);
    ignore(&u);
    fire(&s, (void*)0xcafe);
    ignore(&t);

    event_clear(&s);

    return 0;
}
