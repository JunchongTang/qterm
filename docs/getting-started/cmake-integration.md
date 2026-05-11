# CMake 集成

本文说明如何将 QTerm 加入你的 CMake 项目。

---

## 方式一：add_subdirectory（推荐）

将 QTerm 仓库作为子目录（或 Git 子模块）放入你的项目：

```
myapp/
├── CMakeLists.txt
├── main.cpp
├── Main.qml
└── qterm/            ← git submodule 或直接拷贝
    ├── CMakeLists.txt
    └── …
```

在你的顶层 `CMakeLists.txt` 中：

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyApp)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2)

add_subdirectory(qterm)

qt_add_executable(myapp main.cpp)

qt_add_qml_module(myapp
    URI MyApp
    VERSION 1.0
    QML_FILES Main.qml
)

target_link_libraries(myapp
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        QTerm::qterm      # 核心库 + 会话 + QML 类型
        qtermplugin       # QML 插件（注册 QTermTerminal 等 QML_ELEMENT 类型）
)

# 静态构建时必须，确保 QML 插件链接进可执行文件
qt_import_qml_plugins(myapp)
```

### 链接目标说明

| 目标 | 说明 |
|------|------|
| `QTerm::qterm` | 核心库，包含所有 C++ 类；同时包含 QML 模块（URI `QTerm`）的类型注册 |
| `qtermplugin` | Qt Quick 插件目标，动态加载时由 Qt Quick 自动发现；静态构建时需显式链接 |

---

## 方式二：find_package（系统安装或预构建）

如果已将 QTerm 安装到系统或某个前缀路径：

```cmake
find_package(QTerm REQUIRED)

target_link_libraries(myapp PRIVATE QTerm::qterm qtermplugin)
```

设置安装前缀：

```bash
cmake -S qterm -B qterm-build -DCMAKE_INSTALL_PREFIX=/opt/qterm
cmake --build qterm-build --target install
cmake -S myapp -B myapp-build -DCMAKE_PREFIX_PATH=/opt/qterm
```

---

## 最低 Qt 版本

| 组件 | 最低版本 |
|------|---------|
| Qt Core / Gui | 6.5 |
| Qt Quick / Qml | 6.5 |
| Qt Quick Controls 2 | 6.5（仅 demo 需要，库本身不依赖） |
| Qt Serial Port | 6.5（仅 `QTermSerialBackend` 需要） |

推荐使用 Qt 6.8 或更高版本以获得最佳场景图性能。

---

## 仅使用 QWidget 前端

如果你的项目不使用 Qt Quick，只需链接 `QTerm::qterm` 并引入
`<QTerm/QTermWidget.h>`：

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
add_subdirectory(qterm)

qt_add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE Qt6::Widgets QTerm::qterm)
```

QWidget 前端不依赖 Qt Quick 或 QML，无需 `qtermplugin`。

---

## 禁用不需要的后端

通过 CMake 选项可以裁剪不需要的会话后端：

```cmake
add_subdirectory(qterm)
# 以下选项默认均为 ON，按需关闭：
# set(QTERM_BUILD_SERIAL_BACKEND OFF)   # 不构建串口后端（不依赖 Qt Serial Port）
# set(QTERM_BUILD_TELNET_BACKEND OFF)   # 不构建 Telnet 后端
```

> **注**：以上选项尚在规划中，当前版本所有后端均会构建。
