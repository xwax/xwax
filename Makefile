# Copyright (C) 2010 Mark Hills <mark@pogo.org.uk>
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

INSTALL = install

PREFIX = $(HOME)

CFLAGS += -Wall -O3
CPPFLAGS += -MMD
LDFLAGS += -O3

SDL_CFLAGS = `sdl-config --cflags`
SDL_LIBS = `sdl-config --libs` -lSDL_ttf
ALSA_LIBS = -lasound
JACK_LIBS = -ljack

# Import the optional configuration

-include .config

# Installation paths

BINDIR = $(PREFIX)/bin
EXECDIR = $(PREFIX)/libexec
MANDIR = $(PREFIX)/share/man
DOCDIR = $(PREFIX)/share/doc

# Core objects and libraries

OBJS = interface.o library.o listing.o lut.o player.o rig.o \
	selector.o timecoder.o track.o xwax.o
DEVICE_OBJS = device.o
DEVICE_CPPFLAGS =
DEVICE_LIBS =

# Optional device types

ifdef ALSA
DEVICE_OBJS += alsa.o
DEVICE_CPPFLAGS += -DWITH_ALSA
DEVICE_LIBS += $(ALSA_LIBS)
endif

ifdef JACK
DEVICE_OBJS += jack.o
DEVICE_CPPFLAGS += -DWITH_JACK
DEVICE_LIBS += $(JACK_LIBS)
endif

ifdef OSS
DEVICE_OBJS += oss.o
DEVICE_CPPFLAGS += -DWITH_OSS
endif

# Rules

.PHONY:		clean install

xwax:		$(OBJS) $(DEVICE_OBJS)
xwax:		LDLIBS += $(SDL_LIBS) $(DEVICE_LIBS) -lm
xwax:		LDFLAGS += -pthread

interface.o:	CFLAGS += $(SDL_CFLAGS)

xwax.o:		CFLAGS += $(SDL_CFLAGS) $(DEVICE_CPPFLAGS)
xwax.o:		CPPFLAGS += -DEXECDIR=\"$(EXECDIR)\"

install:
		$(INSTALL) -d $(BINDIR)
		$(INSTALL) xwax $(BINDIR)/xwax
		$(INSTALL) -d $(EXECDIR)
		$(INSTALL) scan $(EXECDIR)/xwax-scan
		$(INSTALL) import $(EXECDIR)/xwax-import
		$(INSTALL) -d $(MANDIR)/man1
		$(INSTALL) -m 0644 xwax.1 $(MANDIR)/man1/xwax.1
		$(INSTALL) -d $(DOCDIR)/xwax
		$(INSTALL) -m 0644 CHANGES $(DOCDIR)/xwax/CHANGES
		$(INSTALL) -m 0644 COPYING $(DOCDIR)/xwax/COPYING
		$(INSTALL) -m 0644 README $(DOCDIR)/xwax/README


clean:
		rm -f xwax *.o *.d *~

-include *.d
