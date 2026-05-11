# 指南：键盘输入与按键编码

本文讨论 QTerm 如何将用户按键转化为终端协议字节序列，以及不同平台的特殊处理。

---

## QTermInputEncoder：核心转换器

```cpp
class QTermInputEncoder {
public:
    QTermInputEncoder(const QTermModeState *modeState);
    
    // 将 Qt 按键事件转化为终端字节序列
    QByteArray encodeKey(const QKeyEvent *event);
    
private:
    const QTermModeState *m_modeState;  // 用于查询应用光标模式等
};
```

### 转换流程

```
QKeyEvent {
    key = Qt::Key_Up,
    text = "",
    modifiers = Qt::NoModifier
}
  ↓ QTermInputEncoder::encodeKey()
  ↓ 1. 检查特殊键（方向键、功能键等）
  ↓ 2. 检查 Ctrl/Alt/Shift 修饰符
  ↓ 3. 查询 m_modeState：应用光标模式开启？
  ↓ 4. 构造终端字节序列
  ↓
QByteArray {
    data = "\x1b[A"  (或 "\x1bOA"，取决于模式)
}
```

---

## 特殊键编码

### 方向键：应用光标模式（DECCKM）

当应用请求应用光标模式时（`CSI ? 1 h`），方向键应发送不同的序列：

| 键 | 正常模式 | 应用光标模式 | 说明 |
|-----|---------|---------|------|
| ↑ | `CSI A` | `SS3 A` | Up |
| ↓ | `CSI B` | `SS3 B` | Down |
| → | `CSI C` | `SS3 C` | Right |
| ← | `CSI D` | `SS3 D` | Left |

其中 `SS3` = `ESC O`（`0x1B 0x4F`）。

```cpp
QByteArray QTermInputEncoder::encodeArrowKey(Qt::Key key) {
    char finalByte;
    switch (key) {
    case Qt::Key_Up:    finalByte = 'A'; break;
    case Qt::Key_Down:  finalByte = 'B'; break;
    case Qt::Key_Right: finalByte = 'C'; break;
    case Qt::Key_Left:  finalByte = 'D'; break;
    default: return {};
    }
    
    if (m_modeState->applicationCursorKeysEnabled()) {
        return QByteArray("\x1bO") + finalByte;  // SS3 格式
    } else {
        return QByteArray("\x1b[") + finalByte;  // CSI 格式
    }
}
```

### 功能键：F1 ~ F20

```cpp
QByteArray QTermInputEncoder::encodeFunctionKey(Qt::Key key) {
    // F1 = "SS3 P", F2 = "SS3 Q", ..., F5 = "SS3 S"
    // F6 = "CSI 17 ~", F7 = "CSI 18 ~", ...
    
    if (key >= Qt::Key_F1 && key <= Qt::Key_F4) {
        char codes[] = {'P', 'Q', 'R', 'S'};
        return QByteArray("\x1bO") + codes[key - Qt::Key_F1];
    }
    
    if (key >= Qt::Key_F5 && key <= Qt::Key_F12) {
        int code = 15 + (key - Qt::Key_F5);  // 15-22
        return QByteArray("\x1b[") + QByteArray::number(code) + "~";
    }
    
    return {};
}
```

### 小键盘：应用小键盘模式（DECNKM）

当启用应用小键盘模式时（`CSI ? 66 h`），小键盘发送应用定义的序列：

| 键 | 数字模式 | 应用模式 |
|-----|---------|----------|
| 0 | `0` | `SS3 p` |
| 1 | `1` | `SS3 q` |
| 2 | `2` | `SS3 r` |
| ... | ... | ... |
| . | `.` | `SS3 n` |
| Enter | `Enter` | `SS3 M` |

```cpp
QByteArray QTermInputEncoder::encodeKeypadKey(Qt::Key key) {
    if (!m_modeState->applicationKeypadEnabled()) {
        // 数字模式：按键自身的数字字符
        switch (key) {
        case Qt::Key_0: return "0";
        case Qt::Key_1: return "1";
        // ...
        case Qt::Key_Period: return ".";
        case Qt::Key_Return: return "\r";
        default: return {};
        }
    }
    
    // 应用模式
    char code;
    switch (key) {
    case Qt::Key_0: code = 'p'; break;
    case Qt::Key_1: code = 'q'; break;
    case Qt::Key_2: code = 'r'; break;
    case Qt::Key_3: code = 's'; break;
    case Qt::Key_4: code = 't'; break;
    case Qt::Key_5: code = 'u'; break;
    case Qt::Key_6: code = 'v'; break;
    case Qt::Key_7: code = 'w'; break;
    case Qt::Key_8: code = 'x'; break;
    case Qt::Key_9: code = 'y'; break;
    case Qt::Key_Period: code = 'n'; break;
    case Qt::Key_Return: code = 'M'; break;
    default: return {};
    }
    
    return QByteArray("\x1bO") + code;
}
```

### 修饰键：Shift、Ctrl、Alt 组合

CSI 键盘协议使用参数编码修饰符：

```
CSI ? ... m
          ↑ 修饰符代码
          1 = Shift
          2 = Alt (Meta)
          3 = Shift+Alt
          4 = Ctrl
          5 = Shift+Ctrl
          6 = Alt+Ctrl
          7 = Shift+Alt+Ctrl
```

```cpp
QByteArray QTermInputEncoder::encodeModifiedKey(Qt::Key key, Qt::KeyboardModifiers mods) {
    int modCode = 0;
    if (mods & Qt::ShiftModifier) modCode |= 1;
    if (mods & Qt::AltModifier) modCode |= 2;
    if (mods & Qt::ControlModifier) modCode |= 4;
    
    // 例如：Ctrl+Home
    if (key == Qt::Key_Home) {
        if (modCode > 0) {
            return QByteArray("\x1b[1;") + QByteArray::number(modCode + 1) + "H";
        } else {
            return "\x1b[H";
        }
    }
    
    return {};
}
```

---

## 可打印字符与 Ctrl 合成

### 普通按键

如果 `event->text()` 非空，直接使用 UTF-8 编码的字符：

```cpp
QByteArray QTermInputEncoder::encodeKey(const QKeyEvent *event) {
    // 1. 检查特殊键（如上所述）
    QByteArray special = encodeSpecialKey(event->key(), event->modifiers());
    if (!special.isEmpty()) {
        return special;
    }
    
    // 2. 检查可打印字符
    if (!event->text().isEmpty()) {
        // 如果是可打印字符，返回 UTF-8 编码
        return event->text().toUtf8();
    }
    
    // 3. 都不是，返回空
    return {};
}
```

### Ctrl 组合：手动合成

当用户按 Ctrl+A、Ctrl+B 等时，某些平台（特别是 macOS）的 Qt 可能不能正确报告 `event->text()`。
此时需要手动合成 C0 控制字符：

```cpp
// C0 控制字符映射（0x00 ~ 0x1F）
const QMap<Qt::Key, unsigned char> CTRL_KEY_MAP = {
    {Qt::Key_A, 0x01},  // Ctrl+A = SOH
    {Qt::Key_B, 0x02},  // Ctrl+B = STX
    {Qt::Key_C, 0x03},  // Ctrl+C = ETX (SIGINT)
    {Qt::Key_D, 0x04},  // Ctrl+D = EOT (EOF)
    {Qt::Key_E, 0x05},  // Ctrl+E = ENQ
    {Qt::Key_F, 0x06},  // Ctrl+F = ACK
    {Qt::Key_G, 0x07},  // Ctrl+G = BEL
    {Qt::Key_H, 0x08},  // Ctrl+H = BS (Backspace)
    {Qt::Key_I, 0x09},  // Ctrl+I = TAB
    {Qt::Key_J, 0x0A},  // Ctrl+J = LF (Enter)
    {Qt::Key_K, 0x0B},  // Ctrl+K = VT (Vertical Tab)
    {Qt::Key_L, 0x0C},  // Ctrl+L = FF (Form Feed，清屏)
    {Qt::Key_M, 0x0D},  // Ctrl+M = CR (Carriage Return)
    {Qt::Key_N, 0x0E},  // Ctrl+N = SO
    {Qt::Key_O, 0x0F},  // Ctrl+O = SI
    {Qt::Key_P, 0x10},  // Ctrl+P = DLE
    {Qt::Key_Q, 0x11},  // Ctrl+Q = DC1 (XON)
    {Qt::Key_R, 0x12},  // Ctrl+R = DC2
    {Qt::Key_S, 0x13},  // Ctrl+S = DC3 (XOFF)
    {Qt::Key_T, 0x14},  // Ctrl+T = DC4
    {Qt::Key_U, 0x15},  // Ctrl+U = NAK (清行)
    {Qt::Key_V, 0x16},  // Ctrl+V = SYN
    {Qt::Key_W, 0x17},  // Ctrl+W = ETB (删除单词)
    {Qt::Key_X, 0x18},  // Ctrl+X = CAN
    {Qt::Key_Y, 0x19},  // Ctrl+Y = EM (恢复)
    {Qt::Key_Z, 0x1A},  // Ctrl+Z = SUB (SIGSTOP)
    {Qt::Key_BracketLeft, 0x1B},   // Ctrl+[ = ESC
    {Qt::Key_Backslash, 0x1C},     // Ctrl+\ = FS
    {Qt::Key_BracketRight, 0x1D},  // Ctrl+] = GS
    {Qt::Key_AsciiCaret, 0x1E},    // Ctrl+^ = RS
    {Qt::Key_Underscore, 0x1F},    // Ctrl+_ = US
};

QByteArray QTermInputEncoder::encodeKey(const QKeyEvent *event) {
    // Ctrl 键按下且 text() 为空（某些平台的行为）
    if ((event->modifiers() & Qt::ControlModifier) && event->text().isEmpty()) {
        if (CTRL_KEY_MAP.contains(event->key())) {
            unsigned char ctrlByte = CTRL_KEY_MAP[event->key()];
            return QByteArray(1, ctrlByte);
        }
    }
    
    // … 其他处理 …
}
```

---

## macOS 特殊处理

### 问题：NSTextInputClient 拦截

macOS 的 Cocoa 框架会拦截某些按键组合，防止 Qt 接收到它们：

- Ctrl+A → 移到行首（Emacs 风格）
- Ctrl+B → 向后移动一字符
- Ctrl+E → 移到行尾
- Ctrl+F → 向前移动一字符
- Ctrl+P → 前一行（历史）
- Ctrl+N → 后一行（历史）

QTermInputEncoder 需要检测并补偿：

```cpp
#ifdef Q_OS_MACOS
QByteArray QTermInputEncoder::encodeMacOSKey(const QKeyEvent *event) {
    // 当 Ctrl 被按下但 text() 为空时，说明 NSTextInputClient 拦截了
    if ((event->modifiers() & Qt::ControlModifier) && event->text().isEmpty()) {
        // 查表合成 C0 字符
        return synthesizeControlCharacter(event->key());
    }
    
    return {};
}

QByteArray QTermInputEncoder::synthesizeControlCharacter(Qt::Key key) {
    // 根据 key 返回对应的 C0 字符
    // Ctrl+A → 0x01, Ctrl+B → 0x02, 等等
    if (CTRL_KEY_MAP.contains(key)) {
        return QByteArray(1, CTRL_KEY_MAP[key]);
    }
    return {};
}
#endif
```

### Alt/Option 键：通常是 Meta 键

在 macOS 上，Alt/Option 通常被 Cocoa 拦截用于输入 Unicode 字符。
对于终端应用，应将其视为 Meta 键（ESC 前缀）：

```cpp
QByteArray QTermInputEncoder::encodeMacOSAltKey(const QKeyEvent *event) {
    if (event->modifiers() & Qt::AltModifier) {
        // Alt+x → ESC x
        if (!event->text().isEmpty()) {
            return QByteArray("\x1b") + event->text().toUtf8();
        }
    }
    return {};
}
```

---

## 完整编码表：常见按键

### 字母和数字

| 按键 | 输出 | 说明 |
|------|------|------|
| `a` | `a` | ASCII 字符 |
| `A` (Shift+a) | `A` | 大写 |
| `Ctrl+a` | `0x01` | C0 SOH |
| `1` | `1` | 数字 |
| Space | ` ` | 空格 (0x20) |

### 功能键

| 按键 | 编码 | 说明 |
|------|------|------|
| F1 | `SS3 P` | ESC O P |
| F12 | `CSI 24 ~` | |
| Home | `CSI H` | 或 `ESC [ 1 ~ ` |
| End | `CSI F` | 或 `ESC [ 4 ~ ` |
| PageUp | `CSI 5 ~` | |
| PageDown | `CSI 6 ~` | |
| Insert | `CSI 2 ~` | |
| Delete | `CSI 3 ~` | |
| Backspace | `0x08` | BS (退格) |
| Tab | `0x09` | HT (水平制表) |
| Return/Enter | `0x0D` | CR (回车) |

### 方向键（正常模式）

| 按键 | 编码 |
|------|------|
| ↑ | `CSI A` |
| ↓ | `CSI B` |
| → | `CSI C` |
| ← | `CSI D` |

### 方向键（应用光标模式，DECCKM 启用）

| 按键 | 编码 |
|------|------|
| ↑ | `SS3 A` |
| ↓ | `SS3 B` |
| → | `SS3 C` |
| ← | `SS3 D` |

---

## 集成到前端：示例

### Qt Quick

```qml
QTermQuickItem {
    id: terminal
    
    Keys.onPressed: function(event) {
        // QTermViewController 已在后端处理
        // 此处主要用于调试或自定义键处理
        console.log("Key pressed:", event.key, "Text:", event.text);
    }
}
```

### QWidget

```cpp
void TerminalWidget::keyPressEvent(QKeyEvent *event) {
    // 将事件转发给 ViewController（已在基类实现）
    if (m_controller->handleKeyPress(event)) {
        event->accept();
        return;
    }
    
    // 默认处理
    QTermWidget::keyPressEvent(event);
}
```

---

## 调试：查看原始字节

如果需要调试键盘编码，可以启用 debugPlainText 查看原始发送的字节：

```cpp
// 在 QTermCore 中
void QTermCore::setDebugEnabled(bool enabled) {
    m_debugEnabled = enabled;
}

void QTermCore::write(const QByteArray &data) {
    if (m_debugEnabled) {
        // 打印原始字节（十六进制）
        QString hex;
        for (unsigned char byte : data) {
            hex += QString("%1 ").arg((int)byte, 2, 16, QChar('0'));
        }
        qDebug() << "Key bytes:" << hex;
    }
    
    // 继续正常处理
    m_parser.parse(data, executor);
}
```

示例输出：
```
Key bytes: 1b 4f 41    # Ctrl+A（在应用光标模式下）= SS3 A
Key bytes: 03           # Ctrl+C = 0x03 (SIGINT)
Key bytes: 1b 1b       # Alt+ESC = ESC ESC
```

---

## 相关文档

- [frontend.md](../architecture/frontend.md) — 按键事件的前端处理
- [selection-clipboard.md](selection-clipboard.md) — 快捷键绑定（Ctrl+Shift+C/V）
- [../architecture/core.md](../architecture/core.md) — 字节序列如何被 Parser 解析
