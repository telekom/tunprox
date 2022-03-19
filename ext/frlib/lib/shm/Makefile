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


override INCLUDES:= -I../include/ -I../base/

ifeq ($(CONFIG_DEBUG),yes)
  DEBUG=-g -O0 -DDEBUG
else
  DEBUG=-O3 -DNDEBUG
endif

CFLAGS?=$(DEBUG) -D$(UNAME) $(C_WARN_STRICT)
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


SHM_OBJ=shm.o

SHM=shm.a

TESTPROGS=

BINS=
OBINS=
EXTBINS=
ARS=$(SHM)

REMAKE_DEPS=Makefile ../Makefile.config


.PHONY: default all obins bins extbins tests ars
default: all
all: pre ars bins extbins tests obins
obins: $(OBINS)
bins: $(BINS)
extbins: $(EXTBINS)
tests: $(TESTPROGS)
ars: $(ARS)

$(SHM): $(SHM_OBJ)
	$(RM) $(SHM)
	$(AR) r $(SHM) $(SHM_OBJ)


.PHONY: pre header
pre: header

header:
	../mkheader2.sh -b shm -i ../include/

%.o: %.c $(REMAKE_DEPS)
	$(MCC) -c $(CFLAGS) $<


.PHONY: depend dep
depend:
	makedepend -f.depend -- $(CFLAGS) -- *.c 2>/dev/null
dep: depend
.depend:
	echo -n > .depend

.PHONY: clean distclean
clean:
	$(RM) *.o $(SHM) $(BINS) $(OBINS) $(EXTBINS) $(TESTPROGS)
distclean: clean
	$(RM) .depend .depend.bak

include .depend

