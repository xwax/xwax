# Copyright (C) 2012 Mark Hills <mark@pogo.org.uk>
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

# Import the optional configuration

-include .config

# Libraries and dependencies

INSTALL ?= install

SDL_CFLAGS ?= `sdl-config --cflags`
SDL_LIBS ?= `sdl-config --libs` -lSDL_ttf
ALSA_LIBS ?= -lasound
JACK_LIBS ?= -ljack

# Installation paths

PREFIX ?= $(HOME)

BINDIR ?= $(PREFIX)/bin
EXECDIR ?= $(PREFIX)/libexec
MANDIR ?= $(PREFIX)/share/man
DOCDIR ?= $(PREFIX)/share/doc

# Build flags

CFLAGS += -Wall -O3
CPPFLAGS += -MMD
LDFLAGS += -O3

# Core objects and libraries

OBJS = controller.o cues.o deck.o device.o external.o interface.o \
	library.o listing.o lut.o \
	player.o realtime.o \
	rig.o selector.o status.o thread.o timecoder.o track.o xwax.o
DEVICE_CPPFLAGS =
DEVICE_LIBS =

# Optional device types

ifdef ALSA
OBJS += alsa.o dicer.o midi.o
DEVICE_CPPFLAGS += -DWITH_ALSA
DEVICE_LIBS += $(ALSA_LIBS)
endif

ifdef JACK
OBJS += jack.o
DEVICE_CPPFLAGS += -DWITH_JACK
DEVICE_LIBS += $(JACK_LIBS)
endif

ifdef OSS
OBJS += oss.o
DEVICE_CPPFLAGS += -DWITH_OSS
endif

DEPS = $(OBJS:.o=.d)

# Rules

.PHONY:		all
all:		xwax tests

# Dynamic versioning

.PHONY:		FORCE
.version:	FORCE
		./mkversion -r

VERSION = $(shell ./mkversion)

# Main binary

xwax:		$(OBJS)
xwax:		LDLIBS += $(SDL_LIBS) $(DEVICE_LIBS) -lm
xwax:		LDFLAGS += -pthread

interface.o:	CFLAGS += $(SDL_CFLAGS)

xwax.o:		CFLAGS += $(SDL_CFLAGS)
xwax.o:		CPPFLAGS += $(DEVICE_CPPFLAGS)
xwax.o:		CPPFLAGS += -DEXECDIR=\"$(EXECDIR)\" -DVERSION=\"$(VERSION)\"
xwax.o:		.version

# Install to system

.PHONY:		install
install:
		$(INSTALL) -D xwax $(DESTDIR)$(BINDIR)/xwax
		$(INSTALL) -D scan $(DESTDIR)$(EXECDIR)/xwax-scan
		$(INSTALL) -D import $(DESTDIR)$(EXECDIR)/xwax-import
		$(INSTALL) -D -m 0644 xwax.1 $(DESTDIR)$(MANDIR)/man1/xwax.1
		$(INSTALL) -D -m 0644 CHANGES $(DESTDIR)$(DOCDIR)/xwax/CHANGES
		$(INSTALL) -D -m 0644 COPYING $(DESTDIR)$(DOCDIR)/xwax/COPYING
		$(INSTALL) -D -m 0644 README $(DESTDIR)$(DOCDIR)/xwax/README

# Distribution archive from Git source code

.PHONY:		dist
dist:		.version
		./mkdist $(VERSION)

# Manual tests

.PHONY:		tests
tests:		test-cues test-library test-status test-timecoder test-track

test-cues:	test-cues.o cues.o

test-library:	test-library.o external.o library.o listing.o

test-midi:	test-midi.o midi.o
test-midi:	LDLIBS += $(ALSA_LIBS)

test-status:	test-status.o status.o

test-timecoder:	test-timecoder.o lut.o timecoder.o

test-track:	test-track.o external.o rig.o thread.o track.o
test-track:	LDFLAGS += -pthread
test-track:	LDLIBS += -lm

.PHONY:		clean
clean:
		rm -f xwax \
			test-cues \
			test-library \
			test-midi \
			test-status \
			test-timecoder \
			test-track \
			$(OBJS) $(DEPS)

-include $(DEPS)
