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
# Portions created by the Initial Developer are Copyright (C) 2003-2014
# by the Initial Developer. All Rights Reserved.
#
# ***** END LICENSE BLOCK *****



MCC?=$(CROSS_COMPILE)gcc
RM?=rm -f
RSTRIP?=$(CROSS_COMPILE)strip
CP?=cp -f
AR?=ar

include ../Makefile.config
include ../Makefile.warnings


UNAME:=$(shell uname)

ifeq ($(CONFIG_DEBUG),yes)
  STRIP=true
else
  STRIP=$(RSTRIP)
endif


override INCLUDES:=  -I../include/
ifneq ($(FRLIB_PATH),)
	INCLUDES+= -I$(FRLIB_PATH)/lib/include/
endif

ifeq ($(CONFIG_DEBUG),yes)
  DEBUG=-g -O0 -DDEBUG
else
  DEBUG=-O3 -DNDEBUG
endif

CFLAGS?=$(DEBUG) $(INCLUDES) -D$(UNAME) $(C_WARN_STRICT)
override CFLAGS+=$(INCLUDES)
ifeq ($(CONFIG_SHLIB),yes)
  override CFLAGS+= -fPIC
endif
ifeq ($(UNAME),SunOS)
  SLIBS= 
endif
ifeq ($(UNAME),Linux)
  SLIBS=
endif
GCCLIB=-Wl,-Bstatic -lgcc -Wl,-Bdynamic
ALIBS=$(GCCLIB) $(SLIBS)


EFENCE=-Wl,-Bdynamic -lefence
ifeq ($(CONFIG_DUMA),yes)
  REFENCE=$(EFENCE)
endif
LIBS?=$(ALIBS) $(REFENCE)

GLFLAGS=
LFLAGS?=$(GLFLAGS) $(LIBS)


THREAD_OBJ:=	thrdstart.o thrd.o

THREAD=thread.a

TESTPROGS=testthrd

BINS=
OBINS=
EXTBINS=
ARS=$(THREAD)

REMAKE_DEPS=Makefile ../Makefile.config


.PHONY: default all obins bins extbins tests ars
default: all
all: pre ars bins extbins tests obins
obins: $(OBINS)
bins: $(BINS)
extbins: $(EXTBINS)
tests: $(TESTPROGS)
ars: $(ARS)


$(THREAD): $(THREAD_OBJ)
	$(RM) $(THREAD)
	$(AR) r $(THREAD) $(THREAD_OBJ)


.PHONY: pre header
pre: header

header:
	../mkheader2.sh -b thread -i ../include/

%.o: %.c $(REMAKE_DEPS)
	$(MCC) -c $(CFLAGS) $<

testthrd: testthrd.c
	$(MCC) -g -ldl -I ../include -o testthrd testthrd.c thread.a ../base/base.a ../cal/cal.a ../base/base.a -lpthread

.PHONY: depend dep
depend:
	makedepend -f.depend -- $(CFLAGS) -- *.c 2>/dev/null
dep: depend
.depend:
	echo -n > .depend

.PHONY: clean distclean
clean:
	$(RM) *.a *.o $(CONNECT) $(BINS) $(OBINS) $(EXTBINS) $(TESTPROGS)
distclean: clean
	$(RM) .depend .depend.bak

include .depend

