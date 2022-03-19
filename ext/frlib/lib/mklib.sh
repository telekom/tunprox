#!/bin/bash
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
# Portions created by the Initial Developer are Copyright (C) 2003-2021
# by the Initial Developer. All Rights Reserved.
#
# ***** END LICENSE BLOCK *****




function newerthan()
{
	out="$1"
	shift
	test -e "$out" || return 0
	while test "$1"; do
		test "$1" -nt "$out" && return 0
		shift
	done
	return 1
}

out="$1"
shift
test "$out" -a "$1" || { echo "usage: mklib <outlib> <inlib>[...]" >&2; exit 1; }
newerthan "$out" $@ || exit 0
test -d tmp || mkdir tmp
rm -Rf tmp/*
cp -av $@ tmp/ || exit 1
cd tmp
for i in *.a; do
	ar x "$i"
done
rm -f *.a
${CROSS_COMPILE}ar sr "$out" *.o

rm -f *.o
mv -f "$out" ../

cd ..
rm -Rf tmp/*


exit 0
















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
