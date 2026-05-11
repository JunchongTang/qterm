# 表面层：Buffer、滚动区域与 Resize

本文介绍 QTerm 存储终端内容的数据结构与不变式，`scrollBottom` 如何控制滚动行为，
以及终端 resize 时各步骤的完整时序。

---

## Buffer 模型

`QTermBuffer` 将终端内容分为两个独立区域：

```
┌───────────────────────────────────────────┐
│  m_historyLines[]                         │  ← 已滚动到视口上方的逻辑行
│  （随内容滚出视口而向下增长）              │
├───────────────────────────────────────────┤
│  m_visibleLines[]                         │  ← 恰好 `rows` 行物理行，
│  第 0 行                                  │    始终是当前视口
│  第 1 行                                  │
│  …                                        │
│  第 rows-1 行                             │
└───────────────────────────────────────────┘
```

每个 `QTermLine` 存储一个 `QTermCell` 向量和一个 `wrappedToNextLine` 标志。
当光标写过第 `columns-1` 列且自动换行开启时，当前行被标记为 `wrappedToNextLine = true`，
光标移到下一行第 0 列。该标志将相邻的物理行链接成一条**逻辑行**——一个在 reflow 后仍然有效的内容单元。

### 逻辑行 vs. 物理行

*逻辑行*是最长的物理行序列，其中除最后一行外每行都有 `wrappedToNextLine = true`。
其文本宽度为 `(链长 - 1) × columns + 最后一行长度`。

在 resize 到新宽度时，`QTermBuffer::resize()` 通过重排逻辑行来重建可见行：
它同时遍历 `m_historyLines` 和 `m_visibleLines`，按新列宽重新折行，
并将光标定位到 resize 前所在的字符格。

---

## 滚动区域：`scrollTop` 与 `scrollBottom`

`QTermScreenState` 与 Buffer 一同持有 `scrollTop` 和 `scrollBottom`。
它们实现了 DECSTBM 转义序列（`CSI Pt ; Pb r`），将滚动限制在屏幕的一个子区域内。

### 两种语义截然不同的状态

| 条件 | 含义 |
|------|------|
| `scrollTop == 0 && scrollBottom == rows - 1` | **全屏区域** — 终端滚动整个视口；内容进入 `m_historyLines` 历史。 |
| 其他任意值 | **自定义滚动区域** — 只有 `scrollTop…scrollBottom` 之间的行滚动；上下区域固定；不产生历史。 |

### `advanceToNextRow()` 逻辑

当光标位于滚动区域最后一行并收到换行符时：

```
if scrollTop == 0 && scrollBottom == buffer.rows() - 1:
    buffer.scrollUp()          // 将第 0 行移入历史，其余行上移
else:
    buffer.deleteLines(scrollTop, 1, scrollTop, scrollBottom)
    // 删除区域最上面一行，其余上移，底部追加空行
```

这个区别至关重要：`buffer.scrollUp()` 是积累历史的路径；`deleteLines` 是纯视口操作。

---

## `scrollBottom` Resize Bug 及修复

### 问题所在

`QTermScreenState` 默认初始化 `scrollBottom = 23`（24 行终端的 rows - 1）。

当终端窗口扩大——比如从 24 行增长到 44 行——朴素的做法是用 `qBound(0, scrollBottom, rows-1)`：

```
qBound(0, 23, 43) → 23    // 没有变化！
```

`scrollBottom` 停在 23。此后 `advanceToNextRow()` 在第 23 行触发，但
`23 ≠ buffer.rows()-1 (43)`，于是走**自定义区域分支**，调用 `deleteLines(0, 1, 0, 23)`。
内容只在原来的 24 行区域内滚动，下方 20 行永远是空白。

手动把窗口缩小再放大，只有在尺寸恰好与过期的 `scrollBottom` 值吻合时才能偶然触发正确路径。

### 修复方案

`QTermScreenState::resize()` 现在在 resize 前检测滚动区域是否覆盖了全屏，
如果是，则 resize 后无条件重置为全屏：

```cpp
void resize(int columns, int rows)
{
    const int oldRows = buffer.rows();
    const bool fullScreenRegion = (scrollTop == 0 && scrollBottom == oldRows - 1);
    cursorState = buffer.resize(columns, rows, cursorState);
    cursorOnWrapTarget = false;

    if (fullScreenRegion) {
        scrollTop = 0;
        scrollBottom = rows - 1;      // 始终跟踪新高度
    } else {
        scrollTop  = qBound(0, scrollTop,  rows - 1);
        scrollBottom = qBound(0, scrollBottom, rows - 1);
        if (scrollBottom < scrollTop) {
            scrollTop = 0;
            scrollBottom = rows - 1;  // 退化区域 → 重置为全屏
        }
    }
}
```

规则：resize 前是全屏区域，resize 后仍是全屏区域。自定义子区域则裁剪到新尺寸范围内。

---

## Resize 完整时序

一次完整的 resize 需要跨四个层协同完成，各步骤必须按顺序执行：

```
1. 前端几何变化
   └── QTermViewController::geometryChange()
         └── 启动 140 ms 防抖计时器

2. 计时器触发
   └── QTermViewController::syncTerminalSize()
         └── 计算 (newCols, newRows) = (像素宽 / cellWidth, 像素高 / cellHeight)
               └── QTermTerminal::setTerminalSize(cols, rows)

3. QTermTerminal::setTerminalSize
   ├── QTermCore::setTerminalSize(cols, rows)
   │     └── QTermScreenState::resize(cols, rows)     ← 历史重排 + scrollBottom 修复
   │           └── 发射 QTermCore::sizeChanged()
   │                 └── QTermTerminal::viewportChanged()
   └── QTermSession::resize(cols, rows)
         └── Backend::resize(cols, rows)
               └── ::ioctl(masterFd, TIOCSWINSZ, …)   ← 通知 shell 进程

4. Shell 响应 SIGWINCH
   └── 重绘 UI（bash 提示符重排、vim 重绘等）
   └── 输出字节经由 dataReceived → 解析 → buffer 更新
         └── QTermTerminal::viewportChanged() → 前端重绘
```

### 为什么要防抖？

在 macOS 上拖动窗口角点，每秒会产生数十个 `geometryChanged` 事件。
对每个中间像素尺寸都发送 `TIOCSWINSZ` 会用 `SIGWINCH` 信号淹没 shell，导致多余的重绘和潜在的竞争。
140 ms 防抖确保只有最终稳定的尺寸被转发给 shell。

---

## 脏行追踪（Dirty Row Tracking）

`QTermBuffer` 追踪自上次绘制以来哪些可见行发生了变化：

- `markDirtyRow(i)` — 任何触碰第 `i` 行的操作都会调用此函数。
- `markAllRowsDirty()` — 滚动、resize 或全量清除时调用。
- `clearDirtyRows()` — 渲染器消费完脏行集合后调用。

`QTermQuickItem`（场景图渲染器）利用 `dirtyRows()` 只更新场景图中被修改的节点。
`QTermQuickPaintedItem` 则以 `allRowsDirty()` 作为快速路径，决定发送全量 `update()` 还是针对性的 `update(rect)`。

---

## `QTermSurfaceModel` 作为只读快照

`QTermSurfaceModel` 通过 `viewportLineRuns()` 向 QML 渲染层暴露终端状态——
每行一组*样式段*（style runs）。每个样式段将一段文本与其 SGR 属性（前景色/背景色、粗体、斜体、下划线、超链接 id 等）打包在一起，供渲染器在一次绘制调用中完成渲染。

`QTermSurfaceModel` 是 `QTermBuffer` 的 `friend class`，可以只读访问
`m_historyLines` 和 `m_visibleLines`，但不暴露任何修改接口。渲染器从不回写 Core。
