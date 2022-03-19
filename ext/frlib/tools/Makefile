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


default: all
rdconfig: rdconfig.o
scfmgr: scfmgr.o
spool: spool.o
starter: starter.o
wrlog: wrlog.o
evparse: evparse.o
timescan: timescan.o
eddi: eddi.o
roman: roman.o
frdate: frdate.o
evfc: evfc.o
uevent2eddi: uevent2eddi.o
varmon: varmon.o
tquote: tquote.o
garp: garp.o
ipool: ipool.o
sar: sar.o

eddi: DLIBS+=-lpthread
uevent2eddi: DLIBS+=-lpthread
varmon: DLIBS+=-lpthread
frdate: DLIBS+=-lpthread
sar: DLIBS+=-lpthread

BINS=rdconfig wrlog scfmgr spool starter evparse timescan eddi \
		roman frdate evfc uevent2eddi tquote garp ipool sar
		#varmon


CONFIG_DEBUG:=yes
CONFIG_DUMA:=no

include ../Makefile.config


MCC?=$(CROSS_COMPILE)gcc
RM?=rm -f
STRIP?=$(CROSS_COMPILE)strip
EFENCE?=-lduma


INCLUDE=-I../lib/include
LINCLUDE=-L../lib/


STAT=-Wl,-Bstatic
DYN=-Wl,-Bdynamic
FDYN=$(1)
FSTAT=$(if $(strip $(1)),$(STAT) $(1) $(DYN),)
UNAME=$(shell uname)
ifeq ($(CONFIG_DEBUG),yes)
	DEBUG=-g -O0 -DDEBUG -DRDEBUG
	STRIP:=true
else
	DEBUG=-O3 -DNDEBUG
endif
ifeq ($(CONFIG_DUMA),yes)
  EFENCE:=$(call FDYN,$(EFENCE))
else
  EFENCE:=
endif
CFLAGS?=-D$(UNAME) $(DEBUG) -Wall -fPIC
override CFLAGS+=$(INCLUDE) -fPIC
FRLIB=-lfrtools -lfr 
FRLIB_DEP=../lib/libfr.a ../lib/libfrtools.a
DLIBS+= $(EFENCE)
LFLAGS+=$(LINCLUDE) $(DYN) -rdynamic

USLIBS=$(call FSTAT,$(SLIBS))

REMAKE_DEPS=Makefile

DLIBS+=-ldl
SLIBS=$(FRLIB)
CLEANUP=

OBINS:=$(foreach bin,$(BINS),$(subst yes,$(bin),$(subst no,,$(value $(shell echo CONFIG_TOOL_$(bin) | tr a-z A-Z)))))

.PHONY: default all
#default: all
all: pre $(OBINS)

.PHONY: pre
pre: $(PREMAKE)


define rul
$(1): $$(OBJ[$(1)]) $$(FRLIB_DEP)
	$$(MCC) -o $$@ $$(LFLAGS) $$(filter %.o,$$+) $$(USLIBS) $$(DYN) $$(DLIBS)
	$$(STRIP) $$@
endef

$(foreach bin,$(OBINS),$(eval $(call rul,$(bin))))
	


%.o:%.c $(REMAKE_DEPS)
	$(MCC) $(CFLAGS) -c $<



.PHONY: depend dep
depend:
	makedepend -f.depend -- $(CFLAGS) -- *.c 2>/dev/null
dep: depend
.depend:
	echo -n > .depend


.PHONY: clean distclean
clean:
	$(RM) *.o $(BINS) $(CLEANUP)
distclean: clean


include .depend

