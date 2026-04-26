# QTermViewControllerBase 多渲染后端共享控制层

## TL;DR

把 `QTermQuickPaintedItem` 当前的 4 类职责拆分：渲染无关的逻辑提取到一个 `QTermViewController`（纯 `QObject`），三种渲染 widget 只继承各自的 Qt 基类并**组合** controller。不用 C++ 多重继承，不引入虚函数渲染接口。

---

## 职责层次分析

当前 `QTermQuickPaintedItem` 包含 4 类完全不同的逻辑：

| 职责类型 | 方法 | 可共用？ |
|---------|------|---------|
| **A. Terminal 绑定** | `setTerminal`, `reconnectSurfaceModel`, `updateMouseAcceptance`, `hyperlinkIdAtPosition` | ✅ 100% |
| **B. 输入处理** | `keyPressEvent`, `mousePressEvent`, `mouseMoveEvent`, `mouseReleaseEvent`, `hoverMoveEvent`, `wheelEvent`, `inputMethodEvent`, `updateSelectionFromDrag`, click streak 逻辑 | ✅ 90%（坐标转换依赖 cellWidth/Height，其余纯逻辑） |
| **C. 尺寸/字体/滚动管理** | `geometryChange`, `syncTerminalSize`, `scrollPosition/Size`, `updateMetrics` | ✅ 90% |
| **D. 渲染** | `paint()`, color helpers, cursor delegate, `updateCursorDelegateGeometry` | ❌ 完全不同 |

---

## 架构设计

```
QTermViewController  (QObject，新增，可单独测试)
  ├── terminal 属性绑定（A 全部）
  ├── 输入事件分发（B：接收标准化事件，返回"已消耗/转发"决策）
  ├── 尺寸/滚动逻辑（C）
  └── signals: copyRequested, hyperlinkActivated, wheelScrolled

QTermQuickPaintedItem  (QQuickPaintedItem + QTermViewController)
  ├── override paint()              ← 唯一渲染差异
  ├── 转发 Qt 事件 → controller
  └── 转发 geometryChange → controller

QTermQuickItem  (QQuickItem + QTermViewController)    ← 未来 QSG 版
  ├── override updatePaintNode()    ← 唯一渲染差异
  └── 转发事件 → controller

QTermWidget  (QWidget + QTermViewController)          ← 未来 QWidget 版
  ├── override paintEvent()         ← 唯一渲染差异
  └── 转发事件 → controller
```

**关键设计决策：不用公共基类，用组合（Composition）。**
原因：三个 widget 的 Qt 基类（`QQuickPaintedItem` / `QQuickItem` / `QWidget`）完全不同，强行用虚函数基类会形成 diamond inheritance，且无法让 QML 透明使用。

---

## `QTermViewController` 公开接口草图

```cpp
class QTermViewController : public QObject {
    Q_OBJECT
public:
    // ── Terminal 绑定 ──────────────────────────────────────
    QTermTerminal *terminal() const;
    void setTerminal(QTermTerminal *);

    // ── 字体/外观（Widget 用来计算 cellSize） ──────────────
    QString fontFamily() const; void setFontFamily(const QString &);
    int fontPixelSize() const;  void setFontPixelSize(int);
    qreal cellWidth() const;    // 由字体计算
    qreal cellHeight() const;

    // ── 尺寸/滚动 ─────────────────────────────────────────
    void notifyGeometryChanged(qreal w, qreal h); // Widget 在 geometryChange 里调用
    qreal scrollPosition() const; void setScrollPosition(qreal);
    qreal scrollSize() const;

    // ── 输入事件分发 ──────────────────────────────────────
    // 返回 true = 已消耗（Widget 调用 event->accept()）
    bool handleKeyPress(QKeyEvent *);
    bool handleInputMethod(QInputMethodEvent *);
    bool handleMousePress(QMouseEvent *);
    bool handleMouseMove(QMouseEvent *);
    bool handleMouseRelease(QMouseEvent *);
    bool handleMouseDoubleClick(QMouseEvent *);
    bool handleHoverMove(QHoverEvent *);
    bool handleWheel(QWheelEvent *);

    // ── 坐标辅助（Widget 覆盖以提供 offset） ──────────────
    int rowAtPosition(qreal y) const;
    int columnAtPosition(qreal x) const;
    int hyperlinkIdAtPosition(int row, int col) const;

    // ── 鼠标协议状态 ──────────────────────────────────────
    bool mouseTrackingEnabled() const;
    bool hoverEventsNeeded() const;

signals:
    void terminalChanged();
    void metricsChanged();         // cellWidth/Height 变了 → Widget 请求 repaint + resize
    void scrollChanged();
    void wheelScrolled(int offset);
    void copyRequested(const QString &text);
    void hyperlinkActivated(const QString &url);
    void mouseAcceptanceChanged(); // Widget 据此更新 setAcceptedMouseButtons
    void repaintNeeded();          // surface 变了 → Widget 调用 update()
    void terminalSizeChanged(int cols, int rows); // → Widget 更新尺寸提示（可选）
};
```

---

## 三个 Widget 剩余代码量估算

每个 Widget 只需：
1. 持有 `QTermViewController m_controller`
2. 在构造函数里连接 `m_controller` 的信号到自身（`repaintNeeded → update()`、`mouseAcceptanceChanged → setAcceptedMouseButtons()`）
3. 在各 `override` 事件里委托给 `m_controller.handleXxx(event)`
4. 实现自己的 `paint` / `updatePaintNode` / `paintEvent`

**每个 Widget 自身代码 ≈ 100–150 行（当前是 ~1000 行）。**

---

## 实施步骤（可分阶段，不破坏现有外部接口）

### Phase A — 提取 Controller（不改 Widget 外部接口）

1. 新建 `src/quick/QTermViewController.h` 和 `src/quick/QTermViewController.cpp`
2. 把 `QTermQuickPaintedItem` 的 B+C 类逻辑**迁移**进去（先复制，再改 Widget 为委托）
3. `QTermQuickPaintedItem` 持有 `m_controller`，所有现有方法改为转发，对外 API 不变
4. 验证：`qterm_core_tests` 83 passed + quick demo 行为不变

### Phase B — 接入 QTermQuickItem（QSG 渲染）

*depends on Phase A*

1. 新建 `src/quick/QTermQuickItem.h` 和 `src/quick/QTermQuickItem.cpp`，继承 `QQuickItem`
2. 复用 `QTermViewController`，只实现 `updatePaintNode()`
3. `src/CMakeLists.txt` 注册类型
4. 在 demo 里切换，用 Instruments 对比帧率

### Phase C — 接入 QTermWidget

*depends on Phase A，可与 Phase B 并行*

1. 新建 `src/widget/QTermWidget.h` 和 `src/widget/QTermWidget.cpp`，继承 `QWidget`
2. 复用 `QTermViewController`，只实现 `paintEvent()`
3. 新增 `examples/widget-demo/`

---

## 相关文件

| 文件 | 状态 |
|------|------|
| `include/QTerm/QTermQuickPaintedItem.h` | 公开接口不变（外部 API 稳定） |
| `src/quick/QTermQuickPaintedItem.cpp` | Phase A 后大幅精简（~150 行） |
| `src/quick/QTermViewController.h/.cpp` | 新增（核心共享层） |
| `src/quick/QTermQuickItem.h/.cpp` | Phase B 新增 |
| `src/widget/QTermWidget.h/.cpp` | Phase C 新增 |
| `src/CMakeLists.txt` | 各阶段新增 source 注册 |

---

## 关键设计决策

- **不用虚函数 renderer 接口**：避免 diamond inheritance，QML 注册更简单
- **Controller 放 `src/quick/`**：依赖 Qt Quick（`QHoverEvent` 等），不是纯 core；若未来需要 QWidget 独立发布可拆出到 `src/ui/`
- **Controller 不继承 QQuickItem/QWidget**：保持可单独 new 测试
- **Widget 事件转发用 `handle*()` 返回 `bool`**：简洁，不需要 event filter
- **颜色/调色板属性留在 Widget 侧**：这些是渲染参数，和输入逻辑无关，controller 不需要感知

---

## 验证标准

1. `ninja qterm qterm_core_tests qterm_quick_demo` 编译无报错
2. `./tests/qterm_core_tests` 83 passed 0 failed
3. quick demo 手工验证：键盘、鼠标、选区、滚动、超链接行为与重构前完全一致
4. Phase B 后：在大终端（200×50，`cat` 大文件）下用 Instruments 对比 CPU 占用
