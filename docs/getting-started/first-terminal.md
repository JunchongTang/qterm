# 第一个终端：Qt Quick 最小示例

本文展示如何用最少的代码在 Qt Quick 应用中嵌入一个可用的本地 shell 终端。
完整可运行示例见 `examples/quick-demo/`。

---

## 前置条件

- Qt 6.5 或更高版本（推荐 6.8+）
- CMake 3.21+
- Unix / macOS（`QTermLocalPtyBackend` 使用 POSIX PTY）

---

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyTermApp)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2)

# 将 QTerm 作为子目录引入（或用 find_package，见 cmake-integration.md）
add_subdirectory(qterm)

qt_add_executable(my_term_app main.cpp)

qt_add_qml_module(my_term_app
    URI MyTermApp
    VERSION 1.0
    QML_FILES Main.qml
)

target_link_libraries(my_term_app
    PRIVATE
        Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::QuickControls2
        QTerm::qterm
        qtermplugin          # QML 类型注册插件
)

qt_import_qml_plugins(my_term_app)   # 静态构建时必须
```

---

## main.cpp

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");

    QQmlApplicationEngine engine;
    engine.loadFromModule("MyTermApp", "Main");

    return engine.rootObjects().isEmpty() ? -1 : app.exec();
}
```

---

## Main.qml

```qml
import QtQuick
import QtQuick.Controls
import QTerm                          // QTermTerminal, QTermSession, QTermLocalPtyBackend, QTermQuickItem

ApplicationWindow {
    width: 800; height: 500
    visible: true
    title: terminal.title || "Terminal"
    color: "#0d1117"

    // ── 1. 会话后端：本地 PTY，启动默认 shell ──────────────────────────────
    QTermLocalPtyBackend {
        id: ptyBackend
        program: ""                   // 空字符串 → 自动使用 $SHELL 或 /bin/sh
    }

    // ── 2. 会话封装 ────────────────────────────────────────────────────────
    QTermSession {
        id: session
        backend: ptyBackend
    }

    // ── 3. 终端核心（高层门面） ─────────────────────────────────────────────
    QTermTerminal {
        id: terminal
        session: session
    }

    // ── 4. 渲染前端 ────────────────────────────────────────────────────────
    QTermQuickItem {
        id: termView
        anchors.fill: parent
        terminal: terminal
        focus: true
    }

    // ── 5. 会话生命周期 ─────────────────────────────────────────────────────
    Component.onCompleted: session.open()
    Component.onDestruction: session.close()
}
```

这五个对象对应 QTerm 架构的五个层：  
`QTermLocalPtyBackend` → `QTermSession` → `QTermTerminal` → `QTermQuickItem`，  
再加上窗口标题绑定到 `terminal.title` 体现 Surface 层暴露的 OSC 标题更新。

---

## 添加滚动条

`QTermQuickItem` 暴露 `scrollPosition`（0.0–1.0）和 `scrollSize`（可见区域比例）属性，
可以直接绑定到 QtQuick.Controls 的 `ScrollBar`：

```qml
import QtQuick.Controls

Item {
    anchors.fill: parent

    QTermQuickItem {
        id: termView
        anchors.fill: parent
        terminal: terminal
        focus: true
    }

    ScrollBar {
        id: scrollBar
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        size: termView.scrollSize
        policy: ScrollBar.AsNeeded

        // 双向绑定：terminal → scrollbar，scrollbar（拖动时）→ terminal
        Binding {
            property: "position"
            value: termView.scrollPosition
            when: !scrollBar.pressed
        }
        onPositionChanged: {
            if (pressed)
                termView.scrollPosition = position
        }
    }
}
```

---

## 处理复制粘贴

`QTermQuickItem` 发出 `copyRequested(text)` 信号；粘贴通过 `terminal.sendPaste(text)` 触发：

```qml
QTermQuickItem {
    // …
    onCopyRequested: (text) => Qt.application.clipboard.text = text
}

// 绑定 Ctrl+Shift+V 快捷键
Shortcut {
    sequence: "Ctrl+Shift+V"
    onActivated: terminal.sendPaste(Qt.application.clipboard.text)
}
```

---

## 下一步

- [CMake 集成](cmake-integration.md) — find_package 路径、静态 / 动态构建说明
- [会话后端](../guides/session-backends.md) — 串口、Telnet 后端参数说明
- [滚动历史](../guides/scrollback.md) — scrollback 行数限制、键盘滚动绑定
- [架构概览](../architecture/overview.md) — 理解五层数据流
