#!/bin/sh
# Runs hiped's tests on a private display and hiped (Xephyr, own socket and keyfile), against the hiped and
# libhipe built in this tree. Needs Xephyr and xdotool. Build first: make in api/ and server/.
#   server/tests/run.sh               the location and markup tests
#   server/tests/run.sh memory        hiped's memory over repeated rebuilds of a 500-line group (must stay flat)
#   server/tests/run.sh bench [n]     APPEND_TAG and mode 3 timings, n instructions each (default 20000)
# TEST_DISPLAY picks the display (default :95). Exit status is non-zero if a test failed.
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
disp=${TEST_DISPLAY:-:95}
work=$(mktemp -d /tmp/hipe-tests-XXXXXX)
mode=${1:-tests}

if [ -e "/tmp/.X11-unix/X${disp#:}" ]; then echo "display $disp is in use; set TEST_DISPLAY" >&2; exit 2; fi
for t in locations markup bench; do
    cc -Wall -O2 "$here/$t.c" -o "$work/$t" -I"$root/api/src" "$root/api/build/libhipe.a" -lpthread || exit 2
done

Xephyr "$disp" -screen 1024x768 -ac > "$work/xephyr.log" 2>&1 &
xephyr=$!
sleep 2
(cd "$root/server" && exec env -u WAYLAND_DISPLAY DISPLAY=$disp QT_QPA_PLATFORM=xcb ./hiped --fill \
    --socket "$work/h.sock" --keyfile "$work/h.key" > "$work/hiped.log" 2>&1) &
hiped=$!
i=0; while ! grep -q Listening "$work/hiped.log" 2>/dev/null && [ $i -lt 50 ]; do sleep 0.2; i=$((i+1)); done
export DISPLAY=$disp HIPE_SOCKET="$work/h.sock" HIPE_KEYFILE="$work/h.key"
hiped_pid=$(pgrep -f "hiped --fill --socket $work/h.sock" | head -1)
rss() { awk '/VmRSS/ {print $2}' /proc/$hiped_pid/status; }

status=0
case $mode in
tests)
    "$work/locations" || status=1
    "$work/markup" || status=1
    ;;
memory)
    "$work/locations" 600 > "$work/memory.log" 2>&1 &
    c=$!
    for k in 100 600; do
        while ! grep -q "REBUILT $k\$" "$work/memory.log" && kill -0 $c 2>/dev/null; do sleep 0.5; done
        eval "rss$k=$(rss)"
        echo "after $k rebuilds: hiped RSS $(rss) kB"
    done
    wait $c || status=1
    # flat: six times the rebuilds may not use more than 5% more
    [ "$rss600" -le $((rss100 * 105 / 100)) ] || { echo "FAIL memory grew"; status=1; }
    ;;
bench)
    "$work/bench" "${2:-20000}" || status=1
    ;;
esac

kill $hiped 2>/dev/null; [ -n "$hiped_pid" ] && kill "$hiped_pid" 2>/dev/null
sleep 1; kill $xephyr 2>/dev/null
[ $status = 0 ] && rm -rf "$work" || echo "logs: $work"
exit $status
