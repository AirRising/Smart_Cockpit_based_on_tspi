# 01 · 环境准备与第一次运行

配套：[本章练习题](exercises/01-environment.md) · [全部题集](exercises/README.md)

[学习路线](README.md) · 下一章：[C++ 与 Qt 入门](02-cpp-and-qt.md)

## 本章目标

在 Linux 桌面上编译并运行现有项目，认识工具链和目录。此时先不要求读懂实现，也不接真实车辆。后面的章节会从空文件重新写小程序。

## 1. 先认识五个工具

| 工具 | 做什么 | 不负责什么 |
| --- | --- | --- |
| C++ 编译器 `g++` | 将 `.cpp` 编译为机器代码，再链接 | 不会替你安装 Qt |
| CMake | 根据目标、源文件、库生成构建规则 | 本身不是 C++ 编译器 |
| Qt Widgets | 窗口、按钮、布局、事件循环、信号槽 | 不会自动理解汽车协议 |
| GStreamer | 音频播放、视频采集、格式处理 | 不是本项目的页面框架 |
| CTest / Qt Test | 组织并执行测试、断言行为 | 不能代替真实显示与设备验收 |

“头文件找不到”通常是编译阶段问题；“undefined reference”通常是链接阶段问题；“找不到 Qt platform plugin”是运行阶段问题。先判断阶段，再查对应依赖。

## 2. 推荐环境

主线推荐带桌面的 Ubuntu 22.04 或更新版本、Debian 12，使用 C++17、Qt 5.15、CMake ≥ 3.21。项目 [CMake 配置](../CMakeLists.txt) 虽声明 CMake ≥ 3.16、Qt5 ≥ 5.12，但历史上 `Qt::hex/dec` 需要 Qt ≥ 5.14（现已改为 `QString::number(id, 16)` + `qDebug().noquote()`，在板端 Qt 5.12.8 上实测编译通过），本文的 `ctest --test-dir` 需要 CMake ≥ 3.20；声明的最低版本不代表已验证兼容。[构建预设](../CMakePresets.json) 也需要 CMake ≥ 3.21。本教程用显式 `-S/-B` 命令，统一采用上述推荐版本。

Qt6 开关是 `SMART_COCKPIT_USE_QT6`，最低 6.2。但初学阶段不要同时切换 Qt 版本、编译器和平台，先跑通 Qt5 主线。Windows/macOS 不具备这里的 Linux SocketCAN 和部署环境，不作为本教程的完整运行基线。

### 安装依赖：会修改开发机系统

下面适用于使用 apt 的开发机，需你有安装权限。这里只提供命令，不要求在目标板上盲目执行相同操作。

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config python3 \
  qtbase5-dev libqt5opengl5-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-tools gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good gstreamer1.0-alsa fonts-noto-cjk
```

媒体章节可能还需与你的音频格式匹配的解码插件，例如 `gstreamer1.0-plugins-ugly` 或 `gstreamer1.0-libav`；不必为了第一扇窗口先安装所有插件。`can-utils` 和 `iproute2` 留到虚拟 CAN 实验使用。

本项目没有依赖 Qt Multimedia 来播放音频，不要因为媒体报错就先去寻找 `QMediaPlayer`。

### 检查工具是否可见

```bash
g++ --version
cmake --version
pkg-config --modversion Qt5Widgets Qt5DBus Qt5Test
pkg-config --modversion gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0
gst-inspect-1.0 videotestsrc
gst-inspect-1.0 appsink
gst-inspect-1.0 playbin
gst-inspect-1.0 alsasink
```

`pkg-config` 输出版本说明开发描述文件可找到，不代表插件、设备或最终屏幕已经正常。`gst-inspect-1.0` 用来确认运行时元素存在。

## 3. 设置路径并检查仓库

在一个终端中执行；后续命令沿用这些变量：

```bash
PROJECT=/home/air/qt/Smart_Cockpit_based_on_tspi
HOST_BUILD="$HOME/build/smart-cockpit-host"
WORK="$HOME/smart-cockpit-lab"
ls "$PROJECT"
ls "$PROJECT/src" "$PROJECT/docs" "$PROJECT/tests"
```

路径不同就修改 `PROJECT`。不要在参考仓库里执行不理解的清理命令，也不要覆盖已有的 `build` 目录。这里特意把产物放在仓库外。

## 4. 配置、编译、测试：三个不同步骤

```bash
cmake -S "$PROJECT" -B "$HOST_BUILD" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DSMART_COCKPIT_USE_QT6=OFF
cmake --build "$HOST_BUILD" -j2
ctest --test-dir "$HOST_BUILD" --output-on-failure
```

- `-S` 是源码目录，必须包含 `CMakeLists.txt`。
- `-B` 是构建目录，CMake 会创建它。不要拿它存唯一一份手写代码。
- `Debug` 保留适合调试的信息；`-j2` 同时做两个构建任务，内存紧张可用 `-j1`。
- 配置失败就先修配置，不要接着尝试运行一个并未生成的程序。
- 当前注册六个测试程序。看到六项通过表示这批用例通过，不表示没有其他 bug。

三个构建目标分别是纯 C++ 库、Qt 服务与 UI 库、最终可执行程序。纯 C++ 库不链接 Qt，但顶层配置仍然查找 Qt/GStreamer；“不链接 Qt”不等于“这个顶层工程可以在未安装 Qt 时直接配置”。

## 5. 不接硬件运行模拟座舱

在**已经登录桌面的本机终端**中运行：

```bash
"$HOST_BUILD/smart-cockpit" --sim \
  --log-dir "$HOME/.local/state/smart-cockpit"
```

预期：

1. 出现全屏窗口，有仪表、媒体、空调页面。
2. 仪表数值随着模拟数据变化。
3. 每轮模拟约 60 秒，45～52 秒进入倒车时段，摄像头默认显示 GStreamer 小球测试源。
4. 可通过底部导航或 F1/F2/F3 切普通页面；倒车期间普通导航不能抢走页面。

用桌面窗口管理器关闭窗口可以退出事件循环并走正常清理流程。终端 `Ctrl+C` 会发送中断信号，并不等于应用实现了优雅退出处理，不能用它证明日志已排空。

### `--sim` 到底模拟了什么

它跳过 CAN socket，每 100ms 直接向 `CarService` 注入仪表、挡位、转角帧。它**没有模拟空调控制器，不会回传 0x200 确认帧**。所以调整空调后约 600ms 未确认而回滚，并不是“模拟器失灵”；07 会教你主动注入 ACK 验证成功路径。

摄像头与 `--sim` 是两件事：不传 `--camera`，空路径选择测试源；传了真实设备路径却打开失败，不会自动改成测试源。

### 参数速查

参数定义见 [程序入口](../src/main.cpp)。

| 参数 | 含义 | 注意 |
| --- | --- | --- |
| `--sim` | 不使用真实或虚拟 CAN | 不产生空调 ACK |
| `--can vcan0` | 非模拟模式下选 CAN 接口 | 默认 vcan0，但应用不会创建该接口 |
| `--camera /dev/video0` | 使用指定视频设备 | 设备须存在且支持协商的格式 |
| `--dbc 路径` | 加载外部 DBC 子集 | 名称和布局必须与业务层匹配 |
| `--log-dir 路径` | 指定日志目录 | 普通用户优先用自己的可写目录 |
| `--media-dir 路径` | 指定音乐扫描目录 | 不递归扫描子目录 |

查看参数可执行：

```bash
QT_QPA_PLATFORM=offscreen "$HOST_BUILD/smart-cockpit" --help
```

这里只是避免查看帮助时依赖显示服务器，**不是推荐把完整视频 UI 放在 offscreen 上验收**。

## 6. 桌面、离屏和板端显示不是一回事

- 桌面开发：Qt 连接现有 X11/Wayland 会话；多数情况下不要手动设置平台插件。
- 离屏测试：`QT_QPA_PLATFORM=offscreen` 可以测试许多普通控件，不保证 QOpenGLWidget 获得可用的真实渲染环境。
- 板端直显：eglfs/KMS 需要设备、驱动、EGL 和权限。留到 12 逐项核对，不能在普通桌面上强行设置 eglfs 来“解决窗口打不开”。

SSH 登录默认可能没有显示会话。不要照抄他人的 `DISPLAY=:0` 或 Xauthority 路径；先确认当前会话和权限。若暂时没有桌面，可以先做纯 C++ 章节与离屏测试，但要记录尚未完成视觉验收。

## 7. 常见错误的排查顺序

| 现象 | 先检查 |
| --- | --- |
| 找不到 Qt5WidgetsConfig.cmake | 是否安装开发包，是否混入别的 Qt 路径 |
| 找不到 gstreamer-app-1.0 | 插件基础开发包与 pkg-config 搜索路径 |
| 构建被 killed | 内存是否不足，降低并行度 |
| 启动找不到 platform plugin | Qt 插件与库是否同版本、是否处于有效桌面会话 |
| 中文变方框 | 中文字体是否安装并可被当前会话发现 |
| 摄像头 NO SIGNAL | 查看日志、测试源插件、实际设备权限；不要只盯 UI |
| 音频失败 | 是否存在输出设备、alsasink、对应解码插件 |
| 模拟模式 frames 显示 0 | 计数来自真实 CanManager 接收，不统计直接注入的模拟帧 |

不要用 `sudo` 启动整个 GUI 来掩盖权限错误；这还可能带来新的显示授权问题。

## 动手与验收

- [ ] 能解释 `PROJECT` 和 `HOST_BUILD` 的区别。
- [ ] 完成一次 Debug 构建，记录实际测试结果。
- [ ] 在真实桌面看到仪表变化和测试视频；若缺显示环境，明确记录未验证。
- [ ] 能说明为什么 `--sim` 下空调会超时。
- [ ] 找到日志文件，而不是假设所有 Qt 日志仍打印在终端。

下一步不急着改成品，去 [02](02-cpp-and-qt.md) 从一个空文件写自己的第一扇窗口。
