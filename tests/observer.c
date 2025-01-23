/*
 * Copyright (C) 2025 Mark Hills <mark@xwax.org>
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
