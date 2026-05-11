# 第一个终端：QWidget 最小示例

本文展示如何用纯 C++ 和 QWidget 嵌入一个可用的本地 shell 终端，无需 QML 或 Qt Quick。

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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# 将 QTerm 作为子目录引入
add_subdirectory(qterm)

qt_add_executable(my_term_app
    main.cpp
)

target_link_libraries(my_term_app
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        QTerm::qterm    # 不需要 qtermplugin（纯 QWidget）
)
```

---

## main.cpp

```cpp
#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QClipboard>

#include <QTerm/QTermWidget.h>
#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermSession.h>
#include <QTerm/QTermLocalPtyBackend.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // ── 创建主窗口 ───────────────────────────────────────────────────────
    auto *mainWindow = new QMainWindow();
    mainWindow->setWindowTitle("QTerm");
    mainWindow->resize(800, 500);

    // ── 创建中央 widget 和布局 ───────────────────────────────────────────
    auto *centralWidget = new QWidget();
    auto *layout = new QHBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    // ── 1. 会话后端：本地 PTY ────────────────────────────────────────────
    auto *backend = new QTerm::QTermLocalPtyBackend();
    backend->setProgram("");  // 空字符串 → 自动使用 $SHELL 或 /bin/sh

    // ── 2. 会话 ──────────────────────────────────────────────────────────
    auto *session = new QTerm::QTermSession();
    session->setBackend(backend);

    // ── 3. 终端核心（高层门面） ──────────────────────────────────────────
    auto *terminal = new QTerm::QTermTerminal();
    terminal->setSession(session);

    // ── 4. QWidget 渲染前端 ──────────────────────────────────────────────
    auto *termWidget = new QTerm::QTermWidget();
    termWidget->setTerminal(terminal);
    termWidget->setFontFamily("Menlo");      // macOS 系统字体
    termWidget->setFontPixelSize(13);

    // ── 5. 滚动条 ────────────────────────────────────────────────────────
    auto *scrollBar = new QScrollBar(Qt::Vertical);

    // 绑定滚动范围和位置
    auto updateScrollBar = [=]() {
        const int maxScroll = terminal->maxScrollOffset();
        const int curScroll = terminal->scrollOffset();
        scrollBar->setRange(0, maxScroll);
        scrollBar->setValue(maxScroll - curScroll);
        scrollBar->setPageStep(terminal->rows());
        scrollBar->setVisible(maxScroll > 0);
    };

    // 初始更新
    updateScrollBar();

    // Terminal 内容变化时更新滚动条
    QObject::connect(terminal, &QTerm::QTermTerminal::viewportChanged,
                     scrollBar, updateScrollBar);

    // 拖动滚动条时跳转历史
    QObject::connect(scrollBar, &QScrollBar::valueChanged, terminal,
                     [=](int value) {
        const int offset = terminal->maxScrollOffset() - value;
        terminal->scrollByLines(offset - terminal->scrollOffset());
    });

    // ── 6. 布局 ──────────────────────────────────────────────────────────
    layout->addWidget(termWidget, 1);  // termWidget 占主要空间
    layout->addWidget(scrollBar);
    centralWidget->setLayout(layout);
    mainWindow->setCentralWidget(centralWidget);

    // ── 7. 复制粘贴 ──────────────────────────────────────────────────────
    QObject::connect(termWidget, &QTerm::QTermWidget::copyRequested,
                     &app, [](const QString &text) {
        QApplication::clipboard()->setText(text);
    });

    // 可选：绑定快捷键用于粘贴
    // Ctrl+Shift+V：手动从剪贴板粘贴
    // （QTermWidget 的 keyPressEvent 会自动处理大多数键盘输入）

    // ── 8. 会话生命周期 ──────────────────────────────────────────────────
    session->open();

    mainWindow->show();
    return app.exec();
}
```

这段代码演示了五层架构在 QWidget 中的对应关系：

```
QTermLocalPtyBackend  ──→ 会话层（Session Layer）
       ↓
QTermSession          ──→ Session 封装
       ↓
QTermTerminal         ──→ 表面层（Surface Layer）
       ↓
QTermWidget           ──→ 前端层（Frontend Layer）
```

---

## 关键点说明

### 无 QML 模块系统

与 Qt Quick 不同，`QTermWidget` 不依赖 QML 模块注册。
`QTermTerminal` 等类型通过标准的 C++ 继承和指针传递组合在一起。

### 手工管理生命周期

与 QML 的自动生命周期不同，C++ 需要显式：
- 创建对象指针（`new`）
- 设置父子关系（通过 parent 参数或 `setParent`）
- 必要时手动 `delete`（或依赖 Qt 的 parent 自动删除）

上例中 `new QTermLocalPtyBackend()` 没有设置 parent，为了简洁起见；
实际项目可以传入 `session` 作为 parent 以确保自动释放。

### 滚动条集成

QWidget 版本的滚动条需要手工管理（QML 有 `ScrollBar` 控件提供高层次绑定）。
关键是将 terminal 的 `scrollOffset` / `maxScrollOffset` 映射到 `QScrollBar` 的
[0, maximum] 范围。

### 字体与颜色

`QTermWidget` 暴露了详细的字体和颜色属性，可根据主题或用户偏好实时调整：

```cpp
termWidget->setFontFamily("JetBrains Mono");
termWidget->setFontPixelSize(14);
termWidget->setBackgroundColor(QColor("#0d1117"));
termWidget->setForegroundColor(QColor("#e6edf3"));
```

---

## 与 Qt Quick 版本对比

| 方面 | Qt Quick | QWidget |
|------|----------|---------|
| 构建系统 | 需要 `qt_add_qml_module` + `qt_import_qml_plugins` | 仅 `target_link_libraries` |
| 依赖 | Qt Quick, Qt QML | 仅 Qt Widgets |
| 代码行数 | ~5 行（QML） + ~10 行（main.cpp） | ~80 行（纯 main.cpp） |
| 响应式绑定 | QML 属性绑定自动更新 | 需要手工 `connect` |
| 布局 | QML anchors 或 Column/Row | QHBoxLayout / QVBoxLayout |
| 定制难度 | 中（QML 上手快但深度定制困难） | 高（需要 C++ 但完全可控） |

---

## 下一步

- [CMake 集成](cmake-integration.md) — 多种项目集成模式
- [会话后端](../guides/session-backends.md) — PTY / 串口 / Telnet 后端配置
- [滚动历史](../guides/scrollback.md) — 高级滚动条技巧
- [架构概览](../architecture/overview.md) — 理解五层架构
