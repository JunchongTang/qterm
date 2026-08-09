#!/bin/bash
# Generates the benchmark payloads, by default under /tmp/qterm-bench.
#
#     make_payloads.sh [output-dir]
#
# Each one stresses a different path: plain ASCII takes the batched run, SGR
# exercises escape parsing, CJK forces the per-character path, long lines drive
# wrapping, and redraw hits cursor addressing rather than scrolling.
#
# Existing files are left alone: together they are a few hundred megabytes and
# take minutes to write, and ctest re-runs the generating fixture every time.
set -euo pipefail
OUT="${1:-/tmp/qterm-bench}"
mkdir -p "$OUT"

[ -s "$OUT/lines1m.txt" ] || seq 1 1000000 > "$OUT/lines1m.txt"

python3 - "$OUT" <<'PY'
import sys, pathlib
out = pathlib.Path(sys.argv[1])

def needed(name):
    path = out / name
    return not (path.exists() and path.stat().st_size > 0)

if needed("color1m.txt"):
  with (out / "color1m.txt").open("w") as f:
    for i in range(1000000):
        f.write(f"\x1b[32m▸\x1b[0m \x1b[1;34m/some/path/segment-{i}\x1b[0m "
                f"\x1b[33mOK\x1b[0m \x1b[2mtail text here\x1b[0m\n")

if needed("cjk.txt"):
  with (out / "cjk.txt").open("w") as f:
    base = "中文宽字符测试终端渲染性能评估基准负载"
    for i in range(400000):
        f.write(f"{base} 第{i}行 mixed ascii tail\n")

if needed("longline.txt"):
  with (out / "longline.txt").open("w") as f:
    seg = "abcdefghij" * 50
    for _ in range(20000):
        f.write(seg * 4 + "\n")

if needed("redraw.txt"):
  with (out / "redraw.txt").open("w") as f:
    for i in range(300000):
        f.write(f"\rprogress {i:>8} [{'#' * (i % 40):<40}] working...")
    f.write("\n")

if needed("mixed.txt"):
  with (out / "mixed.txt").open("w") as f:
    for i in range(300000):
        f.write(f"\x1b[32m✔\x1b[0m \x1b[1;34m/path/seg-{i}\x1b[0m "
                f"\x1b[33m中文\x1b[0m \U0001F680 café \x1b[2mtail\x1b[0m\n")
PY

# CRLF variants: a bare LF moves down without returning to column 0, so feeding
# a file straight into a parser (no pty to translate) produces staircased text
# and measures the wrong thing.
[ -s "$OUT/lines1m_crlf.txt" ] || perl -pe 's/\n/\r\n/' "$OUT/lines1m.txt" > "$OUT/lines1m_crlf.txt"
[ -s "$OUT/color1m_crlf.txt" ] || perl -pe 's/\n/\r\n/' "$OUT/color1m.txt" > "$OUT/color1m_crlf.txt"

ls -la "$OUT"/*.txt | awk '{printf "  %-28s %6.1f MB\n", $9, $5/1048576}'
