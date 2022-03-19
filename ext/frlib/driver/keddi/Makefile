#
# Eddi Message Distribution Module
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 2.0 / GPL 2.0
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
# Portions created by the Initial Developer are Copyright (C) 2013-2014
# by the Initial Developer. All Rights Reserved.
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 (the "GPL"), in which
# case the provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only under the
# terms of the GPL, and not to allow others to use your version of this
# file under the terms of the MPL, indicate your decision by deleting
# the provisions above and replace them with the notice and other provisions
# required by the GPL. If you do not delete the provisions above, a
# recipient may use your version of this file under the terms of any one
# of the MPL or the GPL.
#
# ***** END LICENSE BLOCK *****


default:
	make -C /lib/modules/`uname -r`/build M=$(PWD)

clean:
	make -C /lib/modules/`uname -r`/build M=$(PWD) clean

modules_install:
	make -C /lib/modules/`uname -r`/build M=$(PWD) modules_install

install: modules_install

obj-m += keddi.o

keddi-y := eddinl.o eddimod.o
keddi-$(CONFIG_SYSFS) += eddisys.o

header-y += keddi.h
destination-y := include/linux

eddisys.o: eddinl.int.h
eddimod.o: eddinl.int.h eddisys.int.h
eddinl.o: keddi.h









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
