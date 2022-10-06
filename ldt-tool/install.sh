#!/bin/bash


SOPTS="hd:l"
if test "$SOPTS" && test "$(which getopt)"; then
   TEMP="$(getopt -o "$SOPTS" -n "$PROG" -- "$@")"
   if test $? != 0; then
      echo "$PROG: Terminating..." >&2
      exit 1
   fi
   eval set -- "$TEMP"
fi

PREFIX=""
linkit=no
while test "$1"; do
	case "$1" in
	-d) PREFIX="$2"; shift ;;
	-h) echo "usage: $(basename "$0") [-d <install dir>] [-l]"; exit 0 ;;
	-l) linkit=yes;;
	esac
	shift
done

X="$(pwd -P)"
if test "x$PREFIX" != x -a "x${PREFIX:0:1}" != "x/"; then
	PREFIX="$X/$PREFIX"
fi


# create dirs
test -d "$PREFIX/sbin" || mkdir -p "$PREFIX/sbin"
test -d "$PREFIX/etc" || mkdir -p "$PREFIX/etc"

# copy or link tools
cd "$PREFIX/sbin"
if test "$linkit" = yes; then
	ln -sf "$X/tool/ldt" .
	ln -sf "$X/ext/frlib/tools/ipool" .
else
	cp -vf "$X/tool/ldt" .
	cp -vf "$X/ext/frlib/tools/ipool" .
fi


# create config files
if ! test -e "$PREFIX/etc/ldt.rc"; then
	cat << . > "$PREFIX/etc/ldt.rc"
logfile="/var/log/ldt.log"
.
fi
if ! test -e "$PREFIX/etc/ipool.rc"; then
	cat << . > "$PREFIX/etc/ipool.rc"
write_log=no
.
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
