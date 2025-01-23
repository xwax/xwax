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
