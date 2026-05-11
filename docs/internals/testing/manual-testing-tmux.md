# tmux 回归测试指南

本文档描述如何用 tmux 手动验证 QTerm 的终端协议兼容性，以及执行自动化单元测试。

前缀键：所有快捷键都先按 Ctrl+B，再按命令键

操作	快捷键
左右分屏	Ctrl+B → %
上下分屏	Ctrl+B → "
切换窗格	Ctrl+B → 方向键
新建窗口	Ctrl+B → c
切换窗口	Ctrl+B → 0~9
暂时脱离会话	Ctrl+B → d
重新连接会话	tmux attach
显示所有会话	tmux ls

## 启动方式

```bash
# 构建 quick-demo
cd /path/to/qterm/build
ninja qterm_quick_demo

# 运行演示程序
./examples/quick-demo/qterm_quick_demo
```

在 QTerm 窗口中执行：

```bash
tmux
```

---

## 基础功能检查清单

### 1. 启动画面
- [ ] tmux 能够启动，不卡死（`CSI ? 1049 h` 切换备用屏幕）
- [ ] 底部状态栏正常显示（包含会话名、窗口名、时间）
- [ ] 状态栏颜色正常（绿色/黑色背景）

### 2. 分屏
```
Ctrl+B  %      # 左右分屏
Ctrl+B  "      # 上下分屏
Ctrl+B  方向键  # 切换窗格
```
- [ ] 分割线用 `│` / `─` 等 Unicode 框线字符正确渲染（不显示 `x` / `q` 等 ASCII 字母）
- [ ] 窗格内容各自独立，切换后不花屏

### 3. 应用内测试
在 tmux 内依次运行：

```bash
vim       # 检查：界面完整，不卡启动（依赖 CSI 6n DSR）
htop      # 检查：颜色正常，刷新不花屏
less /etc/hosts   # 检查：翻页、q 退出正常
```

### 4. 滚动 / Reverse Index
```bash
# 在 tmux 内运行 man ls，然后按 k 或方向键上翻
man ls
```
- [ ] 内容上翻时屏幕顶部不出现乱码（依赖 `ESC M` Reverse Index）

### 5. 会话管理
```
Ctrl+B  d      # 脱离会话
tmux attach    # 重新连接
```
- [ ] 重连后界面恢复正常

---

## tmux 所需的关键 VT 序列

| 序列 | 含义 | 状态 |
|------|------|------|
| `CSI ? 1049 h/l` | 备用屏幕 + 保存/恢复光标 | ✅ 已实现 |
| `CSI ? 25 h/l` | 光标显示/隐藏 | ✅ 已实现 |
| `CSI H` / `CSI f` | 光标定位 | ✅ 已实现 |
| `CSI 2 J` | 清屏 | ✅ 已实现 |
| `CSI K` | 清行 | ✅ 已实现 |
| `CSI S/T` | 滚动区域上/下滚 | ✅ 已实现 |
| `CSI 6n` (DSR) | 光标位置查询 | ✅ 已实现 |
| `CSI c` (DA1/DA2) | 设备属性查询 | ✅ 已实现 |
| `SGR` 颜色/属性 | 文字样式 | ✅ 已实现 |
| `OSC 0/2` | 窗口标题 | ✅ 已实现 |
| `ESC M` | Reverse Index（上滚一行） | ✅ 已实现 |
| `ESC ( 0` / `ESC ( B` | DEC 线图字符集切换 | ✅ 已实现 |
| `CSI s` / `CSI u` | ANSI 保存/恢复光标 | ✅ 已实现 |
| `ESC =` / `ESC >` | 应用/普通小键盘模式 | ✅ 已实现 |
| `CSI ? 2026 h/l` | 同步输出（忽略即可） | ✅ 静默忽略 |

---

## 自动化单元测试

```bash
cd /path/to/qterm/build
./tests/qterm_core_tests
```

与 tmux 相关的测试用例：
- `supportsReverseIndex` — ESC M 在滚动区域顶端插入空行
- `supportsDecLineDrawing` — ESC(0 映射 j/k/l/m/n/q/x 为框线字符，ESC(B 恢复
- `supportsSaveCursorAnsi` — CSI s / CSI u 等价于 ESC 7 / ESC 8

---

## 常见问题排查

| 现象 | 可能原因 |
|------|----------|
| 分割线显示为 `x q j` 等字母 | DEC 线图字符集未实现 |
| vim/htop 启动时卡顿 | DSR `CSI 6n` 未响应 |
| 上翻内容时顶部出现乱行 | `ESC M` Reverse Index 未实现 |
| 小键盘方向键失效 | 应用小键盘模式 `ESC =` 未处理 |
| 窗格切换后花屏 | 备用屏幕状态管理问题 |
