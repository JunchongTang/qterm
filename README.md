# QTerm

QTerm 是一个面向 Qt 生态的终端模拟器组件库，目标是提供一个适合 QML、QWidget、可扩展、可复用、协议可控的终端内核与高性能控件体系。

## 项目目标

QTerm 的长期目标是成为一个分层清晰的终端能力库，而不是单一控件：

* 为 Qt Quick 提供高性能终端控件。
* 提供统一的终端会话抽象，接入 PTY、串口、SSH 等常见后端。
* 自主实现终端协议解析、状态建模、滚动历史和重排逻辑。
* 保留 QWidget 接入空间，但不让 QWidget 约束核心架构。
* 最小化依赖，优先使用 Qt 自身能力完成系统集成。

当前开发基线：

* **Qt 6.8**
* **QML / Qt Quick 优先**
* **C++ 为核心实现语言**
* **仅依赖 Qt 构建核心终端实现**

## 分层架构

QTerm 计划采用如下分层：
* 会话层
* 协议层
* 状态模型层
* 渲染与交互层

### 1. Session Layer

职责：与真实终端源建立连接，收发字节流，管理生命周期。

典型后端包括：

* Local PTY backend
* Windows ConPTY backend
* Serial backend
* SSH backend
* 自定义 remote stream backend

这一层只解决：

* 如何连接
* 如何读写原始字节
* 如何传递窗口大小变化
* 如何处理中断、关闭、错误和重连

它不负责 VT 解析，不负责屏幕状态维护。

### 2. Protocol Layer

职责：解析终端输入输出协议，建立 VT/ANSI 语义。

这一层需要逐步覆盖：

* UTF-8 解码
* C0/C1 控制字符
* ESC / CSI / OSC / DCS / APC / PM 等序列
* SGR 样式属性
* 光标移动、擦除、插入删除
* DEC 私有模式
* Alternate screen
* Bracketed paste
* Mouse tracking
* Hyperlink OSC 8
* Title / clipboard / working directory 等 OSC 扩展

建议把协议层继续拆成：

* 字节流解码器
* 状态机解析器
* 命令分发器
* capability / mode 状态管理

### 3. Core Model Layer

职责：保存终端真实状态，是整个库的核心。

这一层应包含：

* 主屏与备用屏
* 基于 cell 的 screen buffer
* scrollback history
* cursor state
* tab stops
* modes and flags
* selection model
* markers / anchors
* search-friendly text projection
* dirty region / incremental update tracking
* resize reflow engine

这里最关键的不是“把字符画出来”，而是正确定义：

* 宽字符与 combining character 如何存储
* 软换行与硬换行如何区分
* 主屏历史如何保留
* resize 时哪些内容应该重排，哪些不应该丢失
* alternate screen 如何与 scrollback 隔离

QTerm 的正确性主要取决于这一层，而不是控件层。

### 4. Frontend Layer

职责：具体 UI 实现。

提供三个渲染方案：
* QTermQuickPaintedItem
* QTermQuickItem
* QTermWidget

## 📚 文档

完整的 QTerm 文档请参考 [docs/](docs/) 目录：

* **[入门指南](docs/getting-started/)** — 快速开始、CMake 集成、第一个终端应用
* **[架构设计](docs/architecture/)** — 五层架构、核心状态机、后端生命周期、前端控制
* **[开发指南](docs/guides/)** — 会话后端、主题系统、滚动历史、选择与剪贴板、键盘编码
* **[内部参考](docs/internals/)** — 测试、协议状态、Bug 分析


如果你也关心 Qt 场景下真正可控的终端组件，这个项目就是为此而做。



