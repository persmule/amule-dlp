#! /bin/sh

# This script gets called by /etc/init.d/amule-daemon, and its purpose
# is to honour the AMULED_HOME variable in /etc/default/amule-daemon.
# This can't be done in the init script itself because HOME gets reset
# by start-stop-daemon on --chuid, or so it seems.
#
# It also sets up a sensible locale, needed by amuled to properly handle
# files with non-ASCII characters.

. /etc/default/amule-daemon

if [ -n "$AMULED_HOME" ]; then
    HOME="$AMULED_HOME"
    export HOME
fi

# Test whether we have some LANG variable set; if we do, do not read
# /etc/default/locale.
: ${L:=$LC_ALL}
: ${L:=$LC_CTYPE}
: ${L:=$LANG}

if [ -z "$L" ] && [ -r /etc/default/locale ]; then
    . /etc/default/locale
    for v in "$LC_ALL" "$LC_CTYPE" "$LANG"; do
	if [ -n "$v" ]; then
	    LANG="$v"
	    export LANG
	    break
	fi
    done
fi

exec /usr/bin/amuled -f
