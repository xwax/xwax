#
# Copyright (C) 2024 Mark Hills <mark@xwax.org>
#
# This file is part of "xwax".
#
# "xwax" is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License, version 3 as
# published by the Free Software Foundation.
#
# "xwax" is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <https://www.gnu.org/licenses/>.
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

CFLAGS ?= -O3
CFLAGS += -Wall
CPPFLAGS += -MMD -MP
LDFLAGS ?= -O3

# Core objects and libraries

OBJS = controller.o \
	cues.o \
	deck.o \
	device.o \
	dummy.o \
	excrate.o \
	external.o \
	index.o \
	interface.o \
	library.o \
	listbox.o \
	lut.o \
	player.o \
	realtime.o \
	rig.o \
	selector.o \
	status.o \
	thread.o \
	timecoder.o \
	track.o \
	mpu6050control.o \
	xwax.o
DEVICE_CPPFLAGS =
DEVICE_LIBS =

TESTS = tests/cues \
	tests/external \
	tests/library \
	tests/observer \
	tests/status \
	tests/timecoder \
	tests/track \
	tests/ttf

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

TEST_OBJS = $(addsuffix .o,$(TESTS))
DEPS = $(OBJS:.o=.d) $(TEST_OBJS:.o=.d) mktimecode.d

# Rules

.PHONY:		all
all:		xwax mktimecode tests

# Dynamic versioning

.PHONY:		FORCE
.version:	FORCE
		./mkversion -r

VERSION = $(shell ./mkversion)

# Main binary

xwax:		$(OBJS)
xwax:		LDLIBS += $(SDL_LIBS) $(DEVICE_LIBS) -lm -lbluetooth
xwax:		LDFLAGS += -pthread 

interface.o:	CFLAGS += $(SDL_CFLAGS)

xwax.o:		CFLAGS += $(SDL_CFLAGS)
xwax.o:		CPPFLAGS += $(DEVICE_CPPFLAGS)
xwax.o:		CPPFLAGS += -DEXECDIR=\"$(EXECDIR)\" -DVERSION=\"$(VERSION)\"
xwax.o:		.version

# Supporting programs

mktimecode:	mktimecode.o
mktimecode:	LDLIBS  += -lm

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

# Editor tags files

TAGS:		$(OBJS:.o=.c)
		etags $^

# Manual tests

.PHONY:		tests
tests:		$(TESTS)
tests:		CPPFLAGS += -I.

tests/cues:	tests/cues.o cues.o

tests/external:	tests/external.o external.o

tests/library:	tests/library.o excrate.o external.o index.o library.o rig.o status.o thread.o track.o
tests/library:	LDFLAGS += -pthread

tests/midi:	tests/midi.o midi.o
tests/midi:	LDLIBS += $(ALSA_LIBS)

tests/observer:	tests/observer.o

tests/status:	tests/status.o status.o

tests/timecoder:	tests/timecoder.o lut.o timecoder.o

tests/track:	tests/track.o excrate.o external.o index.o library.o rig.o status.o thread.o track.o
tests/track:	LDFLAGS += -pthread
tests/track:	LDLIBS += -lm

tests/ttf.o:	tests/ttf.c  # not needed except to workaround Make 3.81
tests/ttf.o:	CFLAGS += $(SDL_CFLAGS)

tests/ttf:	LDLIBS += $(SDL_LIBS)

.PHONY:		clean
clean:
		rm -f xwax \
			$(OBJS) $(DEPS) \
			$(TESTS) $(TEST_OBJS) \
			mktimecode mktimecode.o \
			TAGS

-include $(DEPS)
