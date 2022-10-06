# vim:tw=0:ts=3:wm=0:
#
# Copyright (C) 2015-2022 by Frank Reker, Deutsche Telekom AG
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



CC?=$(CROSS_COMPILE)gcc
RM?=rm -f
STRIP?=$(CROSS_COMPILE)strip
CP?=cp -f
AR?=$(CROSS_COMPILE)ar
DESTDIR?=/usr/local/
PREFIX?=$(DESTDIR)

USE_DEBUG=yes

UNAME:=$(shell uname)

ifeq ($(USE_DEBUG),yes)
  STRIP:=true
endif


FRLIB_PATH=../frlib
override INCLUDES+=-I./
ifneq ($(FRLIB_PATH),)
	override INCLUDES+= -I$(FRLIB_PATH)/lib/include/ -I$(FRLIB_PATH)/include/
endif

ifeq ($(USE_DEBUG),yes)
  DEBUG=-g -gstabs -O0 -DDEBUG
else
  DEBUG=-O3 -DNDEBUG
endif

CFLAGS?=$(DEBUG) $(INCLUDES) -D$(UNAME) -Wall -Wno-char-subscripts -Wfatal-errors
override CFLAGS+=$(INCLUDES)
ifneq ($(STATIC_ONLY),yes)
  override CFLAGS+= -fPIC
endif



LDT_OBJ:=ldt_cfgcmd.o ldt_getinfo.o ldt_nl.o ldt_event.o

LDT_SLIB=libldt.a
LDT_LIB=libldt.so
TPLFLAGS+=-shared -o $(LDT_LIB) \
	-Wl,--whole-archive $(LDT_SLIB) \
	-Wl,--no-whole-archive $(LIBS)

TESTPROGS=

ARS=$(LDT_SLIB)
SO=$(LDT_LIB)

REMAKE_DEPS=Makefile


.PHONY: default all tests libs
default: all
all: libs tests
tests: $(TESTPROGS)
libs: $(ARS) $(SO)


$(LDT_SLIB): $(LDT_OBJ)
	$(RM) $(LDT_SLIB)
	$(AR) r $(LDT_SLIB) $(LDT_OBJ)

$(LDT_LIB): $(LDT_SLIB)
	$(CC) $(TPLFLAGS)
	$(STRIP) $(LDT_LIB)

%.o: %.c $(REMAKE_DEPS)
	$(CC) -c $(CFLAGS) $<

install: default
	mkdir -p $(PREFIX)/lib
	$(CP) -vf libldt* $(PREFIX)/lib/
	mkdir -p $(PREFIX)/include/ldt
	$(CP) -vf ldt*.h $(PREFIX)/include/ldt

.PHONY: depend dep
depend:
	makedepend -f.depend -- $(CFLAGS) -- *.c 2>/dev/null
dep: depend
.depend:
	echo -n > .depend

.PHONY: clean distclean
clean:
	$(RM) *.a *.o *.so $(TESTPROGS) *.log
distclean: clean
	$(RM) .depend .depend.bak

include .depend

