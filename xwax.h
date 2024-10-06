/*
 * Copyright (C) 2024 Mark Hills <mark@xwax.org>
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

#ifndef XWAX_H
#define XWAX_H

#include "deck.h"

extern char *banner;

#define NOTICE \
  "This software is supplied WITHOUT ANY WARRANTY; without even the implied\n"\
  "warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. This is\n"\
  "free software, and you are welcome to redistribute it under certain\n"\
  "conditions; see the file COPYING for details."

extern size_t ndeck;
extern struct deck deck[];

#endif
