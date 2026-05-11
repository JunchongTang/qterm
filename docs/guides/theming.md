# 主题与配色

QTerm 使用三个相互配合的类管理终端外观：

| 类 | 说明 |
|----|------|
| `QTermTheme` | 单套已解析颜色 + 可选字体设置；`Q_GADGET` 值类型，可直接在 QML 中传递 |
| `QTermThemePack` | 多变体容器（dark / light / …）；负责"跟随系统"逻辑 |
| `QTermThemeLoader` | 从 `.qtheme` JSON 文件加载主题包 |

前端控件（`QTermQuickItem`、`QTermQuickPaintedItem`、`QTermWidget`）只消费
`QTermTheme`；变体选择由应用层通过 `QTermThemePack` 完成。

---

## 颜色分层

### 基础色（5 项）

| 字段 | 说明 |
|------|------|
| `foreground` | 默认前景色（无 SGR 时） |
| `background` | 窗口背景色 |
| `selection` | 选区高亮背景色 |
| `cursor` | 光标颜色 |
| `hyperlinkTint` | OSC 8 超链接着色 |

### ANSI 16 色（index 0–15）

终端程序通过 `\e[30m`…`\e[97m` 设置的颜色。  
index 0–7 为普通色（Normal），index 8–15 为亮色（Bright）：

| index | 名称 | SGR 前景 | SGR 背景 |
|-------|------|---------|---------|
| 0 | black | `\e[30m` | `\e[40m` |
| 1 | red | `\e[31m` | `\e[41m` |
| … | … | … | … |
| 8 | brightBlack | `\e[90m` | `\e[100m` |
| … | … | … | … |
| 15 | brightWhite | `\e[97m` | `\e[107m` |

> `brightBlack`（index 8）通常作为注释色使用（俗称 darkGray）。

### 256 色与 True Color

index 16–255 的 256 色 cube 和灰阶按固定公式计算，不存入主题文件。  
True Color（24-bit RGB，`\e[38;2;R;G;Bm`）由程序直接指定，主题不参与。

---

## 使用内置主题

`QTermThemePack::qtermDefault()` 返回内置的深浅色包：

```cpp
// C++
const auto theme = QTerm::QTermThemePack::qtermDefault().variant("dark");
termQuickItem->loadTheme(theme);
```

```qml
// QML：在 Component.onCompleted 中应用
Component.onCompleted: {
    themeHelper.applyDarkTheme(termView)
}
```

---

## 从文件加载主题

`.qtheme` 文件格式为 JSON，支持单变体和多变体两种形式。

### 单变体文件

```jsonc
{
  "name": "One Dark",
  "version": 1,
  "darkMode": true,
  "foreground":    "#abb2bf",
  "background":    "#282c34",
  "selection":     "#3e4451",
  "cursor":        "#528bff",
  "hyperlinkTint": "#61afef",
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

### 多变体文件（深浅色同包）

```jsonc
{
  "name": "Solarized",
  "version": 1,
  "variants": {
    "dark": {
      "darkMode": true,
      "foreground": "#839496",
      "background": "#002b36",
      "palette": { "…": "…" }
    },
    "light": {
      "darkMode": false,
      "foreground": "#657b83",
      "background": "#fdf6e3",
      "palette": { "…": "…" }
    }
  }
}
```

有 `variants` 时，顶层的颜色字段被忽略，变体之间完全独立（无继承）。

### 加载代码

```cpp
QTerm::QTermThemeLoader loader;
const auto pack = loader.load("/path/to/theme.qtheme");
if (pack.has_value()) {
    const auto theme = pack->resolveForSystem();  // 跟随系统深浅色
    termItem->loadTheme(theme);
}
```

---

## 字体设置

`QTermTheme` 包含可选的字体覆盖字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `fontFamily` | QString | 字体族名称；空字符串表示不覆盖，使用控件自身字体 |
| `fontPixelSize` | int | 字体像素大小；0 表示不覆盖 |

```cpp
QTermTheme theme = pack.variant("dark");
theme.setFontFamily("JetBrains Mono");
theme.setFontPixelSize(14);
termItem->loadTheme(theme);
```

---

## 跟随系统深浅色

```cpp
// resolveForSystem() 检查 QGuiApplication::palette().window() 的亮度决定变体
const auto theme = pack.resolveForSystem();
```

在 QML 中响应系统主题切换：

```qml
Connections {
    target: Qt.styleHints
    function onColorSchemeChanged() {
        themeHelper.applyDarkTheme(termView)   // 或 applyLightTheme
    }
}
```

---

## 相关文档

- JSON 格式规范和 ANSI 色名对照表见上文的表格部分
