# Benchmarks

Throughput tools for the parser and the two renderers. They **report** numbers;
they never assert them.

That split is deliberate. A wall-clock threshold has to be retuned per machine
and per build type — a Debug build is roughly 2.5x slower — and the failures it
produces are noise rather than signal. The properties worth failing a build over
are invariants, and those live in the ordinary tests: see
[`../widget/QTermWidgetRepaintTest.cpp`](../widget/QTermWidgetRepaintTest.cpp),
which catches the regression class that a timing threshold is worst at catching.

## Building and running

Off by default, because the tools need a real window and a few hundred megabytes
of generated payloads:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DQTERM_BUILD_BENCHMARKS=ON
cmake --build build
ctest --test-dir build -L benchmark -V
```

The payloads are generated once by a ctest fixture; `make_payloads.sh` leaves
whatever already exists alone, so re-runs are cheap. To generate them by hand:

```bash
tests/benchmarks/make_payloads.sh [output-dir]     # default /tmp/qterm-bench
```

Every run prints one machine-readable line, so a sweep can be collected without
parsing the prose:

```
RESULT	tool=realtime_widget	payload=cjk.txt	seconds=1.170	repaints=91	...
```

## The tools

| Tool | Measures |
|---|---|
| `coremem` | Parser only: fed from memory, no PTY, no rendering. The one yardstick comparable with browser-based terminal benchmarks. |
| `realtime` | Last line on screen, Qt Quick scene graph. |
| `realtime_widget` | Last line on screen, Qt Widgets. Also counts repaints and checks that the last paint drew the final content. |
| `shaping` | Full text shaping versus glyph-index lookup. |

## Reading the numbers

**Release builds only.** Debug is about 2.5x slower and inflates the apparent
benefit of any optimisation.

**Compare like with like.** The two `realtime` tools default to different window
sizes; the ctest wiring pins both to the same one so the grids match. A number
taken at a different grid size is not comparable.

**The two timings must agree.** Each end-to-end run prints both when the last
line reached the screen and when the child process exited. In a healthy
implementation these coincide. If they diverge, something is buffering and the
timing no longer measures what it claims to.

**Pick a marker that really is on the last line.** These tools stop when a marker
string appears, so a marker that also occurs earlier — or never — silently
measures the wrong thing or hangs.

**Throughput alone does not validate a rendering change.** The marker poll reads
the *buffer*, not the screen. A repaint bug shows up as a screen left behind the
buffer while every timing still looks correct, which is why `realtime_widget`
separately reports whether the last paint drew the final content.
