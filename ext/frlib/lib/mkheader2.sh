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
# Portions created by the Initial Developer are Copyright (C) 2003-2014
# by the Initial Developer. All Rights Reserved.
#
# ***** END LICENSE BLOCK *****




PROG="$(basename "$0")" 
SOPTS="n:b:i:"
TEMP="$(getopt -o "$SOPTS" -n "$PROG" -- "$@")"

if test $? -eq 0; then 
	eval set -- "$TEMP"
fi

while test "$1"; do
	case "$1" in
	-n)
		numdot="$2"
		shift
		;;
	-b)
		base="$2"
		shift
		;;
	-i)
		inc="$2"
		shift
		;;
	esac;
	shift
done

if ! test "$base"; then
	echo "no base name specified" >&2
	exit 1
fi

test "$inc" || inc="../include"
if ! test -d "$inc"; then
	echo "$inc does not exist - specify -i" >&2
	exit 1
fi

test "$(test "$numdot" -eq 0 2>&1)" && numdot=2
numdot=2
misc="misc"
for i in `seq 1 $numdot`; do
	misc="../${misc}"
done


test -e "$inc/$base" -a ! -d "$inc/$base" && rm -Rf "$inc/$base"
test -d "$inc/$base" || { echo create: $i; mkdir -p "$inc/$base"; }
found=no
for j in *.h; do
	diff -q "$j" "$inc/$base/$j" >/dev/null 2>&1 && continue;
	found=yes
	cp -vf "$j" "$inc/$base/"
done
for j in "$inc/$base/"*.h; do
	k="$(basename "$j")"
	test -e "$k" && continue;
	rm -f "$j"
	found=yes
done
if test "$found" = yes; then
	cd "$inc"
	nameb="`echo "$base" | tr "[a-z]/" "[A-Z]_"`"
	{ 
		cat "${misc}/copynote.txt"
		echo -e "\n\n#ifndef _R__FRLIB_${nameb}_H\n#define _R__FRLIB_${nameb}_H\n"; 
		for j in $base/*.h; do 
			test "$j" = "buffer/buffer_types.h" && continue;
			echo "#include <fr/$j>"; 
		done
		echo -e "\n\n#endif /* _R__FRLIB_${nameb}_H */"
		cat "${misc}/emacsvim.c"
	} > "${base}.h"
	cd ..
	echo ${base}.h created
fi














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
