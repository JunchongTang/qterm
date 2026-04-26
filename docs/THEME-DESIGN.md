# QTerm 主题系统设计文档

## TL;DR

引入 `QTermTheme`（纯数据类）和 `QTermThemeLoader`（文件 I/O），
两个 Widget 通过 `setTheme()` 接收主题，`QTermRenderUtils` 在渲染时消费调色盘。
自有 JSON 格式为主，后续适配 `.itermcolors` / `.terminal` / Alacritty YAML 等。

---

## 一、核心数据结构

### 1.1 调色盘语义

终端颜色分三层：

| 层 | 数量 | 说明 |
|----|------|------|
| **基础色** | 5 | foreground / background / selection / cursor / hyperlinkTint |
| **ANSI 16 色** | 16 | 终端程序用 `\e[30m`…`\e[97m` 设置的颜色 |
| **256 色 cube** | 240 | 固定公式生成，不在主题文件里存储；只有 16 个命名色可覆盖 |

256 色 cube（index 16–231）和灰阶（232–255）**不纳入主题**——公式固定，不同终端行为一致。

### 1.2 ANSI 16 色命名约定

| index | 标准名 | JSON key | 对应 SGR 码 |
|-------|--------|----------|-------------|
| 0 | Normal Black   | `black`         | `\e[30m` / `\e[40m`  |
| 1 | Normal Red     | `red`           | `\e[31m` / `\e[41m`  |
| 2 | Normal Green   | `green`         | `\e[32m` / `\e[42m`  |
| 3 | Normal Yellow  | `yellow`        | `\e[33m` / `\e[43m`  |
| 4 | Normal Blue    | `blue`          | `\e[34m` / `\e[44m`  |
| 5 | Normal Magenta | `magenta`       | `\e[35m` / `\e[45m`  |
| 6 | Normal Cyan    | `cyan`          | `\e[36m` / `\e[46m`  |
| 7 | Normal White   | `white`         | `\e[37m` / `\e[47m`  |
| 8 | Bright Black   | `brightBlack`   | `\e[90m` / `\e[100m` |
| 9 | Bright Red     | `brightRed`     | `\e[91m` / `\e[101m` |
|10 | Bright Green   | `brightGreen`   | `\e[92m` / `\e[102m` |
|11 | Bright Yellow  | `brightYellow`  | `\e[93m` / `\e[103m` |
|12 | Bright Blue    | `brightBlue`    | `\e[94m` / `\e[104m` |
|13 | Bright Magenta | `brightMagenta` | `\e[95m` / `\e[105m` |
|14 | Bright Cyan    | `brightCyan`    | `\e[96m` / `\e[106m` |
|15 | Bright White   | `brightWhite`   | `\e[97m` / `\e[107m` |

**注**：`brightBlack` 俗称 `darkGray`（很多主题把它用作注释色）；`brightWhite` 通常用作最亮前景色。JSON 解析时按 key 名映射到 index，与字段书写顺序无关。

---

## 二、JSON 主题文件格式

文件扩展名：`.qtheme`

### 2.1 单模式文件（最简形式）

```jsonc
{
  "name": "One Dark",
  "version": 1,
  "author": "QTerm",
  "darkMode": true,
  "description": "Atom One Dark ported to QTerm",

  // ── 基础色 ──────────────────────────────────────────────
  "foreground":    "#abb2bf",
  "background":    "#282c34",
  "selection":     "#3e4451",
  "cursor":        "#528bff",
  "hyperlinkTint": "#61afef",

  // ── ANSI 16 色（具名，顺序无关） ────────────────────────
  "palette": {
    "black":         "#282c34",
    "red":           "#e06c75",
    "green":         "#98c379",
    "yellow":        "#e5c07b",
    "blue":          "#61afef",
    "magenta":       "#c678dd",
    "cyan":          "#56b6c2",
    "white":         "#abb2bf",
    "brightBlack":   "#5c6370",
    "brightRed":     "#e06c75",
    "brightGreen":   "#98c379",
    "brightYellow":  "#e5c07b",
    "brightBlue":    "#61afef",
    "brightMagenta": "#c678dd",
    "brightCyan":    "#56b6c2",
    "brightWhite":   "#ffffff"
  }
}
```

### 2.2 多变体文件（深浅色同包）

顶层加 `"variants"` 对象，每个 key 是变体名（约定 `"dark"` / `"light"`，可自定义）。
**有 `variants` 时，顶层的颜色字段被忽略**——变体之间完全独立，无继承关系，避免 diff 合并的歧义。

```jsonc
{
  "name": "Solarized",
  "version": 1,
  "author": "Ethan Schoonover",
  "description": "Precision colors for machines and people",

  "variants": {
    "dark": {
      "darkMode": true,
      "foreground":    "#839496",
      "background":    "#002b36",
      "selection":     "#073642",
      "cursor":        "#268bd2",
      "hyperlinkTint": "#268bd2",
      "palette": {
        "black":         "#073642",
        "red":           "#dc322f",
        "green":         "#859900",
        "yellow":        "#b58900",
        "blue":          "#268bd2",
        "magenta":       "#d33682",
        "cyan":          "#2aa198",
        "white":         "#eee8d5",
        "brightBlack":   "#002b36",
        "brightRed":     "#cb4b16",
        "brightGreen":   "#586e75",
        "brightYellow":  "#657b83",
        "brightBlue":    "#839496",
        "brightMagenta": "#6c71c4",
        "brightCyan":    "#93a1a1",
        "brightWhite":   "#fdf6e3"
      }
    },
    "light": {
      "darkMode": false,
      "foreground":    "#657b83",
      "background":    "#fdf6e3",
      "selection":     "#eee8d5",
      "cursor":        "#268bd2",
      "hyperlinkTint": "#268bd2",
      "palette": {
        "black":         "#073642",
        "red":           "#dc322f",
        "green":         "#859900",
        "yellow":        "#b58900",
        "blue":          "#268bd2",
        "magenta":       "#d33682",
        "cyan":          "#2aa198",
        "white":         "#eee8d5",
        "brightBlack":   "#002b36",
        "brightRed":     "#cb4b16",
        "brightGreen":   "#586e75",
        "brightYellow":  "#657b83",
        "brightBlue":    "#839496",
        "brightMagenta": "#6c71c4",
        "brightCyan":    "#93a1a1",
        "brightWhite":   "#fdf6e3"
      }
    }
  }
}
```

### 2.3 字段规则

**顶层字段**（两种格式共用）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `name` | string | ✅ | 显示名称 |
| `version` | int | ✅ | 格式版本，当前 `1` |
| `author` | string | ❌ | |
| `description` | string | ❌ | |
| `variants` | object | ❌ | 有此字段则为多变体文件；顶层颜色字段被忽略 |

**颜色字段**（单模式下在顶层；多变体模式下在每个 variant 对象内）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `foreground` | hex string | ✅ | 默认前景色 |
| `background` | hex string | ✅ | 默认背景色 |
| `selection` | hex string | ✅ | 选区高亮色 |
| `cursor` | hex string | ✅ | 光标色 |
| `hyperlinkTint` | hex string | ❌ | 超链接着色，默认 `"#6ab0f5"` |
| `palette` | object | ✅ | ANSI 16 色，16 个具名键（见 1.2 节），必须全部提供 |
| `darkMode` | bool | ❌ | 提示 UI 层是否切换暗色控件样式 |

颜色值接受：`"#rrggbb"` / `"#aarrggbb"` / `"#rgb"`（CSS 短格式）/ 标准 CSS 颜色名。

---

## 三、C++ 类设计

### 3.1 `QTermTheme`（纯数据，`include/QTerm/QTermTheme.h`）

```cpp
class QTermTheme {
public:
    // 从默认内建主题构造（Dark）
    QTermTheme();

    // 基础色
    QColor foreground()    const;
    QColor background()    const;
    QColor selection()     const;
    QColor cursor()        const;
    QColor hyperlinkTint() const;

    // ANSI 16 色调色盘（index 0–15）
    QColor paletteColor(int index) const;
    const QColor *palette16() const; // 指向内部 QColor[16]，供 RenderUtils 直接访问

    // 元信息
    QString name() const;
    bool darkMode() const;

    // 内建主题工厂
    static QTermTheme dark();        // 当前默认配色
    static QTermTheme light();
    static QTermTheme solarizedDark();
    static QTermTheme solarizedLight();
    static QTermTheme dracula();
    static QTermTheme oneDark();

private:
    QString  m_name;
    QColor   m_foreground;
    QColor   m_background;
    QColor   m_selection;
    QColor   m_cursor;
    QColor   m_hyperlinkTint = QColor(QStringLiteral("#6ab0f5"));
    QColor   m_palette[16];
    bool     m_darkMode = true;

    friend class QTermThemeLoader;
};
```

### 3.2 `QTermThemeLoader`（文件 I/O，`include/QTerm/QTermThemeLoader.h`）

```cpp
class QTermThemeLoader {
public:
    // ── 单主题加载 ──────────────────────────────────────────────────────────
    // 单模式文件：直接加载。
    // 多变体文件：加载第一个变体（按 JSON key 顺序）。
    static QTermTheme loadFromFile(const QString &path,
                                   bool *ok = nullptr,
                                   QString *errorString = nullptr);

    // 从 JSON 字节流加载（同上规则，用于网络/内嵌资源）
    static QTermTheme loadFromJson(const QByteArray &json,
                                   bool *ok = nullptr,
                                   QString *errorString = nullptr);

    // ── 多变体访问 ──────────────────────────────────────────────────────────
    // 列出文件中所有变体名；单模式文件返回空列表
    static QStringList listVariants(const QString &path);

    // 加载指定变体（variantName = "dark" / "light" / 自定义）
    static QTermTheme loadVariant(const QString &path,
                                  const QString &variantName,
                                  bool *ok = nullptr,
                                  QString *errorString = nullptr);

    // 加载所有变体，key = 变体名
    static QMap<QString, QTermTheme> loadAllVariants(const QString &path,
                                                     bool *ok = nullptr,
                                                     QString *errorString = nullptr);

    // ── 保存 ────────────────────────────────────────────────────────────────
    // 将单个主题保存为单模式 .qtheme 文件
    static bool saveToFile(const QTermTheme &theme, const QString &path,
                           QString *errorString = nullptr);

    // 将多个变体保存为多变体 .qtheme 文件
    // variants: key = 变体名，value = QTermTheme
    static bool saveVariantsToFile(const QString &name,
                                   const QMap<QString, QTermTheme> &variants,
                                   const QString &path,
                                   QString *errorString = nullptr);

    // 序列化为 JSON 字节流（单主题 → 单模式 JSON）
    static QByteArray toJson(const QTermTheme &theme);
};
```

### 3.3 `QTermRenderUtils` 改动

`QTermPaintRequest` 新增一个字段：

```cpp
struct QTermPaintRequest {
    // ...（原有字段不变）...
    const QColor *palette16 = nullptr; // 指向 QTermTheme::palette16()，nullptr = 使用内建
};
```

`qtermColorFromPaletteIndex` 改为接收调色盘指针，index 0–15 走主题（按固定 key→index 映射），16–255 走固定公式：

```cpp
// 内部签名（仅在 QTermRenderUtils.h 中）
QColor qtermColorFromPaletteIndex(int index, const QColor *palette16);
```

### 3.4 Widget 侧 API

`QTermQuickPaintedItem` 和 `QTermWidget` 均新增：

```cpp
// Q_PROPERTY
QTermTheme theme() const;
void setTheme(const QTermTheme &theme);

signals:
    void themeChanged();
```

`setTheme` 内部：
1. 存储 `m_theme`
2. 用主题基础色覆盖 `m_foregroundColor` 等成员
3. `update()` + `emit themeChanged()`

颜色 setter（`setForegroundColor` 等）仍保留——允许逐项覆盖，`setTheme` 只是批量设置的便捷方法。

---

## 四、内建主题配色参考

| 主题 | background | foreground | 来源参考 |
|------|-----------|-----------|---------|
| **QTerm 2026**（当前默认）| `#0b1016` | `#d2f7d0` | 自定义 |

---

## 五、文件布局

```
include/QTerm/
  QTermTheme.h          ← 公开头文件（纯数据 + 内建主题工厂）
  QTermThemeLoader.h    ← 公开头文件（JSON 加载/保存）

src/
  QTermTheme.cpp        ← 内建主题数据 + QTermTheme 实现
  QTermThemeLoader.cpp  ← JSON 解析/序列化（Qt6 JSON API）
  QTermRenderUtils.h    ← 改动：palette16 指针 + 函数签名更新

themes/                 ← 内建 .qtheme 文件（可选，也可硬编码在 cpp 里）
  dark.qtheme
  light.qtheme
  solarized-dark.qtheme
  solarized-light.qtheme
  dracula.qtheme
  one-dark.qtheme
```

---

## 六、后续外部格式适配计划

| 格式 | 文件扩展名 | 优先级 | 说明 |
|------|----------|--------|------|
| iTerm2 | `.itermcolors` | 高 | macOS 最主流，Apple plist XML |
| Alacritty | `.toml` | 高 | 跨平台流行，需 TOML 解析 |
| Windows Terminal | `settings.json` | 中 | JSON，scheme 段提取 |
| Xresources | `.Xresources` / `.xrdb` | 低 | 文本 key-value，Linux 用户需求 |
| Terminator | `.config` | 低 | INI-like |

适配层设计：每个外部格式对应一个 `static QTermTheme fromXxx(...)` 函数（或单独的 `QTermThemeImporter` 命名空间），转换后统一返回 `QTermTheme`。用户可再用 `QTermThemeLoader::saveToFile()` 导出为 `.qtheme`。

---

## 七、实施步骤

### Phase T1 — 核心数据层
1. 实现 `QTermTheme` + 1 个内建主题
2. 实现 `QTermThemeLoader`（JSON 加载/保存）
3. `QTermRenderUtils` 接收 `palette16` 指针
4. `QTermQuickPaintedItem` / `QTermWidget` 加 `setTheme` API
5. quick-demo / widget-demo 加主题切换 UI
6. 验证：83 tests pass；两个 demo 切换主题无闪烁

### Phase T2 — iTerm2 / Alacritty 导入
1. `QTermThemeImporter::fromITermColors(path)`
2. `QTermThemeImporter::fromAlacrittyToml(path)`
3. 在 demo 里加"从文件导入"按钮

### Phase T3 — 主题浏览器（可选）
- 独立的 `QTermThemePicker` QML/Widget 组件，预览缩略图
