# 滚动历史

QTerm 将已滚出视口的内容保存在 `QTermBuffer` 的历史行中，支持向上滚动查看。
本文介绍滚动历史的工作原理，以及如何在 QML 和 QWidget 中集成滚动条。

---

## 工作原理

终端处于全屏滚动区域（`scrollTop == 0 && scrollBottom == rows - 1`）时，
每次需要在底部新增一行，最顶行就被移入 `m_historyLines`，历史行数随之增加。

`QTermTerminal` 暴露两个属性反映当前滚动状态：

| 属性 | 类型 | 说明 |
|------|------|------|
| `scrollOffset` | int | 当前视口距历史底部的偏移行数；`0` 表示停靠在最新输出 |
| `maxScrollOffset` | int | 历史行总数，即可向上滚动的最大行数 |

前端控件将这两个值换算为 [0, 1] 范围的 `scrollPosition` / `scrollSize`，
供 `ScrollBar` 直接绑定。

---

## 在 Qt Quick 中添加滚动条

`QTermQuickItem` 和 `QTermQuickPaintedItem` 均暴露：

| QML 属性 | 说明 |
|----------|------|
| `scrollPosition` | [0.0, 1.0]，0.0 = 最旧历史顶部，1.0 = 最新输出底部 |
| `scrollSize` | [0.0, 1.0]，当前视口占总内容高度的比例；无历史时为 1.0 |

### 基本集成

```qml
import QtQuick
import QtQuick.Controls
import QTerm

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
        orientation: Qt.Vertical

        // 视口大小比例
        size: termView.scrollSize

        // 双向绑定：terminal → scrollbar（跟随输出）
        // scrollbar（拖动时）→ terminal（跳转历史）
        Binding {
            property: "position"
            value: termView.scrollPosition
            when: !scrollBar.pressed
        }
        onPositionChanged: {
            if (pressed)
                termView.scrollPosition = position
        }

        policy: ScrollBar.AsNeeded
    }
}
```

### 键盘滚动

`QTermQuickItem` 默认接管了 `Shift+PageUp` / `Shift+PageDown` 快捷键，
也可以调用 `QTermTerminal` 的方法手动控制：

```qml
Shortcut {
    sequence: "Shift+Up"
    onActivated: terminal.scrollByLines(-3)
}
Shortcut {
    sequence: "Shift+Down"
    onActivated: terminal.scrollByLines(3)
}
Shortcut {
    sequence: "Shift+PageUp"
    onActivated: terminal.scrollByLines(-terminal.rows)
}
// 回到底部：
Shortcut {
    sequence: "Shift+End"
    onActivated: terminal.scrollToBottom()
}
```

### 鼠标滚轮

`QTermViewController` 默认处理鼠标滚轮事件，3 行/格。
无历史（`maxScrollOffset == 0`）时滚轮事件不消费，可由外层容器处理。

---

## 在 QWidget 中添加滚动条

```cpp
#include <QScrollBar>
#include <QTerm/QTermWidget.h>
#include <QTerm/QTermTerminal.h>

// 在布局中将 QScrollBar 与 QTermWidget 并排放置
auto *termWidget = new QTerm::QTermWidget(this);
auto *scrollBar = new QScrollBar(Qt::Vertical, this);

// 初始化
auto updateScrollBar = [=]() {
    const int max = terminal->maxScrollOffset();
    const int cur = terminal->scrollOffset();
    scrollBar->setRange(0, max);
    scrollBar->setValue(max - cur);  // QScrollBar 0=顶部，terminal 0=底部
    scrollBar->setPageStep(terminal->rows());
};

connect(terminal, &QTerm::QTermTerminal::viewportChanged, scrollBar, updateScrollBar);

// 拖动 scrollBar 时跳转
connect(scrollBar, &QScrollBar::valueChanged, terminal, [=](int value) {
    const int offset = terminal->maxScrollOffset() - value;
    terminal->scrollByLines(offset - terminal->scrollOffset());
});
```

---

## 历史行数限制

当前版本历史行数无上限（受可用内存约束）。
可配置的 `scrollbackLimit` 属性规划中，将在后续版本加入。

---

## 备用屏幕（Alternate Screen）

`vim`、`less`、`htop` 等全屏应用会切换到备用屏幕（`CSI ?1049h`）。
备用屏幕**不保留历史行**——切换回主屏幕后，历史立刻恢复为主屏的历史。
`scrollOffset` 和 `maxScrollOffset` 在备用屏幕下均为 `0`，滚动条自动隐藏（`ScrollBar.AsNeeded`）。
