# QTerm 协议实现状态

本文档按协议维度体系化记录 QTerm 的实现状态，供开发决策与优先级排查使用。

协议参考规范见 [TERMINAL_PROTOCAL.md](TERMINAL_PROTOCAL.md)。

**图例：** ✅ 已实现 · 🔧 部分实现 · ❌ 未实现

---

## 维度一：词法解析（解析器状态机）

> 规范依据：ECMA-48 / Paul Williams VT500 状态机
> QTerm 实现层：`QTermTextParser` / `QTermInputParser`

| 功能 | 状态 | 示例 | 验证方法 | 备注 |
| :--- | :---: | :--- | :--- | :--- |
| Ground 状态：可打印字符直接输出 | ✅ | `printf 'hello'` | 终端正常显示 `hello` | |
| Escape 状态：ESC 开头序列入口 | ✅ | `printf '\e7\e[5;5H*\e8'` | `*` 出现在 (5,5)，`\e8` 后光标回原处 | |
| EscapeIntermediate：字符集切换中间字节 | ✅ | `printf '\e(0aqb\e(B'` | 显示 DEC 线图字符（`▒`、`◆` 等） | ESC ( / ESC ) |
| CSI 状态：参数积累与动作码分发 | ✅ | `printf '\e[2J'` | 清屏成功，光标归 (0,0) | |
| DEC 私有前缀 `?` 和次级前缀 `>` | ✅ | `printf '\e[?25l'` | 光标消失 | |
| OSC 状态：BEL / ST 终结 | ✅ | `printf '\e]2;My Title\a'` | demo 状态栏标题更新为 `My Title` | |
| 跨 write() 调用的状态保持 | ✅ | 见单元测试 `keepsPartialCsiStateAcrossWrites` | 单元测试通过 | 残缺序列跨块可正确续接 |
| DCS 状态 | ❌ | — | — | 目前不需要，暂不实现 |

---

## 维度二：布局控制（光标移动、清屏、滚动区）

> 规范依据：VT100 / ANSI X3.64
> QTerm 实现层：`QTermInputExecutor` + `QTermBuffer`

### C0 控制字符

| 序列 | 状态 | 示例 | 验证方法 | 备注 |
| :--- | :---: | :--- | :--- | :--- |
| NUL (0x00) | ✅ | `printf '\x00'` | 无输出，终端不崩溃，光标不移动 | 忽略 |
| BEL (0x07) | 🔧 | `printf '\a'` | demo 状态栏 Bell 计数+1，屏幕边缘闪烁 | 视觉闪烁已实现；声音未绑定 |
| BS (0x08) | ✅ | `printf 'ab\bc'` | 输出 `ac`（b 被 c 覆盖） | |
| HT (0x09) | ✅ | `printf 'a\tb'` | `b` 对齐到第 9 列 | 每 8 列制表位 |
| LF/VT/FF (0x0A–0x0C) | ✅ | `printf 'a\nb\nc'` | 三行分别显示 a、b、c | 滚动区语义正确 |
| CR (0x0D) | ✅ | `printf 'hello\rworld'` | 显示 `world`（覆盖 hello） | |
| SO/SI (0x0E/0x0F) | ✅ | `printf '\x0e\e(0aqb\x0f'` | 显示 DEC 线图字符后切回 ASCII | 字符集切换 G0/G1 |

### ESC 两字节序列

| 序列 | 状态 | 示例 | 验证方法 | 备注 |
| :--- | :---: | :--- | :--- | :--- |
| ESC 7 / ESC 8（DECSC/DECRC） | ✅ | `printf '\e7\e[10;10H*\e8'` | `*` 出现在 (10,10)，光标恢复到原位 | |
| ESC M（Reverse Index） | ✅ | `printf '\e[1;1H\eM'` | 光标在第一行执行反向索引，顶部插入新行 | tmux 上翻依赖 |
| ESC = / ESC >（应用/普通小键盘） | ✅ | `printf '\e='`，运行 `cat -v` 按数字键 | 输出 `^[Op`（0 键）等应用序列 | |
| ESC c（RIS 全局复位） | ✅ | `printf '\ec'` | 终端完全重置，清屏，属性还原，滚动区清除 | |
| ESC ( B / ESC ( 0（字符集 G0） | ✅ | `printf '\e(0lqqk\nmqqj\e(B'` | 显示 `┌──┐` / `└──┘` 完整框线 | DEC 线图字符集 |
| ESC ) B / ESC ) 0（字符集 G1） | ✅ | `printf '\e)0\x0eaqb\x0f'` | G1 切换后显示 DEC 线图字符，`\x0f` 后恢复 | |

### CSI 光标移动

| 序列 | 状态 | 示例 | 验证方法 | 备注 |
| :--- | :---: | :--- | :--- | :--- |
| CUU/CUD/CUF/CUB (A/B/C/D) | ✅ | `printf '\e[5B\e[3C*'` | `*` 出现在初始光标下 5、右 3 处 | |
| CNL/CPL (E/F) | ✅ | `printf '\e[2E*'` | `*` 出现在初始光标下 2 行行首 | |
| CHA (G) | ✅ | `printf '\e[10G*'` | `*` 出现在第 10 列 | |
| CUP/HVP (H/f) | ✅ | `printf '\e[5;10H*'` | `*` 出现在第 5 行第 10 列 | |
| VPA (d) | ✅ | `printf '\e[5d*'` | `*` 出现在第 5 行（列不变） | |
| ANSI 保存/恢复光标 (s/u) | ✅ | `printf '\e[s\e[5;5H*\e[u'` | `*` 在 (5,5)，光标恢复到保存位置 | |

### CSI 擦除

| 序列 | 状态 | 示例 | 验证方法 | 备注 |
| :--- | :---: | :--- | :--- | :--- |
| ED 0/1/2/3 J | ✅ | `printf '\e[2J'`；`printf '\e[3J'` | 2J 清屏；3J 后 ScrollBar 消失（滚动缓冲清除） | 含清滚动缓冲 3J |
| EL 0/1/2 K | ✅ | `printf 'hello\e[1K'` | 从行首到光标前被清空 | |
| ECH (X) | ✅ | `printf 'hello\e[2G\e[3X'` | 第 2–4 列被清为空格 | |

### CSI 插入/删除/滚动

| 序列 | 状态 | 示例 | 验证方法 | 备注 |
| :--- | :---: | :--- | :--- | :--- |
| ICH (@) / DCH (P) | ✅ | `printf 'ABCDE\e[3G\e[2@'` | 第 3 列前插入 2 个空格，原字符右移 | |
| IL (L) / DL (M) | ✅ | `printf '\e[3L'` | 当前行上方插入 3 行，原内容下移 | |
| SU (S) / SD (T) | ✅ | `printf '\e[5S'` | 可见内容整体上移 5 行 | |

### 滚动区与模式

| 功能 | 状态 | 示例 | 验证方法 | 备注 |
| :--- | :---: | :--- | :--- | :--- |
| DECSTBM (r) 设置滚动区 | ✅ | `printf '\e[5;10r\e[10;1Htest\n'` | 仅第 5–10 行滚动，其他行不受影响 | 光标归位语义已修正为绝对 (0,0) |
| 滚动区外末行 LF 不触发全屏滚动 | ✅ | `printf '\e[5;10r\e[4;1Hline\n'` | 滚动区外的行按 LF 只移动光标 | 已修正边界行为 |
| DECAWM (?7) 自动换行 | ✅ | `printf '\e[?7l'; printf '%0.s-' {1..100}` | 关闭后超出末列覆盖最后一格，不换行 | |
| DECTCEM (?25) 光标显示/隐藏 | ✅ | `printf '\e[?25l'; sleep 1; printf '\e[?25h'` | 光标消失 1 秒后恢复 | |
| 备用屏幕 ?47/?1047/?1049 | ✅ | `printf '\e[?1049h'; sleep 1; printf '\e[?1049l'` | 切换到空白备用屏后还原，主屏内容完整 | 完整保存/恢复光标语义 |

---

## 维度三：视觉样式（SGR 与渲染属性）

> 规范依据：xterm SGR / ISO 8613-6 TrueColor
> QTerm 实现层：`QTermCellAttributes` + 渲染器

### 文字属性

| 属性 | SGR 参数 | 状态 | 示例 | 验证方法 | 备注 |
| :--- | :---: | :---: | :--- | :--- | :--- |
| 重置 | 0 | ✅ | `printf '\e[1;31mRed Bold\e[0m Normal'` | `Normal` 以默认样式显示 | |
| 粗体 Bold | 1 | ✅ | `printf '\e[1mBold\e[0m'` | `Bold` 以粗体显示 | |
| 暗淡 Dim | 2 | ✅ | `printf '\e[2mDim\e[0m'` | `Dim` 亮度降低 | |
| 斜体 Italic | 3 | ✅ | `printf '\e[3mItalic\e[0m'` | `Italic` 以斜体显示 | |
| 下划线 Underline | 4 | ✅ | `printf '\e[4mUnder\e[0m'` | `Under` 文字下方有线 | |
| 闪烁 Blink | 5 | 🔧 | `printf '\e[5mBlink\e[0m'` | 文字正常渲染，无闪烁动画 | 存储已支持，渲染动画未实现 |
| 反色 Reverse | 7 | ✅ | `printf '\e[7mReverse\e[0m'` | 前景背景互换 | |
| 隐藏 Invisible | 8 | ✅ | `printf '\e[8mHidden\e[0m'` | 文字不可见（与背景色相同） | |
| 删除线 Strikethrough | 9 | ✅ | `printf '\e[9mStrike\e[0m'` | `Strike` 文字中间有线 | |
| 关闭粗体/暗淡 | 22 | ✅ | `printf '\e[1mB\e[22mN\e[0m'` | `N` 粗体关闭后恢复正常 | |
| 关闭下划线 | 24 | ✅ | `printf '\e[4mU\e[24mN\e[0m'` | `N` 下划线关闭 | |
| 关闭反色 | 27 | ✅ | `printf '\e[7mR\e[27mN\e[0m'` | `N` 反色关闭 | |

### 颜色

| 功能 | 状态 | 示例 | 验证方法 | 备注 |
| :--- | :---: | :--- | :--- | :--- |
| 标准 16 色前景 (30–37 / 90–97) | ✅ | `printf '\e[31mRed\e[0m'` | 文字显示为红色 | |
| 标准 16 色背景 (40–47 / 100–107) | ✅ | `printf '\e[41mRedBG\e[0m'` | 背景显示为红色 | |
| 默认前景/背景 (39/49) | ✅ | `printf '\e[31mR\e[39mD\e[0m'` | `D` 颜色恢复为默认前景色 | |
| 256 色前景 (38;5;n) | ✅ | `printf '\e[38;5;196mRed256\e[0m'` | 显示 256 色中的亮红色 | |
| 256 色背景 (48;5;n) | ✅ | `printf '\e[48;5;196mRedBG\e[0m'` | 256 色红色背景 | |
| TrueColor 前景 (38;2;r;g;b) | ✅ | `printf '\e[38;2;255;128;0mOrange\e[0m'` | 精确橙色文字 | |
| TrueColor 背景 (48;2;r;g;b) | ✅ | `printf '\e[48;2;255;128;0mOrangeBG\e[0m'` | 精确橙色背景 | |

### 宽字符与组合字符

| 功能 | 状态 | 示例 | 验证方法 | 备注 |
| :--- | :---: | :--- | :--- | :--- |
| CJK 全角字符（2 列宽） | ✅ | `printf '中文'` | 每个汉字占 2 列，光标前进 2 格 | 含续占位 cell |
| 组合字符 Combining Marks | ✅ | `printf 'e\u0301 o\u0308'` | 显示 `é ö`，光标不额外前进 | 附加到前一 cell，不移光标 |
| 非 BMP 字符（surrogate pair）| ✅ | `printf '\U0001F600'` | 显示 😀 emoji，占 2 列 | 跨 write() 保持代理对状态 |
| DEC 线图字符集（框线绘制） | ✅ | `printf '\e(0lqqk\nmqqj\e(B'` | 显示 `┌──┐` / `└──┘` 完整框线 | tmux/vim 框线 |

---

## 维度四：输入反馈（键盘/鼠标编码）

> 规范依据：X11 xterm 鼠标协议、VT220 键盘编码
> QTerm 实现层：`QTermInputEncoder` + Qt 事件桥接

### 键盘编码

| 功能 | 状态 | 示例 | 验证方法 | 备注 |
| :--- | :---: | :--- | :--- | :--- |
| 普通光标键 (ESC [ A/B/C/D) | ✅ | 运行 `cat -v`，按方向键 | 输出 `^[[A` / `^[[B` / `^[[C` / `^[[D` | |
| 应用光标键 (ESC O A/B/C/D，DECCKM ?1) | ✅ | 运行 `vim`，按方向键 | vim 内光标正常移动；`cat -v` 下输出 `^[OA` 等 | |
| Home / End | ✅ | 运行 `cat -v`，按 Home / End | 输出 `^[[H` / `^[[F` | |
| F1–F4 (ESC O P/Q/R/S) | ✅ | 运行 `cat -v`，按 F1–F4 | 输出 `^[OP`–`^[OS` | |
| F5–F12 (ESC [ 15~ 等) | ✅ | 运行 `cat -v`，按 F5–F12 | 输出 `^[[15~`–`^[[24~` | |
| Insert / Delete / PgUp / PgDn | ✅ | 运行 `cat -v`，按相应键 | 输出 `^[[2~`、`^[[3~`、`^[[5~`、`^[[6~` | |
| Shift/Alt/Ctrl 修饰键组合 | ✅ | 运行 `cat -v`，按 Shift+Up | 输出 `^[[1;2A`（modifier=2） | CSI 1;mod 格式 |
| Ctrl+字母（macOS Cocoa 拦截修正） | ✅ | 运行 `cat -v`，按 Ctrl+B/F/P/N | 输出 `^B`/`^F`/`^P`/`^N` | 在 encoder 层修正 text 为空的问题 |
| 应用小键盘模式 (DECKPAM/DECKPNM) | ✅ | `printf '\e='`；运行 `cat -v` 按数字键 | 输出 `^[Op`（0 键）等应用小键盘序列 | |

### 鼠标追踪

| 功能 | 状态 | 示例 | 验证方法 | 备注 |
| :--- | :---: | :--- | :--- | :--- |
| X10 按下/释放 (?1000) | ✅ | `printf '\e[?1006h\e[?1000h'; cat -v` | 点击后输出 `^[[<0;x;yM` / `^[[<0;x;ym` | |
| Button 事件 + 拖拽 (?1002) | ✅ | `printf '\e[?1006h\e[?1002h'; cat -v` | 按住拖拽时持续输出 `^[[<32;x;yM` | 拖拽 +32 偏移已修正 |
| AnyEvent 全移动 (?1003) | ✅ | `printf '\e[?1006h\e[?1003h'; cat -v` | 鼠标任意移动即输出 `^[[<35;x;yM` | |
| SGR 鼠标编码 (?1006) | ✅ | `printf '\e[?1006h\e[?1000h'; cat -v` | 输出含 `<` 前缀，释放用小写 `m` 结尾 | `<` 前缀正确，释放用 `m` |
| 动态更新 acceptedMouseButtons / hoverEvents | ✅ | `printf '\e[?1000l'` 退出鼠标模式 | 退出后可正常拖选文本 | QTermQuickPaintedItem 实现 |
| outboundData 路由到 PTY | ✅ | 在 `htop` 中点击进程行 | 进程行高亮，htop 正常响应点击 | 已修正 |

### 设备查询响应

| 序列 | 状态 | 示例 | 验证方法 | 备注 |
| :--- | :---: | :--- | :--- | :--- |
| DSR / CPR (ESC[6n → ESC[r;cR) | ✅ | `printf '\e[6n'; cat -v` | 输出 `^[[row;colR`，行列与当前光标一致 | |
| DA1 (ESC[c → ESC[?1;0c) | ✅ | `printf '\e[c'; cat -v` | 输出 `^[[?1;0c` | |
| DA2 (ESC[>c → ESC[>0;0;0c) | ✅ | `printf '\e[>c'; cat -v` | 输出 `^[[>0;0;0c` | |

---

## 维度五：系统交互（OSC / 窗口属性绑定）

> 规范依据：xterm OSC 规范
> QTerm 实现层：`QTermInputExecutor` → Qt 属性/信号

| OSC | 功能 | 状态 | 示例 | 验证方法 | 备注 |
| :---: | :--- | :---: | :--- | :--- | :--- |
| 0 / 1 / 2 | 窗口标题 / 图标名 | ✅ | `printf '\e]2;My Title\a'` | demo 状态栏标题更新为 `My Title` | 信号已暴露至前端 |
| 4 | 设置调色板颜色 | ❌ | — | — | 暂不需要 |
| 7 | 当前工作目录 (CWD) | ✅ | `printf '\e]7;file:///tmp\a'` | `terminal.currentDirectory` 属性更新为 `file:///tmp` | |
| 8 | 超链接（OSC 8 hyperlink） | ✅ | `printf '\e]8;;https://qt.io\aQtLink\e]8;;\a'` | `QtLink` 显示为可点击链接，点击触发 `hyperlinkActivated` | 解析、存储、渲染、点击均已实现 |
| 10 / 11 | 查询/设置前景/背景色 | ❌ | — | — | 可按需实现 |
| 52 | 系统剪贴板写入 | 🔧 | `printf '\e]52;c;%s\a' "$(printf 'Hello' \| base64)"` | `clipboardWriteRequested("Hello")` 信号触发 | 读请求忽略；读功能需安全评估后实现 |
| 133 | Shell 集成（提示符标记） | ✅ | `printf '\e]133;A\a'` | `terminal.shellZone` 变为 1；`D;127` 后 `lastExitCode` 变为 127 | A=prompt B=cmd C=output D;n=end |
| — | 括号粘贴 (?2004) | ✅ | `printf '\e[?2004h'`，再粘贴文本 | 粘贴内容被 `ESC[200~...ESC[201~` 包裹 | |
| — | 同步输出 (?2026) | ✅ | `printf '\e[?2026h'` | 忽略，无崩溃，渲染无变化 | 忽略（不渲染抖动） |

---

## 当前缺口摘要

| 维度 | 缺口 | 优先级 |
| :--- | :--- | :---: |
| 视觉样式 | Blink 属性渲染动画 | P3 |
| 系统交互 | BEL 声音 Qt 绑定（视觉闪烁已实现） | P3 |
| 系统交互 | OSC 52 剪贴板读功能（安全评估） | P3 |
| 解析器 | DCS 状态（目前不需要） | — |
