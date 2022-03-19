# vim:tw=0:ts=3:wm=0:
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 2.0
#
#  This Source Code Form is subject to the terms of the Mozilla Public
#  License, v. 2.0. If a copy of the MPL was not distributed with this
#  file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for more details governing rights and limitations under the License.
#
# The Original Code is part of the frlib.
#
# The Initial Developer of the Original Code is
# Frank Reker <frank@reker.net>.
# Portions created by the Initial Developer are Copyright (C) 2003-2017
# by the Initial Developer. All Rights Reserved.
#
# ***** END LICENSE BLOCK *****



MCC?=$(CROSS_COMPILE)gcc
MCCP?=$(CROSS_COMPILE)g++
RM?=rm -f
RSTRIP?=$(CROSS_COMPILE)strip
CP?=cp -vf
AR?=$(CROSS_COMPILE)ar
CC=$(MCC)

DESTDIR=../../lib/

include ../../Makefile.config
include ../../Makefile.warnings


UNAME:=$(shell uname)

ifeq ($(CONFIG_DEBUG),yes)
  STRIP=true
else
  STRIP=$(RSTRIP)
endif


INCLUDES= -I../../lib/include/

ifeq ($(CONFIG_DEBUG),yes)
  DEBUG=-g -O0 -DDEBUG
else
  DEBUG=-O3 -DNDEBUG
endif
CFLAGS?=$(DEBUG) -D$(UNAME) $(C_WARN_STRICT)
override CFLAGS+=$(INCLUDES) -fPIC
GCCLIB=-Wl,-Bstatic -lgcc -Wl,-Bdynamic

LIBS?=$(GCCLIB)

TOOLS_LFLAGS?=$(LFLAGS)
LFLAGS?=$(LIBS)


TOOLS_OBJ=	rdconfig.o\
			scfmgr.o\
			starter.o\
			wrlog.o\
			frdate.o\
			roman.o\
			spool.o\
			timescan.o\
			tquote.o\
			eddi.o\
			evparse.o\
			uevent2eddi.o\
			evfc.o\
			garp.o \
			ipool.o \
			sar.o \
			#varmon.o \


TOOLS=frtools
TOOLSTAT=lib$(TOOLS).a
ifeq ($(UNAME),CYGWIN)
  TOOLDYN=cyg$(TOOLS).dll
  TOOLS_LFLAGS+=-shared -o $(TOOLDYN) \
    -Wl,--out-implib=lib$(TOOLS).dll.a \
    -Wl,--export-all-symbols \
    -Wl,--enable-auto-import \
    -Wl,--whole-archive $(TOOLSTAT) \
    -Wl,--no-whole-archive $(LIBS)
else
  TOOLDYN=lib$(TOOLS).so
  TOOLS_LFLAGS+=-shared -o $(TOOLDYN) \
    -Wl,--whole-archive $(TOOLSTAT) \
    -Wl,--no-whole-archive $(LIBS)
endif

DESTDYN=$(DESTDIR)/$(TOOLDYN)
DESTSTAT=$(DESTDIR)/$(TOOLSTAT)

TESTPROGS=

BINS=
OBINS=
EXTBINS=
ifeq ($(STATIC_ONLY),yes)
	OLIBS=
	DESTS=$(DESTSTAT)
else
	OLIBS=$(TOOLDYN)
	DESTS=$(DESTDYN) $(DESTSTAT)
endif
ARS=$(TOOLSTAT)

REMAKE_DEPS=Makefile ../../Makefile.config


.PHONY: default all obins bins extbins tests ars olibs dests
default: all
all: pre ars bins extbins tests obins olibs dests
obins: $(OBINS)
bins: $(BINS)
olibs: $(OLIBS)
extbins: $(EXTBINS)
tests: $(TESTPROGS)
ars: $(ARS)
dests: $(DESTS)


$(TOOLSTAT): $(TOOLS_OBJ)
	$(RM) $(TOOLSTAT)
	$(AR) rs $(TOOLSTAT) $(TOOLS_OBJ)

$(TOOLDYN): $(TOOLSTAT)
	$(MCC) $(TOOLS_LFLAGS)
	$(STRIP) $(TOOLDYN)

$(DESTSTAT): $(TOOLSTAT)
	$(CP) -a $(TOOLSTAT) $(DESTSTAT)

$(DESTDYN): $(TOOLDYN)
	$(CP) -a $(TOOLDYN) $(DESTDYN)

.PHONY: pre header
pre: header

header:
	../../lib/mkheader2.sh -b tools -i ../../lib/include/

%.o: %.c $(REMAKE_DEPS)
	$(MCC) -c $(CFLAGS) $<
%.o: %.cc $(REMAKE_DEPS)
	$(MCCP) -c $(CFLAGS) $<



.PHONY: depend dep
depend:
	makedepend -f.depend -- $(CFLAGS) -- *.c 2>/dev/null
dep: depend
.depend:
	echo -n > .depend

.PHONY: clean distclean
clean:
	$(RM) *.o $(ARS) $(OLIBS) $(BINS) $(OBINS) $(EXTBINS) $(TESTPROGS)
distclean: clean
	$(RM) .depend .depend.bak

include .depend

