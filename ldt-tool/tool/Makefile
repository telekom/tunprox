# vim:tw=0:ts=3:wm=0:
#
# Copyright (C) 2015-2021 by Frank Reker, Deutsche Telekom AG
#
# LDT - Lightweight (MP-)DCCP Tunnel kernel module
#
# This is not Open Source software. 
# This work is made available to you under a source-available license, as 
# detailed below.
#
# Copyright 2022 Deutsche Telekom AG
#
# Permission is hereby granted, free of charge, subject to below Commons 
# Clause, to any person obtaining a copy of this software and associated 
# documentation files (the "Software"), to deal in the Software without 
# restriction, including without limitation the rights to use, copy, modify,
# merge, publish, distribute, sublicense, and/or sell copies of the Software,
# and to permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
# DEALINGS IN THE SOFTWARE.
#
# “Commons Clause” License Condition v1.0
#
# The Software is provided to you by the Licensor under the License, as
# defined below, subject to the following condition.
#
# Without limiting other conditions in the License, the grant of rights under
# the License will not include, and the License does not grant to you, the
# right to Sell the Software.
#
# For purposes of the foregoing, “Sell” means practicing any or all of the
# rights granted to you under the License to provide to third parties, for a
# fee or other consideration (including without limitation fees for hosting 
# or consulting/ support services related to the Software), a product or 
# service whose value derives, entirely or substantially, from the
# functionality of the Software. Any license notice or attribution required
# by the License must also include this Commons Clause License Condition
# notice.
#
# Licensor: Deutsche Telekom AG
#


default: all
ldt: main.o cmd.o

ldt: SLIBS:=-L../lib -L../frlib/lib -lldt $(SLIBS) -lfr -ldl
ldt: ../frlib/lib/libfr.a

BINS=ldt


USE_DEBUG=yes
USE_EFENCE=no


CC?=$(CROSS_COMPILE)gcc
RM?=rm -f
STRIP?=$(CROSS_COMPILE)strip
EFENCE?=-lduma
DESTDIR?=/usr/local
PREFIX?=$(DESTDIR)


override INCLUDE+=-I../frlib/lib/include -I../ -I../lib/
override LINCLUDE+=-L../frlib/lib/


STAT=-Wl,-Bstatic
DYN=-Wl,-Bdynamic
FDYN=$(1)
FSTAT=$(if $(strip $(1)),$(STAT) $(1) $(DYN),)
UNAME=$(shell uname)
ifeq ($(USE_DEBUG),yes)
	DEBUG=-g -gstabs -O0 -DDEBUG -DRDEBUG
	STRIP:=true
else
	DEBUG=-O3 -DNDEBUG
endif
ifeq ($(USE_EFENCE),yes)
  EFENCE:=$(call FDYN,$(EFENCE))
else
  EFENCE:=
endif
CFLAGS?=-D$(UNAME) $(DEBUG) -Wall -fPIC
override CFLAGS+=$(INCLUDE)
FRLIB=-lfrtools -lfr
#FRLIB_DEP=$(HOME)/lib/libfr.a $(HOME)/lib/libfrtools.a
#FRLIB_DEP=
override LFLAGS+=$(LINCLUDE) $(DYN) $(DLIBS) $(EFENCE) -rdynamic
USLIBS=$(call FSTAT,$(SLIBS))

REMAKE_DEPS=Makefile

#DLIBS=-ldl
SLIBS+=$(FRLIB)
CLEANUP=



.PHONY: default all
#default: all
all: pre $(BINS)

.PHONY: pre
pre: $(PREMAKE)


define rul
$(1): $$(OBJ[$(1)]) $$(FRLIB_DEP)
	$$(CC) -o $$@ $$(LFLAGS) $$(filter %.o,$$+) $$(USLIBS)
	$$(STRIP) $$@
endef

$(foreach bin,$(BINS),$(eval $(call rul,$(bin))))
	


%.o:%.c $(REMAKE_DEPS)
	$(CC) $(CFLAGS) -c $<

.PHONY: install
install: default
	mkdir -p $(PREFIX)/usr/sbin/
	cp -vf ldt $(PREFIX)/usr/sbin/


.PHONY: depend dep
depend:
	makedepend -f.depend -- $(CFLAGS) -- *.c 2>/dev/null
dep: depend
.depend:
	echo -n > .depend


.PHONY: clean distclean
clean:
	$(RM) *.o $(BINS) $(CLEANUP) *.log
distclean: clean


include .depend

