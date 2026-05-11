# 指南：文本选择与剪贴板

本文讨论 QTerm 如何处理鼠标选择、系统剪贴板交互以及远程剪贴板协议（OSC 52）。

---

## 选择模型

### 选择何时可用

选择（Selection）仅在**鼠标追踪关闭**时可用。当应用请求鼠标事件（vim、tmux、less）时，
所有鼠标事件被转发到应用，前端无法进行选择：

```
鼠标追踪关闭（默认）：
  用户鼠标操作 → 前端选择文本 → 复制到剪贴板

鼠标追踪开启（Mode 1002/1003）：
  用户鼠标操作 → 后端应用（如 vim）捕获事件 → 应用决定行为
  前端选择被禁用
```

### 选择类型

QTerm 支持三种鼠标选择操作：

| 操作 | 快捷方式 | 行为 |
|------|---------|------|
| 单击 | 左键单击 | 清空选择，设置光标位置 |
| 双击 | 左键双击 | 扩展选择到单词边界（word selection） |
| 三击 | 左键三击 | 选择整行 |
| 拖拽 | 左键按住+移动 | 从锚点（单击位置）到当前位置的区域 |
| 扩展 | Shift+click | 从旧选择端点扩展到新位置 |

### 单词边界检测

双击时需要识别单词边界。通常定义为：

```cpp
bool isWordCharacter(QChar c) {
    return c.isLetterOrNumber() || c == '_' || c == '-';
}

QString getWordAt(int row, int col) {
    const QTermLine &line = terminal()->line(row);
    
    // 反向扩展到单词开始
    int startCol = col;
    while (startCol > 0 && isWordCharacter(line.cell(startCol - 1).character())) {
        startCol--;
    }
    
    // 正向扩展到单词结束
    int endCol = col;
    while (endCol < line.length() - 1 && isWordCharacter(line.cell(endCol + 1).character())) {
        endCol++;
    }
    
    // 提取单词文本
    QString word;
    for (int c = startCol; c <= endCol; ++c) {
        word += line.cell(c).character();
    }
    return word;
}
```

### 行尾处理：soft wrap vs hard wrap

终端有两种行尾：

1. **Hard wrap**（硬换行）：用户或应用按 Enter，`\r\n` 或 `\n`
2. **Soft wrap**（软换行）：文本超过宽度，自动换行（wrappedToNextLine=true）

选择时，soft wrap 连接多个物理行为一个逻辑行：

```
物理行 0: "This is a " (wrappedToNextLine=true)
物理行 1: "long line"  (wrappedToNextLine=false)

用户双击"long"：
  逻辑行="This is a long line"
  双击选择结果="long"（不包括"line"）
```

---

## 系统剪贴板交互

### 复制流程

当用户有选择时，通常按 Ctrl+Shift+C 或从右键菜单"复制"：

```cpp
// Frontend 捕获 Ctrl+Shift+C
bool QTermViewController::handleKeyPress(const QKeyEvent *event) {
    if (event->key() == Qt::Key_C && 
        (event->modifiers() & Qt::ControlModifier) &&
        (event->modifiers() & Qt::ShiftModifier)) {
        
        if (m_selectionModel->hasSelection()) {
            QString text = m_selectionModel->selectedText();
            QGuiApplication::clipboard()->setText(text);
            return true;
        }
    }
    return false;
}
```

### 粘贴流程

粘贴通常按 Ctrl+Shift+V：

```cpp
if (event->key() == Qt::Key_V && 
    (event->modifiers() & Qt::ControlModifier) &&
    (event->modifiers() & Qt::ShiftModifier)) {
    
    QString text = QGuiApplication::clipboard()->text();
    
    // 进行括号化粘贴（Bracketed Paste Mode）协商
    if (m_modeState->isBracketedPasteModeEnabled()) {
        // 模式 2004：CSI ? 2004 h
        // 格式：CSI 200 ~ + 数据 + CSI 201 ~
        m_terminal->sendKey(QByteArray("\x1b[200~"));
        m_terminal->sendKey(text.toUtf8());
        m_terminal->sendKey(QByteArray("\x1b[201~"));
    } else {
        // 直接发送
        m_terminal->sendKey(text.toUtf8());
    }
    return true;
}
```

### 括号化粘贴模式（Bracketed Paste Mode）

Bracketed Paste Mode（BPM）是现代 shell 和编辑器推荐的粘贴协议。
用途：区分用户真实输入和粘贴数据。

```
启用：CSI ? 2004 h
禁用：CSI ? 2004 l

粘贴时：
  CSI 200 ~     ← 粘贴开始标记
  [粘贴内容]     ← 数据
  CSI 201 ~     ← 粘贴结束标记

好处：
  - Shell 不误解粘贴的特殊字符（如 Ctrl+C）
  - 编辑器可区分打字和粘贴（不同的撤销粒度）
  - 脚本粘贴不会触发 shell 命令执行
```

---

## OSC 52：远程剪贴板

OSC 52 允许远程应用（通过 SSH 或 Telnet 连接的进程）访问**本地**剪贴板。
格式由 RFC 暂存规范 (iTerm2) 定义：

```
OSC 52 ; c ; base64-encoded-data ST
         ↑
         c = 剪贴板选择器 (c=clipboard, p=primary, s=secondary 等)
```

### 写入远程剪贴板（应用 → 本地剪贴板）

应用需要将内容复制到本地：

```bash
# 在远程 SSH 会话中执行：
echo "hello" | xclip -selection clipboard -i

# 或使用 OSC 52（某些应用支持）：
printf '\e]52;c;%s\e\\' "$(echo -n 'hello' | base64)"
```

QTerm 应处理此 OSC 序列：

```cpp
// Parser 识别 OSC 52 并调用：
executor.requestClipboardWrite(dataBase64);

// Executor 回调到 Core：
void QTermCore::onRequestClipboardWrite(const QString &dataBase64) {
    // 1. Base64 解码
    QByteArray decodedData = QByteArray::fromBase64(dataBase64.toUtf8());
    
    // 2. 写入系统剪贴板
    QGuiApplication::clipboard()->setText(QString::fromUtf8(decodedData));
    
    // 3. 发送反馈（可选）
    // OSC 52 ; c ; ? ST  表示"应答成功"
    backend()->writeData("\x1b]52;c;?\x1b\\");
}
```

### 读取远程剪贴板（本地剪贴板 → 应用）

应用如果支持 OSC 52 读取，可以请求本地剪贴板内容：

```
OSC 52 ; c ; ? ST  → 请求剪贴板内容

QTerm 响应：
OSC 52 ; c ; base64-encoded-clipboard ST
```

实现（可选）：

```cpp
void QTermInputExecutor::requestClipboardRead() {
    // 读取本地剪贴板
    QString clipboardText = QGuiApplication::clipboard()->text();
    QByteArray base64Data = clipboardText.toUtf8().toBase64();
    
    // 构造 OSC 52 响应
    QByteArray response = "\x1b]52;c;" + base64Data + "\x1b\\";
    
    // 回送给应用
    m_outboundHandler(response);
}
```

---

## 在 Qt Quick 中绑定

### 信号连接模式

```qml
import QtQuick
import QTerm

ApplicationWindow {
    QTermQuickItem {
        id: terminal
        
        // 当复制被请求时（用户选择 + Ctrl+Shift+C）
        Component.onCompleted: {
            terminal.copyRequested.connect(function(text) {
                // 将文本写入系统剪贴板
                // 注：Qt Quick 没有直接剪贴板 API，需要 C++ 支持
            });
        }
    }
    
    // 粘贴快捷键
    Shortcut {
        sequences: ["Ctrl+Shift+V"]
        onActivated: {
            // 需要 C++ 侧实现 pasteFromClipboard()
            terminal.pasteFromClipboard();
        }
    }
}
```

### C++ 侧支持

```cpp
class TerminalWidget : public QObject {
    Q_OBJECT
    Q_PROPERTY(QTermQuickItem *terminal MEMBER m_terminal)
    
public slots:
    void pasteFromClipboard() {
        QString text = QGuiApplication::clipboard()->text();
        if (!text.isEmpty()) {
            m_terminal->terminal()->sendPaste(text.toUtf8());
        }
    }
    
    void copyToClipboard(const QString &text) {
        QGuiApplication::clipboard()->setText(text);
    }
    
private:
    QTermQuickItem *m_terminal;
};
```

---

## 在 QWidget 中绑定

### 上下文菜单

```cpp
class TerminalWidget : public QTermWidget {
protected:
    void contextMenuEvent(QContextMenuEvent *event) override {
        QMenu menu;
        
        QAction *copyAction = menu.addAction(tr("复制"));
        connect(copyAction, &QAction::triggered, this, [this]() {
            if (selectionModel()->hasSelection()) {
                QString text = selectionModel()->selectedText();
                QGuiApplication::clipboard()->setText(text);
            }
        });
        
        QAction *pasteAction = menu.addAction(tr("粘贴"));
        connect(pasteAction, &QAction::triggered, this, [this]() {
            QString text = QGuiApplication::clipboard()->text();
            sendPaste(text.toUtf8());
        });
        
        menu.exec(event->globalPos());
    }
};
```

### 快捷键处理

```cpp
void TerminalWidget::keyPressEvent(QKeyEvent *event) {
    // Ctrl+Shift+C：复制
    if (event->key() == Qt::Key_C && 
        event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        if (selectionModel()->hasSelection()) {
            QGuiApplication::clipboard()->setText(selectionModel()->selectedText());
        }
        event->accept();
        return;
    }
    
    // Ctrl+Shift+V：粘贴
    if (event->key() == Qt::Key_V && 
        event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        QString text = QGuiApplication::clipboard()->text();
        sendPaste(text.toUtf8());
        event->accept();
        return;
    }
    
    QTermWidget::keyPressEvent(event);
}
```

---

## 选择高亮渲染

### Qt Quick 版本

```qml
QTermQuickItem {
    id: terminal
    
    // 获取当前选择范围
    function getSelectionRanges() {
        return terminal.selectionModel.ranges;  // [{startRow, startCol, endRow, endCol}, ...]
    }
}

// 在 Canvas 上绘制选择高亮
Canvas {
    onPaint: {
        var ctx = getContext("2d");
        var ranges = terminal.getSelectionRanges();
        
        ctx.fillStyle = "rgba(100, 150, 255, 0.4)";  // 半透明蓝色
        
        for (var range of ranges) {
            for (var row = range.startRow; row <= range.endRow; ++row) {
                var x = (row === range.startRow ? range.startCol : 0) * charWidth;
                var y = row * charHeight;
                var w = ((row === range.endRow ? range.endCol : columnsCount) - (row === range.startRow ? range.startCol : 0)) * charWidth;
                ctx.fillRect(x, y, w, charHeight);
            }
        }
    }
}
```

### QWidget 版本（paintEvent）

```cpp
void TerminalWidget::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    
    // 1. 绘制文本和背景（如常）
    // …
    
    // 2. 绘制选择高亮
    if (selectionModel()->hasSelection()) {
        const auto &ranges = selectionModel()->ranges();
        
        painter.setOpacity(0.4);
        painter.fillRect(event->rect(), QColor(100, 150, 255));  // 半透明蓝色
        
        for (const auto &range : ranges) {
            for (int row = range.startRow; row <= range.endRow; ++row) {
                int startCol = (row == range.startRow) ? range.startCol : 0;
                int endCol = (row == range.endRow) ? range.endCol : columnsCount;
                
                QRect selectionRect(
                    startCol * fontWidth(), row * fontHeight(),
                    (endCol - startCol) * fontWidth(), fontHeight()
                );
                painter.fillRect(selectionRect, QColor(100, 150, 255));
            }
        }
        
        painter.setOpacity(1.0);
    }
}
```

---

## 处理 Wrap 行的选择

当选择跨越 soft wrap 边界时，需要正确处理：

```cpp
QString getSelectionText(const SelectionRange &range) {
    QString result;
    
    for (int row = range.startRow; row <= range.endRow; ++row) {
        const QTermLine &line = terminal()->line(row);
        
        int startCol = (row == range.startRow) ? range.startCol : 0;
        int endCol = (row == range.endRow) ? range.endCol : line.length();
        
        // 提取该行的文本
        for (int col = startCol; col < endCol; ++col) {
            result += line.cell(col).character();
        }
        
        // 如果该行不是 soft wrap 结尾，添加换行符
        if (row < range.endRow && !line.wrappedToNextLine()) {
            result += '\n';
        }
    }
    
    return result;
}
```

---

## 相关文档

- [frontend.md](../architecture/frontend.md) — 鼠标事件处理、鼠标协议
- [input-keymap.md](input-keymap.md) — 键盘快捷键编码
- [../guides/session-backends.md](session-backends.md) — OSC 52 在不同 Backend 中的传输
