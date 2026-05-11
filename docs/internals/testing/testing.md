# 测试指南

本文描述 QTerm 的测试矩阵、如何运行各套测试，以及 golden test 的文件结构。

---

## 测试矩阵

| 套件 | 测试目标 | 运行方式 | 依赖 GUI |
|------|---------|---------|---------|
| `qterm_core_tests` | 协议解析、Buffer 语义、光标操作、选区、resize reflow | `ninja qterm_core_tests && ./tests/qterm_core_tests` | 否 |
| `qterm_session_tests` | QTermSession / QTermSessionBackend 状态机 | `ninja qterm_session_tests && ./tests/qterm_session_tests` | 否 |
| `qterm_local_pty_tests` | 本地 PTY 后端的打开/关闭/写入/SIGWINCH | `ninja qterm_local_pty_tests && ./tests/qterm_local_pty_tests` | 否（但需要 `/dev/ptmx`） |
| 手工回归测试 | 渲染、鼠标、键盘、tmux、vim、htop | 见 [manual-testing.md](manual-testing.md) | 是 |

---

## 快速运行

```bash
cd /path/to/qterm/build

# 构建所有测试目标
ninja qterm_core_tests qterm_session_tests qterm_local_pty_tests

# 运行所有测试（via CTest）
ctest --output-on-failure

# 只运行核心测试，过滤名称
./tests/qterm_core_tests --gtest_filter='*Resize*'
```

---

## 核心测试（`qterm_core_tests`）

入口文件：`tests/core/QTermCoreTest.cpp`

测试覆盖：

- **协议解析**：CSI 光标移动、DECSTBM 滚动区域、SGR 属性、OSC 标题/超链接/shell 集成
- **Buffer 语义**：`scrollUp`、`insertLines`、`deleteLines`、历史行积累
- **Resize reflow**：单行/多行/跨历史行的宽度变化；`scrollBottom` 全屏区域在 resize 后保持全屏
- **选区**：单点、范围、逻辑行全选、`selectedText` 提取
- **输入编码**：Ctrl+ 组合键、应用光标键模式、bracketed paste

### 添加新测试

测试基础类 `QTermCoreTest` 已包含初始化好的 `QTermCore` 实例和常用辅助方法：

```cpp
// tests/core/QTermCoreTest.cpp
TEST_F(QTermCoreTest, MyFeature)
{
    feed("\e[5;10H");        // 调用 feed() 向核心发送字节序列
    EXPECT_EQ(core.cursorState().row, 4);
    EXPECT_EQ(core.cursorState().column, 9);
}
```

---

## 会话测试（`qterm_session_tests`）

入口文件：`tests/session/QTermSessionTest.cpp`

测试覆盖：

- `QTermSession` 状态转换（Closed → Open → Closed）
- 后端替换（运行时换 backend）
- `dataReceived` 信号透传

---

## 本地 PTY 测试（`qterm_local_pty_tests`）

入口文件：`tests/session/QTermLocalPtyBackendTest.cpp`

测试覆盖：

- 打开/关闭 PTY
- 写入数据并验证回显
- resize 触发 SIGWINCH 后子进程响应

---

## 手工回归测试

详见：

- [manual-testing.md](manual-testing.md) — 鼠标、键盘、选区、滚动条等 UI 验证步骤
- [manual-testing-tmux.md](manual-testing-tmux.md) — tmux 专项测试：分屏、vim、htop、VT 序列兼容性
