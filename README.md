# QTerm

[![Qt](https://img.shields.io/badge/Qt-6.8%2B-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C?logo=cmake&logoColor=white)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

English | [简体中文](README.zh-CN.md)

Qt-native terminal emulator library for Qt Quick and QWidget.

## Overview

QTerm is a terminal stack for modern Qt applications. It provides a reusable core for terminal state, protocol parsing, rendering integration, and session backends.

The project is designed with a core-first approach:

- headless core correctness before UI expansion
- transport-agnostic session abstraction
- Qt Quick first, QWidget supported
- incremental rendering and testability

## Architecture Layers

QTerm is organised into four layers, each with a single responsibility:

**1. Session layer** — connects to a real terminal source and moves raw bytes.
Backends include local PTY, Windows ConPTY, serial, SSH and custom remote
streams. It owns connecting, reading and writing bytes, propagating window-size
changes, and handling interruption, close, errors and reconnection. It does no
VT parsing and keeps no screen state.

**2. Protocol layer** — parses the terminal protocol into VT/ANSI semantics:
UTF-8 decoding, C0/C1 controls, ESC/CSI/OSC/DCS/APC/PM sequences, SGR
attributes, cursor movement, erasing, and insertion/deletion.

**3. Core model layer** — owns terminal state: the cell buffer, scrollback,
cursor, modes, selection, reflow on resize, and dirty tracking. This is where
correctness is decided, and it is fully testable headless.

**4. Rendering and interaction layer** — projects core state into something a
frontend can draw, and turns input into protocol bytes. Both the Qt Quick and
QWidget frontends sit on this layer rather than reimplementing the core.

## Highlights

- Qt-only implementation with minimal external runtime dependencies
- VT/ANSI parsing pipeline with terminal state modeling
- scrollback, selection, and resize reflow support
- multiple session backends: local shell, serial, telnet
- dual frontend path: Qt Quick and QWidget

## Repository Layout

```text
qterm/
  include/QTerm/                Public headers
  src/                          Core library and implementations
  examples/qtquick-terminal/    Qt Quick demo
  examples/qwidget-terminal/    QWidget demo
  tests/                        Unit and integration tests
  docs/qdoc/                    Public API docs (QDoc source)
```

## Requirements

- Qt 6.8 or newer (development baseline; older Qt 6 versions are untested but may still be compatible)
- CMake 3.25 or newer
- C++17 toolchain
- macOS / Linux / Windows (platform support evolves with current implementation)

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Build options:

- `QTERM_BUILD_EXAMPLES=ON|OFF` (default ON)
- `QTERM_BUILD_TESTS=ON|OFF` (default ON)

## Run Demos

Qt Quick demo:

```bash
./build/examples/qtquick-terminal/qtquick-terminal
```

QWidget demo:

```bash
./build/examples/qwidget-terminal/qwidget-terminal
```

If your generator uses a different output directory, locate the binaries under your build tree.

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Documentation

Public repository currently keeps API documentation sources in:

- [docs/qdoc/qterm.qdocconf](docs/qdoc/qterm.qdocconf)
- [docs/qdoc/qterm-cpp-module.qdoc](docs/qdoc/qterm-cpp-module.qdoc)
- [docs/qdoc/qterm-qml-module.qdoc](docs/qdoc/qterm-qml-module.qdoc)

Generate QDoc locally:

```bash
qdoc docs/qdoc/qterm.qdocconf
```

Internal and Chinese working documents are maintained in the private repository.

## API Snapshot

Major public types include:

- `QTermTerminal`
- `QTermSession`
- `QTermSurfaceModel`
- `QTermQuickItem`
- `QTermQuickPaintedItem`
- `QTermWidget`

## Roadmap Direction

Near-term engineering focus:

- protocol completeness and behavior parity for common CLI tools
- rendering and interaction quality in Qt Quick frontend
- backend robustness across local shell / serial / telnet scenarios
- public API stabilization and test coverage growth

## Contributing

Issues and pull requests are welcome.

Suggested contribution flow:

1. Create an issue to discuss changes before large work.
2. Add or update tests together with code changes.
3. Keep public APIs and behavior changes clearly documented.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).
