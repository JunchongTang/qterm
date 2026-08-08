#!/usr/bin/env python3
"""Generates the reference-test corpus.

Each case is a directory holding case.json (terminal geometry) and input.bin
(the raw bytes to replay). Snapshots are produced separately by running the
test binary with QTERM_UPDATE_REFS=1.

Cases come from two sources:
  - synthetic sequences written here, covering behaviour that is hard to
    trigger reliably from a real program;
  - recordings captured from real commands via record_case.sh.

Run from anywhere:  python3 tests/refdata/generate_cases.py
"""

import json
import os
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent

ESC = b"\x1b"
CSI = ESC + b"["


def case(name, data, columns=80, rows=24, scrollback=None, chunk=4096):
    directory = ROOT / name
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "input.bin").write_bytes(data)
    config = {"columns": columns, "rows": rows, "chunk": chunk}
    if scrollback is not None:
        config["scrollback"] = scrollback
    (directory / "case.json").write_text(json.dumps(config, indent=2) + "\n")
    print(f"  {name:<34} {len(data):>7} bytes")


def sgr(*codes):
    return CSI + b";".join(str(c).encode() for c in codes) + b"m"


# ── Plain output and scrolling ───────────────────────────────────────────────

case("plain-ascii",
     b"".join(b"line %d\r\n" % i for i in range(1, 40)))

case("scroll-past-screen",
     b"".join(b"%d\r\n" % i for i in range(1, 501)),
     rows=10, scrollback=50)

case("long-lines-wrap",
     b"".join(bytes(str(i % 10), "ascii") * 200 + b"\r\n" for i in range(6)),
     columns=40, rows=12)

# ── SGR ──────────────────────────────────────────────────────────────────────

case("sgr-basic",
     sgr(1) + b"bold " + sgr(3) + b"italic " + sgr(4) + b"under " +
     sgr(7) + b"inverse " + sgr(0) + b"plain\r\n" +
     sgr(31) + b"red " + sgr(42) + b"on-green " + sgr(0) + b"\r\n" +
     sgr(90) + b"bright-fg " + sgr(107) + b"bright-bg" + sgr(0) + b"\r\n")

case("sgr-256-and-truecolor",
     sgr(38, 5, 196) + b"idx-196 " + sgr(48, 5, 21) + b"bg-21 " + sgr(0) + b"\r\n" +
     sgr(38, 2, 255, 128, 0) + b"rgb-orange " +
     sgr(48, 2, 0, 64, 128) + b"rgb-bg" + sgr(0) + b"\r\n")

case("sgr-reset-partial",
     sgr(1, 4, 31) + b"all " + sgr(22) + b"nobold " + sgr(24) + b"nounder " +
     sgr(39) + b"defaultfg" + sgr(0) + b"\r\n")

# ── Cursor movement and erasing ──────────────────────────────────────────────

case("cursor-addressing",
     CSI + b"2J" + CSI + b"H" + b"top-left" +
     CSI + b"5;10H" + b"row5-col10" +
     CSI + b"3;1H" + b"row3" +
     CSI + b"10;1H" + b"row10" + b"\r\n")

case("erase-operations",
     b"".join(b"AAAAAAAAAAAAAAAAAAAA\r\n" for _ in range(6)) +
     CSI + b"2;5H" + CSI + b"K" +      # erase to end of line
     CSI + b"3;5H" + CSI + b"1K" +     # erase to start of line
     CSI + b"4;1H" + CSI + b"2K" +     # erase whole line
     CSI + b"5;3H" + CSI + b"J")       # erase to end of screen

case("insert-delete",
     b"abcdefghij\r\n" +
     CSI + b"1;4H" + CSI + b"3@" +     # insert 3 blanks
     CSI + b"2;1H" + b"0123456789\r\n" +
     CSI + b"2;3H" + CSI + b"4P" +     # delete 4 chars
     CSI + b"5;1H" + b"keep\r\n" +
     CSI + b"5;1H" + CSI + b"2L" +     # insert 2 lines
     CSI + b"9;1H" + b"tail\r\n")

# ── Scroll regions ───────────────────────────────────────────────────────────

case("scroll-region",
     CSI + b"3;8r" +                   # margins rows 3..8
     CSI + b"3;1H" +
     b"".join(b"region-%d\r\n" % i for i in range(1, 15)) +
     CSI + b"r" +                      # reset margins
     CSI + b"12;1H" + b"after\r\n",
     rows=14)

# ── Wide characters and combining marks ──────────────────────────────────────

case("cjk-wide",
     "宽字符测试 CJK mixed\r\n".encode() +
     "一二三四五六七八九十\r\n".encode() +
     b"ascii tail\r\n",
     columns=24, rows=8)

case("wide-at-line-edge",
     b"x" * 23 + "宽".encode() + b"after\r\n" +
     b"y" * 22 + "宽宽".encode() + b"\r\n",
     columns=24, rows=8)

# A run of narrow characters can land on a wide character at either end: its
# start may sit on a continuation cell, and its end may cover the leading cell
# while leaving the continuation behind. Both need the stale half cleared.
case("narrow-run-over-wide",
     "xx宽yy\r\n".encode() +
     CSI + b"1;1H" + b"abc" +          # run ends on the wide char's leading cell
     CSI + b"2;1H" + "AB宽CD\r\n".encode() +
     CSI + b"2;4H" + b"zz" +           # run starts on the continuation cell
     CSI + b"3;1H" + "宽宽宽\r\n".encode() +
     CSI + b"3;2H" + b"q" +            # single write onto a continuation cell
     CSI + b"4;1H" + "四五六七\r\n".encode() +
     CSI + b"4;3H" + b"mnop",          # run spanning two wide characters
     columns=20, rows=8)

case("combining-marks",
     "é à ö base+mark\r\n".encode() +
     "ź̧ double-mark\r\n".encode(),
     columns=30, rows=6)

case("emoji",
     "emoji: \U0001F600 \U0001F680 tail\r\n".encode(),
     columns=30, rows=6)

# ── Alternate screen ─────────────────────────────────────────────────────────

case("alt-screen",
     b"main-1\r\nmain-2\r\nmain-3\r\n" +
     CSI + b"?1049h" +                 # enter alt screen
     b"alt-a\r\nalt-b\r\n" +
     CSI + b"?1049l" +                 # leave, main should be restored
     b"main-4\r\n")

# ── Tabs, CR/LF edge cases ───────────────────────────────────────────────────

case("tabs",
     b"a\tb\tc\td\r\n" +
     b"12345678\t9\r\n" +
     CSI + b"3g" + b"x\ty\r\n")

case("carriage-return-overwrite",
     b"aaaaaaaaaa\rbbbb\r\n" +
     b"progress 10%\rprogress 100%\r\n")

# ── Malformed and unsupported sequences ──────────────────────────────────────
# These must not leak their payload onto the screen. They currently do in some
# cases; the snapshots record today's behaviour so the corpus shows movement
# when the parser is fixed.

case("truncated-sequences",
     b"before " + CSI + b"31" +        # CSI cut off mid-parameter
     b" after\r\n" +
     b"x " + ESC +                     # lone ESC at end of chunk
     b"y\r\n")

case("unknown-csi-final",
     b"a" + CSI + b"1;2Z" + b"b\r\n" +
     b"c" + CSI + b"?99h" + b"d\r\n")

case("osc-sequences",
     ESC + b"]0;window title\x07" + b"after-title\r\n" +
     ESC + b"]8;;https://example.com\x07" + b"link text" +
     ESC + b"]8;;\x07" + b" plain\r\n")

print()
print("Regenerate snapshots with:  QTERM_UPDATE_REFS=1 ./qterm_ref_tests")
