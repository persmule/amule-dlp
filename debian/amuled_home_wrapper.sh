#! /bin/sh

# This script gets called by /etc/init.d/amule-daemon, and its purpose
# is to honour the AMULED_HOME variable in /etc/default/amule-daemon.
# This can't be done in the init script itself because HOME gets reset
# by start-stop-daemon on --chuid, or so it seems.

. /etc/default/amule-daemon

if [ -n "$AMULED_HOME" ]; then
    HOME="$AMULED_HOME"
    export HOME
fi

exec /usr/bin/amuled -f
