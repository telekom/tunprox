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



include Makefile.config
include Makefile.warnings


CC?=gcc
ifeq ($(CC),cc)
	CC:=$(CROSS_COMPILE)gcc
endif
CPP?=$(CROSS_COMPILE)g++
ifeq ($(CPP),cpp)
	CPP:=$(CROSS_COMPILE)g++
endif

MCC?=$(CC)
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
CCLIBS=-Wl,-Bstatic -lgcc -Wl,-Bdynamic 


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


CCFLAGS:=$(CFLAGS) $(CC_WARN_STRICT)
override CFLAGS+=$(DEBUG) $(INCLUDES) $(UNAME_FLAG) $(C_WARN_STRICT)

#-Wno-deprecated 

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
	#SSLLIBS=/lib/libssl.dll.a /lib/libcrypto.dll.a
else
	REPFILE=replinux.o
	REPLIBS=
	SSLFILES=
	#SSLLIBS=-lssl -lcrypto
endif

override LIBS+=$(ALIBS) $(REFENCE) $(SSLLIBS) $(REPLIBS)

GLFLAGS=
override LFLAGS+=$(GLFLAGS) $(LIBS)

ifeq ($(M32),yes)
	CFLAGS+=-m32
	LFLAGS+=-m32
endif



FRLIB_SUB:=lib
ifeq ($(CONFIG_TOOLLIB),yes)
	FRLIB_SUB+= tools/lib tools
endif






BINS=
OBINS=
OLIBS=
TESTPROGS=
SUBS=$(FRLIB_SUB)


.PHONY: default all bins obins tests libs subs
default: all
all: subs libs bins tests obins
obins: $(OBINS) 
bins: $(BINS)
tests: $(TESTPROGS)
libs: $(OLIBS)
subs: subdirs

.PHONY: subdirs
subdirs: 
	set -e; for i in $(FRLIB_SUB); do \
		CFLAGS="$(CFLAGS)" \
		CCFLAGS="$(CCFLAGS)" \
		LFLAGS="$(LFLAGS)" \
		MCC="$(MCC)" \
		MCCP="$(MCCP)" \
		CROSS_COMPILE="$(CROSS_COMPILE)" \
		$(MAKE) -C $$i all; done


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

.PHONY: clean distclean
clean:
	set -e; for i in $(FRLIB_SUB); do $(MAKE) -C $$i clean; done
distclean:
	set -e; for i in $(FRLIB_SUB); do $(MAKE) -C $$i distclean; done

install: all
	mkdir -p $(PREFIX)/lib
	$(CP) -vf lib/libfr* $(PREFIX)/lib/
	mkdir -p $(PREFIX)/include
	rm -Rf $(PREFIX)/include/fr
	$(CP) -rvf lib/include $(PREFIX)/include/fr



.c.o:
	$(MCC) -c $(CFLAGS) $(GUI_CFLAGS) $<

.cc.o:
	$(MCCP) -c $(CFLAGS) $<




.PHONY: docs doc-install
docs:
	#doxygen
	make -C doc

doc-install:
	make -C doc install










