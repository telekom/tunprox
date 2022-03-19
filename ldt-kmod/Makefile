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

LINUXSRC?=/lib/modules/$(shell uname -r)/build
VER=$(shell cat ldt_version.h  | grep -w LDT_VERSION | cut -f2 -d\")

default:
	make -C $(LINUXSRC) M=$(PWD)

clean:
	make -C $(LINUXSRC) M=$(PWD) clean

modules_install:
	make -C $(LINUXSRC) M=$(PWD) modules_install

install: modules_install

obj-m += ldt.o

ldt-y := ldt_cfg.o ldt_dev.o ldt_event.o ldt_ip.o \
				ldt_mod.o ldt_netlink.o ldt_prot1.o \
				ldt_sysctl.o ldt_tunaddr.o ldt_tun.o \
				ldt_queue.o ldt_lock.o

ldt-$(CONFIG_IP_DCCP) += ldt_mpdccp.o

header-y += ldt_kernel.h ldt_version.h
destination-y := include/linux

distclean: clean













#
# Overrides for XEmacs and vim so that we get a uniform tabbing style.
# XEmacs/vim will notice this stuff at the end of the file and automatically
# adjust the settings for this buffer only.  This must remain at the end
# of the file.
# ---------------------------------------------------------------------------
# Local variables:
# c-indent-level: 3
# c-basic-offset: 3
# tab-width: 3
# End:
# vim:tw=0:ts=3:wm=0:
#
