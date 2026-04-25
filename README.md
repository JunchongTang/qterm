# QTerm

QTerm 是一个面向 Qt 生态的终端模拟器组件库，目标是替代 qtermwidget 在现代 Qt Quick 场景中的角色，提供一个适合 QML、可扩展、可复用、协议可控的终端内核与控件体系。

项目的核心判断很明确：

1. qtermwidget 的协议许可不适合作为本项目的长期基础。
2. 以 QWidget 为中心的设计不适合 Qt Quick / QML 的渲染模型。
3. 依赖 libvterm 一类外部终端内核虽然起步快，但在 resize reflow、主屏历史恢复、软换行重排等关键行为上，业界已有明显长期问题。
4. 因此 QTerm 选择参考 xterm.js 的架构思想，但基于 Qt 自主实现完整终端能力，而不是封装已有终端库。

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

## 为什么参考 xterm.js

xterm.js 不是因为运行环境而值得参考，而是因为它的架构边界划分是成熟的：

* 传输层与终端状态解耦。
* 协议解析与渲染解耦。
* 核心 Buffer 模型独立于具体前端。
* 输入、渲染、装饰、搜索、选择、链接检测等能力都建立在稳定的核心状态之上。

QTerm 会借鉴这种思路，但实现方式会面向 Qt 的对象模型、事件系统和渲染管线重新设计。

## 设计原则

### 1. Qt-only 的终端内核

QTerm 不以 libvterm 作为底层核心，不把关键语义委托给外部终端库。终端状态、scrollback、reflow、dirty tracking、输入协议、选择模型都由 QTerm 自己掌控。

### 2. 先做内核，再做控件

终端控件只是核心能力的一个前端。真正的产品边界是：

* 会话层
* 协议层
* 状态模型层
* 渲染与交互层

只有把这些层分开，后续才能稳定支持 QML、QWidget、测试工具和无界面场景。

### 3. 逻辑行优先，而不是物理行拼接

很多 resize bug 的根因都来自把“当前屏幕的物理行”和“历史滚动区的物理行”混在一起处理。QTerm 会优先建立逻辑行与 cell 级语义模型，再基于视口宽度进行重排，而不是依赖外部库在不同宽度间反复重建物理布局。

### 4. 渲染层只消费状态，不拥有状态

QML/Qt Quick 前端只负责把 terminal surface 渲染出来，并处理输入事件、选区可视化、光标、IME、鼠标协议等交互。终端真实状态应由核心模型维护，避免 UI 与协议逻辑耦合。

### 5. 后端传输可替换

无论是本地 PTY、Windows ConPTY、串口，还是 SSH 字节流，本质上都应该表现为一个可读写、可 resize、可通知状态变化的 session backend。核心终端逻辑不应感知具体传输方式。

## 分层架构

QTerm 计划采用如下分层：

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

### 4. Presentation Layer

职责：把核心模型投影成便于 UI 使用的表面数据。

这一层建议提供：

* viewport lines projection
* style runs
* cursor snapshot
* selection ranges
* search highlights
* dirty line notifications

这一层的意义是让前端无需直接依赖复杂内核结构，同时为不同 UI 技术栈提供统一消费接口。

### 5. Frontend Layer

职责：具体 UI 实现。

当前主路径：

* QTermQuickPaintedItem

后续可扩展：

* QWidget adapter
* Designer-friendly wrapper
* 测试或无头渲染辅助工具

Qt Quick 前端的重点包括：

* 基于 QSGNode 的增量渲染
* 字形缓存与批量绘制
* 高亮、光标、选区覆盖层
* 鼠标选择与拖拽滚动
* IME、快捷键、粘贴、组合键输入
* 高 DPI 与字体 fallback 支持

## 推荐的数据流

一个输入输出闭环大致如下：

1. Session backend 从 PTY / 串口 / SSH 收到原始字节流。
2. Protocol parser 将字节流解析为终端命令和文本单元。
3. Core model 更新 buffer、cursor、history、modes 和 dirty 状态。
4. Presentation layer 生成受影响行的增量投影。
5. QTermQuickPaintedItem 只重绘脏区。
6. 用户输入经 key mapping / mouse encoding 后重新发送给 session backend。

这个闭环里，任何一层都应避免跨层偷改状态。

## 与现有方案的取舍

### 相比 qtermwidget

QTerm 的目标不是做一个“QWidget 版终端控件的 QML 包装”，而是从一开始就把 QML 作为第一前端，同时保留共享核心给 QWidget 的可能性。

### 相比 libvterm 路线

QTerm 不把重排、历史恢复、软换行语义这些关键行为外包给第三方库，原因很直接：这些行为一旦出错，问题通常不是渲染 bug，而是核心状态模型失真。一个面向长期演进的终端库，需要在最底层掌握这些规则。

### 相比“先做控件，再补内核”

这条路通常会很快进入不可维护状态。QTerm 会优先建立稳定的数据模型与协议边界，再推进控件表现。

## 能力路线图

### Phase 1: 可验证内核

* 建立基础 cell buffer 与 cursor model
* 建立最小 VT parser
* 支持普通文本、换行、回车、退格、基本 SGR
* 支持主屏 resize 与基础 reflow
* 提供可测试的无 UI surface projection

### Phase 2: Qt Quick 可用控件

* QTermQuickPaintedItem 基础渲染
* 文本选择、复制粘贴、光标显示
* 滚动历史浏览
* 快捷键与基础鼠标协议

### Phase 3: 实用会话后端

* Unix PTY
* Windows ConPTY
* 串口 backend
* SSH backend

### Phase 4: 高级终端行为

* Alternate screen
* Bracketed paste
* 鼠标跟踪协议
* OSC 8 hyperlink
* 搜索、高亮、标记点、可编程扩展

### Phase 5: 工程化与生态集成

* 稳定公共 API
* QWidget adapter
* 自动化回归测试矩阵
* 性能 profiling 与大输出场景优化

## 当前范围与非目标

当前阶段更关注“终端内核可验证”而不是一次性覆盖所有终端特性。以下内容不作为第一阶段重点：

* 复杂插件系统
* 主题市场或装饰系统
* 与 shell 深度耦合的 IDE 特性
* 非终端核心相关的通用运行时框架

## 工程建议

如果把这个项目做稳，建议遵循下面几条：

* 先定义 cell、line、logical line、history entry 的数据结构，再写 parser。
* 把 resize reflow 当成核心设计题，而不是后期修补项。
* 从一开始就准备 golden tests，覆盖宽字符、combining、alternate screen、长行重排、scrollback 与 selection。
* parser、model、projection、render 必须分别可测。
* 先做“正确”，再做“快”；但 dirty tracking 和增量更新的数据结构要尽早预留。

## 项目状态

QTerm 当前仍处于架构设计与基础实现阶段。现阶段最重要的目标不是尽快拼出一个可见控件，而是先建立一套长期可维护、可测试、可扩展的终端内核边界。

如果你也关心 Qt 场景下真正可控的终端组件，这个项目就是为此而做。



