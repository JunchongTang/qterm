# QTerm Design

## 1. Purpose

QTerm is a Qt-native terminal library designed for Qt Quick first, with a shared core that can later back QWidget and headless use cases.

This document is the canonical product and architecture specification for the repository. README explains intent; this file defines the actual boundaries, responsibilities, invariants, and implementation direction.

## 2. Product Definition

QTerm is not a single widget. It is a layered terminal stack made of:

1. Session backends that connect to PTY, ConPTY, serial, SSH, or custom byte streams.
2. A protocol pipeline that turns raw bytes into terminal operations.
3. A terminal core that owns buffers, history, modes, cursor state, and resize behavior.
4. A presentation layer that projects core state into efficient viewport snapshots.
5. Frontends such as a Qt Quick item and, later, a QWidget adapter.

The primary deliverable is a correct and testable terminal core. UI components are consumers of that core.

## 3. Reference Model: xterm.js

QTerm mainly references xterm.js for architectural separation, not for language-level structure or browser-specific implementation details.

The parts of xterm.js worth mirroring conceptually are:

* A headless terminal core separated from the browser frontend.
* A dedicated buffer service and mode state instead of embedding terminal state into the widget.
* A parser plus input handler split, where parsing and command execution are distinct concerns.
* A write buffer between incoming data and parser execution.
* Independent services for selection, rendering, mouse handling, and links built on top of stable terminal state.
* Heavy test coverage around parser behavior and buffer semantics.

QTerm should not copy xterm.js one-to-one. In particular:

* QTerm should use Qt object ownership, signals, slots, and value types where appropriate.
* QTerm should not reproduce xterm.js's browser service graph mechanically.
* QTerm should keep public API smaller than xterm.js at the start.

## 4. Non-Goals

The following are explicitly not first-phase goals:

* Plugin or addon ecosystems.
* IDE-specific shell integration features.
* Terminal multiplexing.
* Embedding a third-party terminal engine as the semantic core.
* Feature-complete emulation before core architecture is stable.

## 5. Core Principles

### 5.1 Qt-only Core Implementation

The terminal engine is implemented within QTerm using Qt and C++. QTerm does not delegate core semantics such as resize reflow, scrollback reconstruction, or screen history restoration to libvterm or another terminal engine.

### 5.2 Headless-first Architecture

The core must be usable without any GUI object. Parsing, state updates, resize logic, and history management must be testable without QQuickItem or QWidget.

### 5.3 Transport-agnostic Sessions

PTY, serial, SSH, and remote streams are all byte-oriented transports. The terminal core must not know which transport is active.

### 5.4 Logical Semantics Over Physical Rows

QTerm must preserve logical line semantics and cell-level content independent of current viewport width. Physical rows are projections of the current width, not the canonical content model.

### 5.5 Renderer Does Not Own Terminal State

Renderers consume immutable snapshots or incremental projections. They do not mutate buffer state directly.

### 5.6 Testability Is a Design Constraint

Parser behavior, buffer mutations, selection projection, and resize reflow must each be testable in isolation.

## 6. Layered Architecture

## 6.1 Public API Layer

This layer exposes stable application-facing types.

Planned public objects:

* QTermTerminal: High-level facade for terminal core plus session attachment.
* QTermSession: Transport-facing session binding.
* QTermSurfaceModel: Read-only presentation model for UI consumers.
* QTermQuickItem: Qt Quick frontend.
* QTermProfile: User-facing options such as font-independent terminal defaults, scrollback limit, keyboard behavior, and feature flags.

Responsibilities:

* Lifecycle orchestration.
* High-level events such as title change, bell, resize, and state changes.
* Public configuration surface.
* Stable API boundaries that hide internal service decomposition.

## 6.2 Session Layer

This layer connects to the external world.

Key abstractions:

* QTermSessionBackend: Abstract interface for a duplex terminal transport.
* QTermLocalPtyBackend: Unix PTY implementation.
* QTermConPtyBackend: Windows backend.
* QTermSerialBackend: Serial transport.
* QTermSshBackend: SSH transport.

Required backend capabilities:

* open
* close
* write bytes
* resize terminal
* notify about received bytes
* notify about errors and exit conditions
* expose connection state

Hard rule:

The backend only moves bytes and connection state. It does not parse terminal sequences and does not own terminal screen state.

## 6.3 Input Pipeline Layer

This layer converts raw incoming data into terminal operations.

Planned components:

* QTermByteDecoder
* QTermEscapeSequenceParser
* QTermInputExecutor
* QTermWriteBuffer

Responsibilities split:

* QTermByteDecoder: Decode UTF-8 and preserve control bytes.
* QTermEscapeSequenceParser: Maintain the VT parser state machine for ESC, CSI, OSC, DCS, APC, PM, and execute transitions.
* QTermInputExecutor: Translate parser events into core model mutations.
* QTermWriteBuffer: Preserve incoming write order and enable chunking, backpressure, and future async parser handlers if needed.

This is the closest QTerm equivalent to xterm.js's parser plus InputHandler plus WriteBuffer combination.

## 6.4 Core Model Layer

This is the semantic heart of QTerm.

Planned components:

* QTermCore
* QTermBufferManager
* QTermScreenBuffer
* QTermHistoryStore
* QTermCursorState
* QTermModeState
* QTermTabStopMap
* QTermDirtyTracker
* QTermSelectionModel
* QTermMarkerStore

Core model responsibilities:

* Maintain primary and alternate screen buffers.
* Maintain scrollback history and viewport state.
* Own cursor, margins, tab stops, insert mode, origin mode, wrap mode, and DEC private modes.
* Preserve cell-level data including width and combining behavior.
* Track dirty regions for incremental presentation and rendering.
* Execute resize behavior, including logical reflow.
* Provide search-friendly and selection-friendly projections without losing semantic fidelity.

### Buffer Model

The core buffer model should distinguish:

* Logical lines: canonical content flow units.
* Wrapped screen rows: current-width projections of logical lines.
* Scrollback entries: logical history that remains valid across resizes.
* Visible viewport rows: a window over the projected rows.

QTerm must not treat the current visible physical row list as the single source of truth.

### Cell Model

Each visible position should be derived from a richer cell representation.

Planned QTermCell fields:

* Base codepoint or grapheme payload.
* Combining sequence payload where applicable.
* Display width.
* Style attributes.
* Flags such as continuation, wrapped boundary participation, protected state, and hyperlink attachment.

The exact storage can evolve for performance, but the semantic model must preserve:

* Wide characters.
* Combining marks.
* Zero-width continuation semantics.
* Style reset boundaries.
* Selection-safe text extraction.

### Modes and State

The core must own modes similar in responsibility to xterm.js's CoreService and related mode state.

Examples:

* Insert mode
* Wraparound mode
* Origin mode
* Bracketed paste mode
* Mouse tracking mode
* Alternate screen mode
* Cursor visibility and style state
* Application cursor keys mode
* Focus event mode

## 6.5 Presentation Layer

This layer projects the core into frontend-friendly read models.

Planned components:

* QTermSurfaceModel
* QTermViewportSnapshot
* QTermStyleRun
* QTermRenderSpan
* QTermSelectionSnapshot

Responsibilities:

* Produce viewport rows without exposing internal storage directly.
* Group cells into runs suitable for efficient rendering.
* Expose cursor, selection, and hyperlink overlays.
* Emit dirty line ranges or changed regions.
* Offer plain-text projection for testing and search without becoming the canonical storage form.

This layer exists so frontends can stay simple and fast.

## 6.6 Frontend Layer

This layer implements the actual UI.

Primary target:

* QTermQuickItem

Secondary future target:

* QTermWidgetAdapter

Qt Quick frontend responsibilities:

* Incremental rendering using QSGNode.
* Font metrics integration.
* Text selection visuals.
* Cursor painting and blinking policy hookup.
* IME composition support.
* Mouse hit testing and drag selection.
* Clipboard integration.
* Key mapping and encoded input emission.

The frontend must never become the owner of core buffer semantics.

## 7. Recommended Data Flow

Incoming data flow:

1. Session backend emits raw bytes.
2. QTermWriteBuffer serializes incoming chunks.
3. QTermByteDecoder converts them into parser-consumable units.
4. QTermEscapeSequenceParser emits terminal operations.
5. QTermInputExecutor mutates QTermCore and dirty trackers.
6. QTermSurfaceModel refreshes changed projections.
7. QTermQuickItem redraws affected regions only.

Outgoing data flow:

1. User input reaches QTermQuickItem.
2. Input mapping converts Qt key, text, mouse, wheel, and paste events into terminal byte sequences.
3. QTermSession writes encoded bytes to the active backend.

## 8. Resize and Reflow Strategy

Resize behavior is a first-class design topic, not an implementation detail.

QTerm adopts the following invariants:

1. Reflow must operate on logical content, not on already reflowed physical rows.
2. Scrollback and visible screen content must not be merged through lossy physical-row reconstruction.
3. Alternate screen content is isolated from primary scrollback.
4. Wide characters, combining marks, and style runs must survive width changes without corruption.
5. Repeated narrow-widen-narrow cycles must be idempotent with respect to logical content.

Practical rules:

* Keep canonical logical content separate from current row projection.
* Preserve explicit hard line breaks separately from soft wraps.
* Resize the core model immediately.
* Debounce backend resize notifications where needed to reduce shell redraw storms, but do not debounce internal semantic resize.
* Never replay historical content through the parser to simulate reflow.

## 9. Protocol Coverage Strategy

The parser should be built in vertical slices instead of attempting full xterm compatibility at once.

Priority order:

### Stage A

* Printable text
* UTF-8 decode
* CR, LF, CRLF, BS, HT
* Basic SGR
* Cursor addressing
* Erase functions
* Insert/delete character and line basics

### Stage B

* DECSET/DECRST private modes
* Scroll regions
* Save/restore cursor
* Alternate screen
* Device status reports and attributes as needed

### Stage C

* OSC title
* OSC 8 hyperlinks
* Bracketed paste
* Mouse tracking encodings
* Focus reporting

### Stage D

* More DCS and advanced xterm compatibility features where justified by real clients

Protocol additions should be driven by tests and real interoperability needs.

## 10. Public API Direction

The initial public API should stay narrow.

Minimum first-wave API goals:

* Create terminal with rows and columns.
* Attach and detach session backend.
* Feed incoming bytes for tests and headless use.
* Resize terminal.
* Read viewport and plain-text debug projection.
* Access title, cursor, scroll position, and selection state.
* Send user input through the session.

Avoid exposing mutable internals such as direct access to raw buffer lines in the first public revision.

## 11. Source Layout

The intended source layout is:

* include/QTerm
* src/core
* src/core/buffer
* src/core/parser
* src/core/presentation
* src/session
* src/session/local
* src/session/remote
* src/platform
* src/quick
* tests/core
* tests/session
* tests/quick
* examples/quick-demo

Design intent by area:

* include/QTerm: public headers only.
* src/core: terminal semantics and shared non-UI logic.
* src/session: session abstractions and transport backends.
* src/platform: platform-specific helpers that should not pollute core semantics.
* src/quick: Qt Quick integration.

## 12. Testing Strategy

QTerm needs a testing pyramid shaped around terminal correctness.

### Unit tests

For:

* UTF-8 decoding
* parser transitions
* input execution commands
* cell and line behavior
* style handling
* mode transitions

### Golden tests

For:

* wide character layout
* combining character behavior
* wrap and unwrap behavior
* narrow-to-wide-to-narrow idempotence
* alternate screen separation
* selection extraction across wraps

### Integration tests

For:

* PTY session round trips
* resize propagation
* mouse protocol emission
* bracketed paste behavior

### Frontend tests

For:

* viewport synchronization
* selection interaction
* rendering damage tracking

## 13. Initial Mapping from xterm.js Concepts to QTerm

This mapping is conceptual only.

* xterm.js Terminal/CoreTerminal -> QTermTerminal/QTermCore
* xterm.js BufferService -> QTermBufferManager
* xterm.js CoreService -> QTermModeState plus core event hub
* xterm.js InputHandler -> QTermInputExecutor
* xterm.js WriteBuffer -> QTermWriteBuffer
* xterm.js SelectionService -> QTermSelectionModel plus frontend controller
* xterm.js RenderService -> QTermSurfaceModel plus QTermQuick renderer path
* xterm.js headless terminal -> QTerm headless core test harness

The point of this mapping is to preserve separation of concerns, not to force identical class topology.

## 14. Immediate Implementation Order

The recommended implementation order is:

1. Define core data types and invariants.
2. Build a headless buffer manager plus cursor and history model.
3. Implement the minimal parser and input executor.
4. Add surface projection and debug plain-text projection.
5. Connect a local PTY backend.
6. Build the first Qt Quick renderer and input bridge.
7. Expand protocol coverage with regression tests.
8. Add serial and SSH backends.

This order is chosen to maximize correctness and testability before UI polish.

## 15. Decision Summary

QTerm is a Qt-native terminal stack inspired by xterm.js architecture, but with its own core implementation. Its most important technical decision is to own terminal semantics directly, especially buffer history and resize reflow, because those determine whether the terminal is trustworthy under real workloads.