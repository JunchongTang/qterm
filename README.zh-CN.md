# QTerm

[![Qt](https://img.shields.io/badge/Qt-6.8%2B-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C?logo=cmake&logoColor=white)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

[English](README.md) | 简体中文

面向 Qt Quick 与 QWidget 的 Qt 原生终端模拟器组件库。

## 项目概览

QTerm 是一个面向现代 Qt 应用的终端能力栈，提供可复用的终端核心、协议解析、渲染集成与会话后端抽象。

项目采用“核心优先”设计：

- 先保证无头核心正确性，再扩展 UI
- 会话层与传输方式解耦
- Qt Quick 优先，同时支持 QWidget
- 强调增量渲染与可测试性

## 核心特性

- Qt 原生实现，尽量减少外部运行时依赖
- VT/ANSI 解析与终端状态建模
- 支持滚动历史、选择、窗口尺寸重排
- 多会话后端：本地 shell、串口、telnet
- 双前端路径：Qt Quick 与 QWidget

## 仓库结构

```text
qterm/
  include/QTerm/                公开头文件
  src/                          核心库与实现代码
  examples/qtquick-terminal/    Qt Quick 示例
  examples/qwidget-terminal/    QWidget 示例
  tests/                        单元与集成测试
  docs/qdoc/                    公开 API 文档（QDoc 源文件）
```

## 环境要求

- Qt 6.8 或更高版本(当前开发基线；更低的 Qt 6 版本尚未验证，但不代表无法适配)
- CMake 3.25 或更高版本
- 支持 C++17 的编译器
- macOS / Linux / Windows（具体平台能力随当前实现持续完善）

## 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

构建选项：

- `QTERM_BUILD_EXAMPLES=ON|OFF`（默认 ON）
- `QTERM_BUILD_TESTS=ON|OFF`（默认 ON）

## 运行示例

Qt Quick 示例：

```bash
./build/examples/qtquick-terminal/qtquick-terminal
```

QWidget 示例：

```bash
./build/examples/qwidget-terminal/qwidget-terminal
```

如果你的生成器使用不同输出目录，请在 build 目录下按目标名查找可执行文件。

## 测试

```bash
ctest --test-dir build --output-on-failure
```

## 文档

公开仓库当前保留的文档主要是 API 文档源：

- [docs/qdoc/qterm.qdocconf](docs/qdoc/qterm.qdocconf)
- [docs/qdoc/qterm-cpp-module.qdoc](docs/qdoc/qterm-cpp-module.qdoc)
- [docs/qdoc/qterm-qml-module.qdoc](docs/qdoc/qterm-qml-module.qdoc)

本地生成 QDoc：

```bash
qdoc docs/qdoc/qterm.qdocconf
```

内部文档与中文工作文档维护在私有仓库中。

## 对外 API 快照

主要公开类型包括：

- `QTermTerminal`
- `QTermSession`
- `QTermSurfaceModel`
- `QTermQuickItem`
- `QTermQuickPaintedItem`
- `QTermWidget`

## 路线方向

近期工程重点：

- 完善协议覆盖并提升常见 CLI 工具行为一致性
- 提升 Qt Quick 前端渲染与交互质量
- 强化本地 shell / 串口 / telnet 后端稳定性
- 稳定公开 API 并扩展测试覆盖

## 参与贡献

欢迎提交 Issue 和 Pull Request。

建议贡献流程：

1. 对较大改动先通过 Issue 讨论方案。
2. 代码改动同时补充或更新测试。
3. 对外 API 和行为变化请同步更新文档说明。

## 许可证

本项目采用 MIT 许可证，详见 [LICENSE](LICENSE)。
