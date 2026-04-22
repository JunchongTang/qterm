# QTerm Roadmap

This roadmap is derived from DESIGN.md and focuses on execution order.

## Phase 0: Repository Bootstrap

Goals:

* Create source tree expected by the root CMake project.
* Establish public include layout.
* Add a minimal qterm library target.
* Add a minimal quick demo target that can instantiate the library shell.

Exit criteria:

* CMake configure succeeds.
* A demo target builds.
* The repository has a stable place for core, session, and quick code.

## Phase 1: Headless Core Skeleton

Goals:

* Define QTermCell, style attributes, cursor state, and mode state.
* Define primary buffer, alternate buffer, and history abstractions.
* Implement a debug plain-text projection for tests.

Exit criteria:

* Headless tests can create a terminal core.
* Rows, columns, cursor movement, and plain text projection work without UI.

## Phase 2: Parser MVP

Goals:

* Implement byte decoding, parser skeleton, and input executor.
* Support printable text, CR, LF, BS, HT, basic SGR, erase, and cursor movement.
* Add a write buffer that preserves parse order.

Exit criteria:

* Headless tests validate basic VT behavior.
* The core can process real shell output for simple prompts.

## Phase 3: Resize and Reflow Correctness

Goals:

* Implement logical-line-based reflow.
* Preserve history and visible screen semantics across resize.
* Add regression tests for repeated narrow/wide cycles.

Exit criteria:

* Reflow golden tests pass for ASCII, CJK, and combining text.
* No lossy reconstruction through parser replay.

## Phase 4: Qt Quick First Frontend

Goals:

* Add QTermSurfaceModel.
* Add QTermQuickItem with incremental rendering.
* Support selection visuals, cursor rendering, and clipboard basics.

Exit criteria:

* A Qt Quick demo shows live terminal content.
* Dirty-line updates trigger partial redraws.

## Phase 5: Local PTY Integration

Goals:

* Implement Unix PTY backend.
* Wire terminal resize propagation.
* Validate interactive shell workflows.

Exit criteria:

* The demo can run a local shell through PTY.
* Resize, scrollback, and prompt interaction are stable.

## Phase 6: Protocol Expansion

Goals:

* Add DEC private modes, alternate screen, bracketed paste, mouse tracking, and OSC title handling.
* Add hyperlink support via OSC 8.

Exit criteria:

* Common CLI tools such as vim, less, htop, and interactive shells behave acceptably.

## Phase 7: Additional Session Backends

Goals:

* Add serial backend.
* Add SSH backend.
* Keep all transports behind the same session abstraction.

Exit criteria:

* The same QTerm core and Qt Quick frontend can drive PTY, serial, and SSH sessions.

## Phase 8: API Stabilization and Adapters

Goals:

* Refine public API.
* Add QWidget adapter over the same surface model.
* Document stable extension points.

Exit criteria:

* Public headers are reviewed for compatibility.
* QWidget is an adapter, not a second core implementation.

## Cross-Phase Test Matrix

Each phase should extend tests in these areas:

* Parser unit tests
* Buffer and history tests
* Reflow golden tests
* Session integration tests
* Qt Quick smoke tests

## Current Immediate Next Actions

1. Scaffold the source tree and CMake targets expected by the root project.
2. Add the first headless core types and a plain-text projection path.
3. Add tests that lock down row storage and resize invariants before richer protocol work begins.