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
# Portions created by the Initial Developer are Copyright (C) 2003-2020
# by the Initial Developer. All Rights Reserved.
#
# ***** END LICENSE BLOCK *****



MCC?=$(CROSS_COMPILE)gcc
MCCP?=$(CROSS_COMPILE)g++
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


INCLUDES:=  -I../../include/
ifneq ($(FRLIB_PATH),)
	INCLUDES+= -I$(FRLIB_PATH)/lib/include
endif

ifeq ($(CONFIG_DEBUG),yes)
  DEBUG=-g -O0 -DDEBUG
else
  DEBUG=-O3 -DNDEBUG
endif

CCFLAGS?=$(DEBUG) -D$(UNAME) $(CC_WARN_STRICT)
override CCFLAGS+=$(INCLUDES)
ifeq ($(CONFIG_SHLIB),yes)
  override CCFLAGS+= -fPIC
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


EVENT_OBJ=Event.o

EVENT=ccevent.a

TESTPROGS=

BINS=
OBINS=
EXTBINS=
ARS=$(EVENT)

REMAKE_DEPS=Makefile ../Makefile.config


.PHONY: default all obins bins extbins tests ars
default: all
all: pre ars bins extbins tests obins
obins: $(OBINS)
bins: $(BINS)
extbins: $(EXTBINS)
tests: $(TESTPROGS)
ars: $(ARS)

$(EVENT): $(EVENT_OBJ)
	$(RM) $(EVENT)
	$(AR) r $(EVENT) $(EVENT_OBJ)


.PHONY: pre header
pre: header

header:
	../../mkheader2.sh -b cc/event -i ../../include/

%.o: %.c $(REMAKE_DEPS)
	$(MCC) -c $(CCFLAGS) $<
%.o: %.cc $(REMAKE_DEPS)
	$(MCCP) -c $(CCFLAGS) $<


.PHONY: depend dep
depend:
	makedepend -f.depend -- $(CCFLAGS) -- *.c *.cc 2>/dev/null
dep: depend
.depend:
	echo -n > .depend

.PHONY: clean distclean
clean:
	$(RM) *.o $(EVENT) $(BINS) $(OBINS) $(EXTBINS) $(TESTPROGS)
distclean: clean
	$(RM) .depend .depend.bak

include .depend

