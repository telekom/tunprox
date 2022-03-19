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
CCLIBS?=


ifeq ($(CONFIG_DEBUG),yes)
  STRIP=true
else
  STRIP=$(RSTRIP)
endif


INCLUDES=$(EXTINCL)

ifeq ($(CONFIG_DEBUG),yes)
  DEBUG=-g -O0 -DDEBUG
else
  DEBUG=-O3 -DNDEBUG
endif


CCFLAGS?=$(DEBUG) $(INCLUDES) $(UNAME_FLAG) $(CC_WARN_STRICT)
SLIBS=
#SSLLIBS=-Wl,-Bstatic -lcrypto -Wl,-Bdynamic
#MSSLLIBS=openssl/libcrypto.a
ALIBS=$(CCLIB) $(SLIBS)



EFENCE=-Wl,-Bdynamic -lefence
ifeq ($(CONFIG_DUMA),yes)
  REFENCE=$(EFENCE)
endif

ifeq ($(UNAME),CYGWIN)
	REPFILE=repwin.o 
	#REPLIB=-L/lib/w32api -ladvapi32
	REPLIB=/lib/w32api/libadvapi32.a
	SSLFILES=
	SSLLIBS=/lib/libssl.dll.a /lib/libcrypto.dll.a
else
	REPFILE=replinux.o
	REPLIBS=
	SSLFILES=
	SSLLIBS=-lssl -lcrypto
endif

LIBS?=$(ALIBS) $(REFENCE) $(SSLLIBS) $(REPLIBS)

GLFLAGS=
LFLAGS?=$(GLFLAGS) $(LIBS)






CCFRLIB_SUB=base buffer connect mail shm xml event
CCFRLIB_A := $(shell for i in $(CCFRLIB_SUB); do echo "$$i/cc$$i.a"; done)

CCFRLIB_STATIC=cc.a



BINS=
OBINS=
OLIBS=$(CCFRLIB_STATIC)
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
	set -e; for i in $(CCFRLIB_SUB); do \
		CFLAGS="$(CCFLAGS)" \
		LFLAGS="$(LFLAGS)" \
		MCC="$(MCC)" \
		MCCP="$(MCCP)" \
		CROSS_COMPILE="$(CROSS_COMPILE)" \
		$(MAKE) -C $$i all; done

.PHONY: headers header
headers: header
header:
	set -e; for i in $(CCFRLIB_SUB); do \
		CFLAGS="$(CCFLAGS)" \
		LFLAGS="$(LFLAGS)" \
		MCC="$(MCC)" \
		MCCP="$(MCCP)" \
		CROSS_COMPILE="$(CROSS_COMPILE)" \
		$(MAKE) -C $$i header; done


.PHONY: dep
dep:
	set -e; for i in $(CCFRLIB_SUB); do \
		CFLAGS="$(CCFLAGS)" \
		LFLAGS="$(LFLAGS)" \
		MCC="$(MCC)" \
		MCCP="$(MCCP)" \
		CROSS_COMPILE="$(CROSS_COMPILE)" \
		$(MAKE) -C $$i dep; done

.PHONY: clean distclean myclean
clean: myclean
	set -e; for i in $(CCFRLIB_SUB); do $(MAKE) -C $$i clean; done
distclean: myclean
	set -e; for i in $(CCFRLIB_SUB); do $(MAKE) -C $$i distclean; done
myclean:
	$(RM) $(CCFRLIB_STATIC)


$(CCFRLIB_STATIC): subdirs $(CCFRLIB_A)
	../mklib.sh $(CCFRLIB_STATIC) $(CCFRLIB_A)

%/%.a: subdirs



.c.o:
	$(MCC) -c $(CFLAGS) $(GUI_CFLAGS) $<

.cc.o:
	$(MCCP) -c $(CFLAGS) $<




.PHONY: docs
docs:
	doxygen
	make -C doc/latex














