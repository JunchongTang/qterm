# QTerm 路线图

本路线图基于 DESIGN.md 整理，重点关注执行顺序与当前进度。

状态说明：

* 已完成：该阶段的核心目标已经落地，且已有对应验证。
* 进行中：该阶段已有实质实现，但仍有关键能力或收尾工作未完成。
* 未开始：尚未进入该阶段的实质实现。

## Phase 0：仓库初始化

状态：已完成

目标：

* 建立根 CMake 工程期望的源码目录结构。
* 建立公开头文件目录布局。
* 增加最小 qterm 库目标。
* 增加可实例化库壳层的最小 quick demo 目标。

已完成：

* 根工程可配置、可构建。
* qterm 静态库目标已建立。
* quick demo 目标已建立并可运行。
* core、session、quick 等代码已有稳定归档位置。

退出标准：

* CMake 配置成功。
* demo 目标可以构建。
* 仓库中已经有 core、session、quick 代码的稳定位置。

## Phase 1：无头核心骨架

状态：已完成

目标：

* 定义 QTermCell、样式属性、光标状态和模式状态。
* 定义主屏、备用屏和历史缓冲抽象。
* 为测试实现 debug plain-text 投影。

已完成：

* 无头 core 已可独立创建和测试。
* 行列、光标、主屏/备用屏、基础历史模型均已存在。
* debugPlainText 已成为核心测试投影路径。

退出标准：

* 无头测试可以创建 terminal core。
* 行、列、光标移动以及纯文本投影可以在无 UI 情况下工作。

## Phase 2：Parser MVP

状态：已完成

目标：

* 实现字节解码、解析器骨架和输入执行器。
* 支持可打印文本、CR、LF、BS、HT、基础 SGR、擦除和光标移动。
* 增加保持解析顺序的写入路径。

已完成：

* UTF-8 解码与解析执行主链路已建立。
* 基础 VT 行为已有 headless 测试覆盖。
* 已超出 MVP：当前还支持插删字符、插删行、保存恢复光标、更多 SGR、窗口标题、alternate screen、bracketed paste、application cursor keys 等。

退出标准：

* 无头测试验证基础 VT 行为。
* 核心能处理简单 shell prompt 的真实输出。

## Phase 3：Resize 与 Reflow 正确性

状态：已完成

目标：

* 实现基于逻辑行的重排。
* 在 resize 过程中保留历史与可见屏幕语义。
* 增加反复窄化/放宽循环的回归测试。

已完成：

* 第一版 Qt 原生逻辑行 reflow 已接入 resize 主链。
* 已有 ASCII、CJK、宽字符、wrap 边界、整屏滚动进入 history 的回归测试。
* resize 后 selection 可按逻辑锚点重映射，且 offscreen selection 能保留 selectedText。
* 已支持基于 scrollback viewport 的历史浏览与历史区选择。
* combining text（如 e + U+0301）单轮 / 多轮 narrow/widen 循环测试全部通过。
* combining text 进入 scrollback history 后的 reflow 正确性已验证。
* 多条独立逻辑行在 history 中的 reflow 已验证。
* 光标逻辑行在 scrollback reflow 后保持正确映射。

退出标准：

* ASCII、CJK、combining text 的 reflow golden tests 全部通过。✅（85/85 通过）
* 不再通过 parser replay 做有损重建。✅（已使用逻辑行 reflow）

## Phase 4：Qt Quick 第一前端

状态：已完成

目标：

* 增加 QTermSurfaceModel。
* 增加 QTermQuickItem（QSG 场景图增量渲染）与增量渲染路径。
* 支持选区可视化、光标渲染和基础剪贴板能力。

已完成：

* QTermSurfaceModel 已建立，并已收敛为展示层。
* **QTermQuickItem（QSG）已实现**：场景图节点树（bgFillNode、selectionNode、textGroupNode、cursorNode）、脏行增量更新、`visibleLineRunsChangedPartial` 局部重绘路径。
* quick demo 已具备可见行投影、样式 runs、光标、复制、双击选词、三击选逻辑行、offscreen selection 状态显示、scrollback viewport 浏览与历史选择。
* **OSC 8 超链接**可渲染（下划线着色）与点击交互。

退出标准：

* Qt Quick demo 能显示实时终端内容。✅
* 脏行更新可触发局部重绘。✅

## Phase 5：本地 PTY 集成

状态：基本完成

目标：

* 实现 Unix PTY backend。
* 打通终端 resize 传播。
* 验证交互式 shell 工作流。

已完成：

* Unix PTY backend 已实现并有测试覆盖。
* demo 已可运行本地 shell。
* terminal resize 已传播到 session/backend。
* 当前 scrollback、prompt 与基础交互已有可工作路径。

仍需观察：

* 复杂真实 shell 工作负载下的稳定性仍需要继续打磨。

退出标准：

* demo 能通过 PTY 运行本地 shell。
* resize、scrollback 和 prompt 交互稳定。

## Phase 6：协议扩展

状态：已完成

目标：

* 覆盖 macOS Terminal.app 所需的全量协议能力，包括 DEC 私有模式、鼠标追踪、键盘编码、OSC 序列和超链接。

协议实现的完整维度分析与当前状态见 [docs/internals/protocol-status.md](docs/internals/protocol-status.md)。

已完成（核心里程碑）：

* 解析器与执行器已覆盖全部 P0/P1 级协议。
* **vim、less、htop、tmux（含分屏、框线、滚动）经过人工验证，行为正确。**
* **鼠标追踪已完整实现**：X10 / Button / AnyEvent 事件类型，SGR 编码格式，正确路由到 PTY。
* **OSC 8 超链接**可渲染与点击交互。

退出标准：

* vim、less、htop 和交互式 shell 等常见 CLI 工具行为基本可接受。✅
* mouse tracking 在 tmux / htop 中可用。✅
* OSC 8 超链接可渲染与交互。✅

## Phase 7：更多会话后端

状态：已完成

目标：

* 增加串口 backend。
* 增加 Telnet backend。
* 所有传输方式继续统一在同一个 session abstraction 之后。

已完成：

* **QTermSerialBackend**：基于 QtSerialPort，支持波特率、数据位、校验位、停止位、流控配置；`QTermSerialPortScanner` 提供可用串口枚举，已注册为 QML singleton。
* **QTermTelnetBackend**：基于 QTcpSocket，支持 Telnet 协商（IAC 序列过滤），已可连接远程主机。
* multi-tab QML demo 支持 PTY / 串口 / Telnet 三种会话类型的新建对话框。

退出标准：

* 同一个 QTerm core 与前端可同时驱动 PTY、串口与 Telnet 会话。✅

## Phase 8：API 稳定化与适配层

状态：已完成

目标：

* 打磨公开 API。
* 在同一 surface model 之上增加 QWidget adapter。
* 补充稳定扩展点文档。

已完成：

* `QTermSurfaceModel` 所有内部写方法移入 `private`，`QTermTerminal` 为 `friend class`，外部不可直接调用。
* `QTermTerminal` 移除 `modeState()` 公开方法，替换为语义化查询：`isMouseProtocolActive()`、`isHoverTrackingActive()`、`isButtonTrackingActive()`。
* `QTermModeState.h` 不再被 `QTermTerminal.h` 包含，切断外部头文件对 VT 内部 struct 的依赖。
* `setCurrentDirectory` 改为 `private slot`；`setTitle` 保留公开。
* **`QTermWidget`（QWidget 适配层）已完整实现**：共享 `QTermViewController`，支持全部输入事件、IME、光标渲染、调色板、滚动 API、主题、`sizeHint`；通过 `QPainter` + `qtermPaintTerminal` 绘制，与 QSG 渲染器无关。
* **widget-demo** 已可运行并验证 QWidget 路径。

退出标准：

* 公开头文件已经过兼容性审视。✅
* QWidget 只是适配层，而不是第二套核心实现。✅

## 跨阶段测试矩阵

每个阶段都应继续扩展以下测试面：

* parser 单元测试
* buffer 与 history 测试
* reflow golden tests
* session 集成测试
* Qt Quick 冒烟测试

当前已具备：

* core headless 测试
* session 集成测试
* local PTY backend 测试
* quick demo 构建验证

仍需加强：

* 更系统的 reflow golden tests
* 更强的 scrollback / selection / prompt 组合回归
* 真正前端层的 Qt Quick smoke tests

## 当前下一步

1. **SSH backend**
   基于 libssh2 或平台 `ssh` 命令行实现 SSH 会话 backend，加入 multi-tab demo 的新建对话框。

2. **测试矩阵补全**
   重点补充 reflow golden tests（combining text 多轮循环、复杂 prompt 拼接、scrollback 与可见屏拼接边界）以及前端 Qt Quick smoke tests。

3. **稳定性打磨**
   复杂真实 shell 工作负载（大量输出、快速 resize、tmux 嵌套）下的边界问题持续修复。