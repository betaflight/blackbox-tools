#!/bin/sh
# Regression test: DISARM (15) and FLIGHTMODE (30) event frames must be decoded and their payloads consumed.
# Usage: test/test_events.sh [path/to/blackbox_decode]
set -eu

DECODER=${1:-obj/blackbox_decode}
[ -x "$DECODER" ] || { echo "decoder not found: $DECODER (run make first)" >&2; exit 2; }
DECODER=$(cd "$(dirname "$DECODER")" && pwd)/$(basename "$DECODER")

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

# Fixtures: two I frames (iteration,time) with one event frame between them, then the clean-end event.
python3 - <<'PY'
def vb(n):
    out = bytearray()
    while n > 127:
        out.append((n & 0x7f) | 0x80)
        n >>= 7
    out.append(n & 0x7f)
    return bytes(out)

HDR = (b"H Product:Blackbox flight data recorder by Nicholas Sherlock\n"
       b"H Data version:2\n"
       b"H Field I name:loopIteration,time\n"
       b"H Field I signed:0,0\n"
       b"H Field I predictor:0,0\n"
       b"H Field I encoding:1,1\n")
END = b"E" + bytes([255]) + b"End of log\x00"

def iframe(it, t):
    return b"I" + vb(it) + vb(t)

def eframe(eid, *vals):
    return b"E" + bytes([eid]) + b"".join(vb(v) for v in vals)

def write(name, event):
    with open(name, "wb") as f:
        f.write(HDR + iframe(0, 1000) + event + iframe(1, 2000) + END)

write("disarm.bbl", eframe(15, 2))
write("flightmode.bbl", eframe(30, 0x01000001, 1))
# flags=73 encodes to the single byte 0x49 ('I'), which collides with the I frame marker.
write("desync.bbl", eframe(30, 73, 1))
PY

fail=0
check() {
    name=$1; pattern=$2
    "$DECODER" "$name.bbl" >/dev/null 2>&1 || { echo "FAIL $name: decoder exit status"; fail=1; return; }
    rows=$(grep -c '^ *[0-9]' "$name.01.csv" || true)
    [ "$rows" = 2 ] || { echo "FAIL $name: expected 2 data rows, got $rows"; fail=1; }
    grep -qF "$pattern" "$name.01.event" || { echo "FAIL $name: event line missing: $pattern"; fail=1; }
    grep -qF '"Log clean end"' "$name.01.event" || { echo "FAIL $name: clean end missing"; fail=1; }
}

check disarm      '"name":"Disarm", "time":1000, "data":{"reason":2}'
check flightmode  '"name":"Flight mode", "time":1000, "data":{"flags":16777217,"lastFlags":1}'
check desync      '"name":"Flight mode", "time":1000, "data":{"flags":73,"lastFlags":1}'

[ "$fail" = 0 ] && echo "PASS test_events"
exit "$fail"
