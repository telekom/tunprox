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



include ../Makefile.config
include ../Makefile.warnings


MCC?=$(CROSS_COMPILE)gcc
MCCP?=$(CROSS_COMPILE)g++
RM?=rm -f
RSTRIP?=$(CROSS_COMPILE)strip
CP?=cp -f
MAKE?=make


UNAME:=$(shell uname)
FUNAME:=$(shell uname | cut -c1-6)
ifeq ($(FUNAME),CYGWIN)
  UNAME_FLAG:=-DCYGWIN=$(UNAME) -DLinux
  UNAME:=$(FUNAME)
else
  UNAME_FLAG:=-D$(UNAME)
endif


EXTINCL=
EXTLIBS=
#CCLIBS=-Wl,-Bstatic -lgcc -Wl,-Bdynamic 
CCLIBS=


ifeq ($(CONFIG_DEBUG),yes)
  STRIP=true
else
  STRIP=$(RSTRIP)
endif


override INCLUDES+=$(EXTINCL)

ifeq ($(CONFIG_DEBUG),yes)
  DEBUG=-g -O0 -DDEBUG
else
  DEBUG=-O3 -DNDEBUG
endif


CFLAGS?=$(DEBUG) $(INCLUDES) $(UNAME_FLAG) -fPIC $(C_WARN_STRICT)
CFLAGS?=$(DEBUG) $(INCLUDES) $(UNAME_FLAG) -fPIC $(CC_WARN)

ifeq ($(UNAME),CYGWIN)
	#override LIBS+=-L/lib/w32api -ladvapi32
	override LIBS+=/lib/w32api/libadvapi32.a
endif


FR_LFLAGS?=$(LFLAGS)
LFLAGS?= $(LIBS)



FRLIB_SUB:=base buffer connect mail shm spool xml cal netlink event thread arch
ifeq ($(CONFIG_REGISTRY),yes)
	FRLIB_SUB+= registry
endif
ifeq ($(CONFIG_CPLUSPLUS),yes)
	FRLIB_SUB+= cc
endif
FRLIB_A := $(shell for i in $(FRLIB_SUB); do echo "$$i/$$i.a"; done)

FRLIB=fr
FRLIB_STATIC=lib$(FRLIB).a
ifeq ($(UNAME),CYGWIN)
  LIB_FR=cyg$(FRLIB).dll
  FR_LFLAGS+=-shared -o $(LIB_FR) \
    -Wl,--out-implib=lib$(FRLIB).dll.a \
    -Wl,--export-all-symbols \
    -Wl,--enable-auto-import \
    -Wl,--whole-archive $(FRLIB_STATIC) \
    -Wl,--no-whole-archive $(LIBS)
else
  LIB_FR=lib$(FRLIB).so
  FR_LFLAGS+=-shared -o $(LIB_FR) \
    -Wl,--whole-archive $(FRLIB_STATIC) \
    -Wl,--no-whole-archive $(LIBS)
endif





BINS=
OBINS=
OLIBS=$(LIB_FR)
#OLIBS=$(FRLIB_STATIC)
TESTPROGS=



.PHONY: default all bins obins tests libs
default: all
all: libs bins tests obins
obins: $(OBINS) 
bins: $(BINS)
tests: $(TESTPROGS)
libs: $(OLIBS)

.PHONY: subdirs
subdirs: headers
	set -e; for i in $(FRLIB_SUB); do \
		CFLAGS="$(CFLAGS)" \
		CCFLAGS="$(CCFLAGS)" \
		LFLAGS="$(LFLAGS)" \
		MCC="$(MCC)" \
		MCCP="$(MCCP)" \
		CROSS_COMPILE="$(CROSS_COMPILE)" \
		$(MAKE) -C $$i all; done

.PHONY: autogen
autogen:
	./mkheader.sh -n 2
	./configen.sh

.PHONY: headers header
headers: header
header: autogen
	set -e; for i in $(FRLIB_SUB); do \
		CFLAGS="$(CFLAGS)" \
		CCFLAGS="$(CCFLAGS)" \
		LFLAGS="$(LFLAGS)" \
		MCC="$(MCC)" \
		MCCP="$(MCCP)" \
		CROSS_COMPILE="$(CROSS_COMPILE)" \
		$(MAKE) -C $$i header; done

.PHONY: dep
dep:
	set -e; for i in $(FRLIB_SUB); do \
		CFLAGS="$(CFLAGS)" \
		CCFLAGS="$(CCFLAGS)" \
		LFLAGS="$(LFLAGS)" \
		MCC="$(MCC)" \
		MCCP="$(MCCP)" \
		CROSS_COMPILE="$(CROSS_COMPILE)" \
		$(MAKE) -C $$i dep; done

.PHONY: clean distclean myclean
clean: myclean
	set -e; for i in $(FRLIB_SUB); do $(MAKE) -C $$i clean; done
distclean: myclean
	set -e; for i in $(FRLIB_SUB); do $(MAKE) -C $$i distclean; done
	find include/ -name \*.h -exec rm -f \{\} \;
myclean:
	$(RM) $(LIB_FR) $(FRLIB_STATIC) lib$(FRLIB).dll.a


$(FRLIB_STATIC): subdirs $(FRLIB_A)
	CROSS_COMPILE=$(CROSS_COMPILE) ./mklib.sh $(FRLIB_STATIC) $(FRLIB_A)

%/%.a: subdirs

$(LIB_FR): $(FRLIB_STATIC)
	if test "$(CONFIG_SHLIB)" = yes; then \
		$(MCCP) $(FR_LFLAGS); \
		$(STRIP) $(LIB_FR); \
	fi



.c.o:
	$(MCC) -c $(CFLAGS) $(GUI_CFLAGS) $<

.cc.o:
	$(MCCP) -c $(CFLAGS) $<




.PHONY: docs
docs:
	doxygen
	make -C doc/latex














