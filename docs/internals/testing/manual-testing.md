# QTerm 手工回归测试指南

本文档覆盖 QTerm 各核心功能的手工验证步骤。每个测试用例给出**前提条件、操作步骤、期望结果**，以及失败时的常见原因。

自动化单元测试参见 [tests/](../../tests/)；tmux 专项测试参见 [manual-testing-tmux.md](manual-testing-tmux.md)。

---

## 准备工作

```bash
cd /path/to/qterm/build
ninja qterm_quick_demo
./examples/quick-demo/qterm_quick_demo
```

macOS 上首次运行可能出现系统权限弹窗，授权后重启即可。

---

## 1. 鼠标跟踪（Mouse Tracking）

### 1.1 最小验证：查看原始上报字节

**目的**：确认 SGR 鼠标模式（`?1006`）下点击事件能通过 `outboundData` 发出并送达 PTY。

**步骤**：

```bash
# 启用 SGR + Button 鼠标模式，然后用 cat -v 观察原始字节
printf '\033[?1006h\033[?1000h'; cat -v
```

在终端内**点击任意位置**，期望看到类似：

```
^[[<0;12;5M   ← 左键按下，列 12，行 5
^[[<0;12;5m   ← 左键释放
```

**退出**：`Ctrl+C`（注意：此时需要鼠标模式已退出，否则 Ctrl+C 本身也需要键盘输入）。

> **失败现象**：无任何输出 → `sendMouse` 未发出 `outboundData`，或 `QTermQuickPaintedItem` 未转发 `mousePressEvent`。

---

### 1.2 Button 模式（拖拽上报）

**步骤**：

```bash
printf '\033[?1006h\033[?1002h'; cat -v
```

1. 按住左键拖拽
2. 期望每移动一格出现一行 `^[[<32;...M`（button=32 表示拖拽）
3. 释放后出现 `^[[<0;...m`

> **失败现象**：拖拽无输出 → `mouseMoveEvent` 未在 Button 模式 + 左键按下时调用 `sendMouse`。

---

### 1.3 AnyEvent 模式（无按键移动）

**步骤**：

```bash
printf '\033[?1006h\033[?1003h'; cat -v
```

1. **不按任何键**，移动鼠标
2. 期望每移动一格出现 `^[[<35;...M`（button=35 = 32+3，无按键移动）

> **失败现象**：移动无输出 → `hoverMoveEvent` 未实现，或 `setAcceptHoverEvents(true)` 未被调用。

---

### 1.4 退出鼠标模式后恢复文本选择

**步骤**：

1. 运行 `printf '\033[?1006h\033[?1000h'; cat` 开启鼠标模式
2. 按 `Ctrl+C` 退出（cat 退出，关闭鼠标模式）
3. 在终端内拖选一段文字
4. 期望：文字高亮，触发 `copyRequested` 信号（在 demo 中表现为系统剪贴板有内容）

> **失败现象**：退出后鼠标点击仍不触发选区 → `modeStateChanged` 未还原 `setAcceptedMouseButtons(Qt::LeftButton)`。

---

### 1.5 htop 鼠标交互

**步骤**：

```bash
htop
```

1. 用鼠标点击进程列表某一行，期望该行高亮
2. 点击顶部排序栏（`PID` / `CPU%` 等），期望列表重新排序
3. 点击底部 `[F10 Quit]`，期望退出 htop

> **htop 使用 `?1002` Button 模式**，失败现象：点击无响应 → 同 1.2。

---

### 1.6 vim 鼠标点击

**步骤**：

```bash
vim
```

进入 vim 后执行：

```
:set mouse=a
```

1. 打开一个多行文件（`:e /etc/hosts`）
2. 点击某一行，期望光标跳到该位置
3. 滚轮上下滚动，期望内容滚动
4. 拖选一段文字（vim visual 模式），期望选区正确

---

## 2. 键盘编码

### 2.1 Ctrl+字母（macOS Cocoa 拦截问题）

**背景**：macOS 的 `NSTextInputClient` 会在系统层面拦截 Ctrl+B/F/P/N/A/E 等，导致 `QKeyEvent::text()` 为空。QTerm 在编码器层通过 Qt key code 合成控制字符。

**步骤**：

```bash
cat -v
```

依次按下以下组合键，期望看到对应控制字符：

| 按键 | 期望输出 |
|------|----------|
| Ctrl+A | `^A` |
| Ctrl+B | `^B` |
| Ctrl+C | `^C` |
| Ctrl+E | `^E` |
| Ctrl+F | `^F` |
| Ctrl+N | `^N` |
| Ctrl+P | `^P` |
| Ctrl+U | `^U` |
| Ctrl+Z | `^Z` |

> **失败现象**：某键无输出或输出错误 → `QTermInputEncoder::encodeKey` 的 fallback 合成逻辑缺少该键。

---

### 2.2 tmux Ctrl+B 前缀键

**步骤**：

```bash
tmux
```

按 `Ctrl+B`，再按 `%`（左右分屏）。

- 期望：窗口一分为二，分割线正确显示（`│`）
- 若仅输出 `^B%` 字符或无反应 → Ctrl+B 未送达 tmux

---

### 2.3 方向键应用模式（DECCKM）

**步骤**：

```bash
vim
```

按上下左右方向键移动光标。

- **vim 启动时开启 `?1h` DECCKM**，期望方向键发出 `\eOA/B/C/D`
- 失败现象：光标不移动，或屏幕输出 `A`/`B` 字母 → 方向键编码未切换到应用模式

---

### 2.4 功能键

**步骤**：

```bash
htop
```

按 F1–F10，期望对应功能（F1=帮助，F10=退出等）正常响应。

---

## 3. tmux 综合测试

详见 [manual-testing-tmux.md](manual-testing-tmux.md)。

核心检查项（快速版）：

```bash
tmux
```

| 操作 | 快捷键 | 期望结果 |
|------|--------|----------|
| 左右分屏 | `Ctrl+B %` | 出现竖向分割线 `│` |
| 上下分屏 | `Ctrl+B "` | 出现横向分割线 `─` |
| 切换窗格 | `Ctrl+B 方向键` | 焦点切换，光标跳转 |
| 新建窗口 | `Ctrl+B c` | 底部状态栏新增标签 |
| 脱离会话 | `Ctrl+B d` | 退回 shell |
| 重连会话 | `tmux attach` | 界面完整恢复 |

在 tmux 内运行常见 TUI：

- `vim` — 界面完整，方向键可用，`:q` 退出正常
- `htop` — 颜色正常，刷新不花屏
- `less /etc/hosts` — 翻页正常，`q` 退出

---

## 4. 颜色与样式

### 4.1 256 色 + TrueColor

```bash
# 打印 256 色色板
for i in $(seq 0 255); do
  printf '\033[38;5;%dm%3d \033[0m' "$i" "$i"
  [ $(( (i+1) % 16 )) -eq 0 ] && echo
done
```

期望：0–15 标准色、16–231 彩色立方、232–255 灰阶梯度均正确显示。

```bash
# TrueColor 渐变测试
awk 'BEGIN{for(r=0;r<256;r+=8) printf "\033[48;2;%d;0;0m \033[0m",r; print""}'
```

期望：从黑到红的平滑渐变（不出现色带跳变）。

---

### 4.2 SGR 属性

```bash
printf '\033[1m粗体\033[0m '
printf '\033[3m斜体\033[0m '
printf '\033[4m下划线\033[0m '
printf '\033[7m反色\033[0m '
printf '\033[9m删除线\033[0m\n'
```

期望：各属性独立正常渲染，`\e[0m` 后完全还原。

---

## 5. 备用屏幕与滚动

### 5.1 备用屏幕切换（vim/htop 场景）

```bash
vim /etc/hosts
```

1. vim 启动：屏幕切换到备用缓冲区（`?1049h`），主缓冲区内容消失
2. `:q` 退出：回到主缓冲区，原始 shell 输出完整恢复

> **失败现象**：退出 vim 后 shell 输出消失或错乱 → `?1049l` 未正确恢复主缓冲区光标位置。

---

### 5.2 滚动缓冲区

```bash
# 产生大量输出，测试向上滚动
seq 1 200
```

用鼠标滚轮或 ScrollBar 向上滚动，期望能看到第 1 行。

---

### 5.3 清除滚动缓冲（ED 3）

```bash
printf '\033[3J'
```

执行后滚动缓冲区应清空，ScrollBar 的 `scrollSize` 回到 1.0（不可再上滚）。

---

## 6. 文字选择与复制

### 6.1 单击选词（双击）

1. 在终端输出一些单词
2. 双击某单词
3. 期望：整个单词高亮选中

### 6.2 拖选复制

1. 按住左键拖过一段文字
2. 释放后期望文字高亮
3. 按 `Cmd+C`（macOS）触发复制
4. 在其他应用粘贴，内容正确

> **注意**：鼠标协议启用时（如在 htop 内），拖选不可用，点击应转发给 PTY。

---

## 7. IME 输入（中文输入法）

**步骤**：

1. 切换到中文输入法（如搜狗/百度/系统自带拼音）
2. 输入拼音 `nihao`，选择"你好"
3. 期望：终端收到 `你好` 两个 Unicode 字符，不出现拼音残留

---

## 8. 窗口调整大小

**步骤**：

1. 启动 demo 并在里面运行 `htop` 或 `vim`
2. 拖拽 demo 窗口边框改变大小
3. 期望：
   - TUI 程序立即重新布局（SIGWINCH 触发 `TIOCSWINSZ`）
   - 不出现内容截断或多余空行

---

## 9. 光标形状（DECSCUSR）

### 9.1 基本形状切换

**步骤**：在 demo 终端内运行：

```bash
# 细竖线（Bar）
printf '\e[6 q'; sleep 1
# 下划线
printf '\e[4 q'; sleep 1
# 方块（默认）
printf '\e[2 q'
```

期望：光标形状随每条命令立即改变。

### 9.2 循环演示

```bash
while true; do
  for s in 2 4 6; do
    printf '\e[%d q' "$s"
    sleep 0.8
  done
done
```

期望：方块 → 下划线 → 竖线每 0.8 秒轮换，`Ctrl+C` 后光标停留在最后一种形状。

### 9.3 参数映射表

| 参数 `n` | 期望形状 |
|----------|----------|
| 0 或 2   | 方块（Block） |
| 3 或 4   | 下划线（Underline） |
| 5 或 6   | 细竖线（Bar） |
| 1        | 方块（Block，闪烁变体） |

```bash
for s in 0 1 2 3 4 5 6; do
  printf "n=%d: " "$s"
  printf '\e[%d q' "$s"
  read -r   # 按 Enter 继续
done
```

### 9.4 vim 光标形状跟随模式

```bash
vim
```

vim 默认在插入模式发送 `\e[5 q`（竖线），普通模式发送 `\e[2 q`（方块）。

1. 启动 vim，处于普通模式 → 期望光标为**方块**
2. 按 `i` 进入插入模式 → 期望光标变为**细竖线**
3. 按 `Esc` 返回普通模式 → 期望光标变回**方块**

> **失败现象**：形状不变 → `QTermTextParser` 的 `CsiIntermediate` 状态未正确识别 `SP q` 序列，或 `QTermQuickPaintedItem::paint` 未读取 `surfaceModel->cursorShape()`。

---

## 常见失败速查

| 现象 | 最可能原因 |
|------|-----------|
| 鼠标点击无任何响应 | `modeStateChanged` 未触发 `updateMouseAcceptance`，按钮未被 accept |
| 鼠标点击有响应但程序无反应 | `sendMouse` 调用了 `feedText` 而非 `outboundData`（旧 bug） |
| AnyEvent 移动无上报 | `hoverMoveEvent` 未实现 / `setAcceptHoverEvents` 未开启 |
| Ctrl+B 不触发 tmux | macOS Cocoa 拦截，`encodeKey` fallback 合成逻辑缺失 |
| 分割线显示为 `x q` 字母 | DEC 线图字符集 `ESC(0` 未映射 |
| vim 启动卡住 | DSR `CSI 6n` 未响应 |
| 退出 vim 后 shell 消失 | `?1049l` 未正确恢复主缓冲区 |
| 颜色全部相同 | SGR 256色 / TrueColor 解析缺失 |
