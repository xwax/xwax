# Copyright (C) 2007 Mark Hills <mark@pogo.org.uk>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2, as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License version 2 for more details.
# 
# You should have received a copy of the GNU General Public License
# version 2 along with this program; if not, write to the Free
# Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA 02110-1301, USA.
#

CFLAGS += -Wall -O3

SDL_CFLAGS = `sdl-config --cflags`
SDL_LIBS = `sdl-config --libs` -lSDL_ttf

.PHONY:		clean depend

xwax:		device.o interface.o library.o oss.o player.o rig.o \
		timecoder.o track.o xwax.o
		$(CC) $(CFLAGS) -o $@ $^ -pthread $(SDL_LIBS)

interface.o:	CFLAGS += $(SDL_CFLAGS)

depend:		device.c interface.c library.c player.c rig.c timecoder.c \
		track.c xwax.c
		$(CC) -MM $^ > .depend

clean:
		rm -f .depend xwax *.o *~

-include .depend
