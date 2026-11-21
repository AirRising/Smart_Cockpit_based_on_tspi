# 智能座舱（嵌入式 Linux · SocketCAN · GStreamer · eglfs/KMS · Qt Widgets）

面向车载仪表的嵌入式 Linux 应用，目标硬件：立创泰山派 RK3566（Cortex-A55，
2+16GB），Debian 11/12，10.1" 800×1280 竖屏。以 `eglfs + KMS/DRM` 直接接管
显示，**无 X11**；应用层用 C++17 编写，覆盖 SocketCAN 总线收发、GStreamer
音视频管线、异步日志、systemd 部署与 aarch64 交叉编译。UI 层使用 Qt Widgets
作为渲染与事件框架（**不使用 QML**），界面已中文化（车速 / 电机转速 / 电量等）。

## 技术亮点（嵌入式软件能力）

- **CAN 总线通信**：`SocketCAN` 阻塞读线程 + 跨线程队列信号，按 DBC 解析/编码
  0x100–0x400；UI 下发指令后等待车辆 ACK，超时自动回滚。
- **GStreamer 管线**：`playbin` 音频播放；`appsink` 拉取摄像头 NV12 流并转
  RGBA，转换在 streaming 线程完成，不阻塞主渲染线程。
- **嵌入式显示栈**：eglfs + KMS/DRM 无窗口系统直显，systemd 单元开机自启。
- **交叉编译与部署**：aarch64 工具链 + sysroot 交叉构建，rsync + systemd 一键部署。
- **健壮性**：异步文件日志（10MB 滚动）、系统状态机（Normal/Degraded/Emergency）、
  内置信号模拟器（无 CAN/vcan 也能完整演示）。
- **现代车载 HMI 视觉**：统一设计 token（深色底 `#0A0E14`、单色强调 `#2F81F7`、
  语义色 Ok/Warn/Err/Cool/Warm），全局 Theme QSS + 属性选择器，**无任何位图
  素材**——图标全部为 QPainter 路径或 Unicode 几何符号。
- **弱平台渲染优化**：所有自绘控件（仪表/电量/温度盘/专辑占位块）采用
  **静态层 QPixmap 缓存**——表盘、刻度、外壳在 `resizeEvent` 一次性渲染，
  `paintEvent` 只画值弧/指针/读数，无每帧渐变/字体/路径重建；数字读数用等宽
  字体防跳动。
- **空调指令防抖**：连续调节经 250ms debounce 合并为一条 CAN 指令，ACK 前锁面板、
  超时回滚，支持手动取消待确认指令。
- **可测试性**：核心逻辑与 UI 分层，Qt Test 覆盖 CAN 解析、空调 ACK/防抖/取消、
  页面调度、UI 冒烟，并支持 `offscreen` 无头运行。

## 功能模块

| 模块 | 说明 |
| --- | --- |
| CAN 通信 | SocketCAN（vcan0/can0），按 DBC 解析 0x100/0x200/0x300/0x400，内置默认信号表，也可加载 .dbc |
| 信号模拟 | `--sim` 内置合成信号源，**无需 CAN/vcan** 即可演示整个 HMI（板子内核无 CAN 模块时可用） |
| 系统状态机 | Normal / Degraded / Emergency，HealthMonitor 轮询各服务，驱动顶栏状态胶囊（绿/橙/红） |
| 日志 | 异步写 `/var/log/smart-cockpit/`，Info/Warn/Error，单文件 10MB 滚动 |
| 部署 | CMake + aarch64 交叉工具链 + systemd 单元 + 一键部署脚本 |
| 数字仪表 | 竖屏三段布局：大字车速（0–240 km/h）、电机转速（0–8000 rpm）双圆表 + 底部状态卡（挡位 P/R/N/D、电池样式电量、6 枚自绘指示灯），数据来自 CAN 0x100 |
| 中控多媒体 | GStreamer `playbin` 播放本地 MP3/FLAC，ALSA 输出；竖屏上播放下列表布局，自绘专辑占位块（曲名首字母）、QPainter 三联控制键；播放错误上报状态条 |
| 空调面板 | 自绘温度圆表 16–30℃（≤20℃ 冷色 / >20℃ 暖色双色态）、风量 0–7、QPainter 图标的吹风模式/内外循环键、AC/自动；250ms 防抖下发 CAN 0x200，等车辆 ACK，超时回滚 UI，可手动取消 |
| 倒车影像 | R 挡（CAN 0x300 bit0）全屏显示，GStreamer appsink 拉取 NV12 流转 RGBA，QOpenGLWidget 渲染，叠加随方向盘角度变化的轨迹线；下方条带为 R 徽章与标题 |

## 目录结构

```
tspi/
├── CMakeLists.txt
├── cmake/
│   └── toolchain-aarch64-linux-gnu.cmake
├── scripts/
│   ├── vcan-setup.sh            # 主机端创建 vcan0
│   └── demo-can.sh              # 向 vcan0 发送模拟帧
├── deployment/
│   ├── smart-cockpit.service    # systemd 单元（eglfs 环境）
│   ├── kms-config.json
│   └── deploy.sh                # rsync + systemctl 一键部署
├── config/
│   └── vehicle.dbc              # 示例 DBC（--dbc 加载，覆盖内置表）
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── SystemState.h        # SystemState/PageId/PagePriority/ClimateState
│   │   ├── CanTypes.h           # CanSignalDef + 帧 ID（纯 C++）
│   │   ├── CanFrameParserCore.h/.cpp  # DBC 解析/编码核心（纯 C++，无 Qt）
│   │   ├── CanFrameParser.h/.cpp# Qt 适配层：QString/QByteArray <-> std 容器
│   │   ├── CanManager.h/.cpp    # SocketCAN 接收线程（阻塞 read）
│   │   ├── CarService.h/.cpp    # 车辆状态汇总 + 空调确认/回滚
│   │   ├── SignalSimulator.h/.cpp # --sim 内置信号模拟器
│   │   └── HealthMonitor.h/.cpp # 状态机轮询
│   ├── logging/
│   │   ├── AsyncLoggerCore.h/.cpp # 异步文件日志核心（纯 C++，无 Qt）
│   │   └── AsyncLogger.h/.cpp   # Qt 适配层 + qDebug 路由
│   ├── media/
│   │   └── MediaService.h/.cpp  # GStreamer playbin 封装（ALSA 输出）
│   ├── camera/
│   │   ├── CameraService.h/.cpp # appsink NV12 -> RGBA
│   │   └── VideoWidget.h/.cpp   # QOpenGLWidget + 动态轨迹线
│   └── ui/
│       ├── MainWindow.h/.cpp    # 顶栏 + QStackedWidget + 底部导航
│       ├── ScreenManager.h/.cpp # 优先级页面管理
│       ├── Theme.h/.cpp         # 设计 token + 全局 QSS + 等宽数字字体
│       ├── InstrumentPage.h/.cpp# 仪表页（含文件内 LampWidget 指示灯）
│       ├── GaugeWidget.h/.cpp   # QPainter 圆形仪表（静态层缓存）
│       ├── BatteryWidget.h/.cpp # 电池样式电量（静态层缓存）
│       ├── ClimatePage.h/.cpp   # 空调页（含文件内 TempDialWidget/IconButton）
│       ├── MediaPage.h/.cpp     # 媒体页（含文件内 AlbumArt/ElidedLabel/TransportButton）
│       └── ReverseCameraPage.h/.cpp
└── tests/
    ├── CMakeLists.txt
    ├── tst_core_cxx.cpp         # 纯 C++ 核心测试（只链接无 Qt 的 core 库）
    ├── tst_canframeparser.cpp   # DBC 解析/编码测试
    ├── tst_climateservice.cpp   # 空调 ACK/超时回滚/取消按钮/防抖合并测试
    ├── tst_screenmanager.cpp    # 页面抢占/优先级测试
    └── tst_ui.cpp               # UI 冒烟测试（offscreen）
```

## 分层与解耦

代码分为两个 CMake target：

- `smart_cockpit_cxx`：**纯 C++ 标准库实现**，不含任何 Qt 头文件。包含
  `CanFrameParserCore`（DBC 解析/编码）与 `AsyncLoggerCore`（异步文件日志）。
  可脱离 Qt 编译、复用与测试。
- `smart_cockpit_core`：Qt 层。其中的 `CanFrameParser` / `AsyncLogger` 是
  **薄适配层**，只做 `QString`/`QByteArray`/`QHash` 与 `std::string`/
  `std::vector`/`std::unordered_map` 之间的转换，业务逻辑全部委托给核心类。

`tst_core_cxx` 只链接 `smart_cockpit_cxx`，一旦核心模块误引入 Qt 即编译失败，
作为解耦的守卫测试。

## UI 视觉体系

**设计 token**（`Theme.h` 常量，全项目唯一事实来源，禁止 .cpp 内联魔数）：

| 类别 | 值 |
| --- | --- |
| 背景 / 卡片 / 分隔线 | `#0A0E14` / `#161B22` / `#21262D` |
| 主文字 / 次文字 / 禁用 | `#E6EDF3` / `#8B949E` / `#484F58` |
| 强调色 | `#2F81F7`（Accent），文字态强调 `#38BDF8`（Cool） |
| 语义色 | Ok `#22C55E` / Warn `#F59E0B` / Err `#EF4444` / Warm `#FB923C` |
| 圆角 | 8 / 12 / 16 px；间距一律 8 的倍数 |

**实现要点**：

- `Theme::styleSheet()` 一份全局 QSS 管所有标准控件；动态状态（系统健康胶囊、
  CAN 连接点、空调确认状态）用 **QSS 属性选择器**（`[state="ok|warn|err"]`、
  `[connected="true|false"]`）+ `setProperty`/`polish`，零内联样式。
- 数字读数（表盘/温度/电量/时间）统一 `Theme::monoFont()` 等宽字体，刷新不跳动。
- 自绘控件一律**静态层缓存**：`resizeEvent`（或 configure）时把表盘/刻度/外壳
  渲染进成员 `QPixmap`，`paintEvent` 只 blit + 画动态图元；无渐变对象每帧重建、
  无 `QGraphicsEffect`、无常驻动画。
- 文件内私有控件（不新增源文件）：`LampWidget`（指示灯）、`TempDialWidget` 与
  `IconButton`（温度盘/空调模式图标键）、`AlbumArtWidget`/`ElidedLabel`/
  `TransportButton`（专辑块/省略号标签/播放控制键）。

**页面布局**（800×1280 竖屏，stretch 自适应）：

- 仪表页：车速卡 : 转速卡 : 状态卡 = 47 : 30 : 20
- 空调页：温度卡 : 风速卡 : (吹风模式+循环模式) : 底行 = 42 : 18 : 24 : 固定
- 媒体页：播放卡 : 列表卡 = 37 : 63
- 倒车页：视频区（stretch 1）+ 底部条带（标题居中、R 徽章居右，不遮挡画面）

## 主机调试（Ubuntu 20.04）

依赖：

```bash
sudo apt install build-essential cmake qtbase5-dev qtbase5-dev-tools \
     libqt5dbus5 libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
     gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
     gstreamer1.0-plugins-ugly gstreamer1.0-libav \
     libgstreamer-plugins-base1.0-dev libqt5test5 can-utils
```

构建并运行：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

# 模拟 CAN 总线
./scripts/vcan-setup.sh
./scripts/demo-can.sh          # 另一个终端发送模拟帧

./build/smart-cockpit --can vcan0 --camera "" --media-dir ~/Music

# 或：完全不需要 CAN/vcan 的内置模拟（演示/开发最省事）
./build/smart-cockpit --sim --camera "" --media-dir ~/Music
```

未指定 `--camera` 时使用 `videotestsrc` 模拟摄像头；未指定 `--can` 时默认
`vcan0`。桌面环境运行即可看到四个页面（F1/F2/F3 切换，R 帧触发倒车全屏）。
`--sim` 会用内置信号源自动跑"加速→巡航→刹停→倒车→停车"循环，**板子内核缺
CAN/vcan 模块时也能直接演示 HMI**（健康监控会自动把 CAN 视为正常）。

## 交叉编译（RK3566 / aarch64）

准备 sysroot（把泰山派 Debian rootfs 放到 `$SDK_SYSROOT`，内含
`usr/lib/aarch64-linux-gnu/pkgconfig` 等），并安装
`g++-aarch64-linux-gnu`：

```bash
export SDK_SYSROOT=/opt/taisanpi/sysroot
export SDK_QT_ROOT=/opt/taisanpi/qt5        # 可选：Qt 不在 sysroot 中时
cmake -B build-arm \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-linux-gnu.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DSMART_COCKPIT_USE_QT6=OFF
cmake --build build-arm -j$(nproc)
```

部署（`deploy.sh` 内为 rsync + systemd，按需修改 IP/用户）：

```bash
./deployment/deploy.sh root@192.168.1.100 build-arm/smart-cockpit
```

设备侧依赖：`libqt5widgets5 libgstreamer1.0-0 gstreamer1.0-plugins-base
gstreamer1.0-plugins-good fonts-noto-cjk systemd`，以及 `libqt5gui5` 的 eglfs
平台插件（**`fonts-noto-cjk` 必须有，否则中文显示为方块**）。R 挡触发依赖 CAN
0x300 的 bit0；无 CAN/vcan 时用 `--sim` 演示。

## 多线程与信号/槽连接

- **CAN 接收线程**：`CanManager`（QThread）在 `run()` 中阻塞 `read()`；
  每帧通过 `frameReceived(quint32, QByteArray, bool)` 队列信号投递到主线程的
  `CarService::onCanFrame`（`Qt::QueuedConnection`，跨线程自动生效）。
- **摄像头采集线程**：GStreamer appsink 回调在 streaming 线程做 NV12→RGBA
  转换，`CameraService::frameReady(QImage)` 队列投递到主线程
  `VideoWidget::setFrame`（`QImage` 已注册元类型）。
- **主线程**：负责全部 UI 渲染与页面切换。

关键连接（`CarService::start`）：

```cpp
connect(m_can, &CanManager::frameReceived,
        this,   &CarService::onCanFrame, Qt::QueuedConnection);
```

倒车抢占（`MainWindow`）：

```cpp
connect(car, &CarService::reverseChanged, this, &MainWindow::onReverseChanged);
// onReverseChanged: true  -> m_screen->requestPage(ReverseCamera, Urgent)
//                   false -> m_screen->dismissPage(ReverseCamera)
```

## 空调指令链路（防抖 + ACK + 回滚）

```
用户拖动温度/风速滑块（或点模式键）
  → onControlChanged：更新读数，重启 250ms debounce 定时器（不锁面板）
  → 用户停止操作 250ms 后 timer 到期
  → issueClimateCommand：readControls → CarService::sendClimateCommand
  → m_pending = true，锁定面板，「取消待处理」按钮使能
  → 车辆回发 0x200 且 ack=1 → climateAckOk → 解锁，按确认值刷新
  → 600ms 未回 → climateAckTimeout → 解锁，UI 回滚到上次确认值
  → （ pending 期间点「取消待处理」→ 立即按超时路径回滚 ）
```

要点：debounce 窗口内连续调节只产生**一条**指令；`m_syncing` 守卫区分"程序
改控件 vs 用户改控件"，防止 `applyControls` 误触发新指令。

## CAN 帧格式（内置 DBC 表）

| ID | 信号 | 位置/长度 | 说明 |
| --- | --- | --- | --- |
| 0x100 | speed | 0\|16 LE | km/h |
| 0x100 | engine_rpm | 16\|16 LE | rpm |
| 0x100 | fuel_level | 32\|8 LE ×0.5 | % |
| 0x100 | left/right/hazard | 40/41/42 各 1 bit | 转向灯 |
| 0x100 | mil_engine/abs/airbag | 48/49/50 各 1 bit | 故障灯 |
| 0x200 | temp_set / fan_speed / blow_mode / ac_on / auto_mode / ack | 见 `CanFrameParser` | UI→CAN，车辆回传 ack=1 |
| 0x300 | gear_signal | 0\|8 | bit0=R 挡，bits1-3=P/R/N/D |
| 0x400 | steering_angle | 0\|16 LE signed ×0.1 | deg，用于轨迹线 |

如需按真实 DBC 解析，运行 `--dbc your.dbc` 即可覆盖内置表（支持标准
`BO_`/`SG_` 子集）。

## 单元测试（Qt Test）

```bash
ctest --test-dir build --output-on-failure
```

- `tst_core_cxx`：纯 C++ 核心（只链接无 Qt 的 core 库，作为解耦守卫）。
- `tst_canframeparser`：0x100/0x300 解码、DBC 文本加载、0x200 编码回环、截断帧整帧拒绝。
- `tst_climateservice`：ACK 确认链路、超时回滚、DBC 覆盖缺字段保持最后有效状态、
  **取消按钮随 pending 状态使能/禁用**、**debounce 合并连续调节为单条指令**
  （测试直接注入帧，不依赖真实 CAN）。
- `tst_screenmanager`：页面抢占逻辑（正常切页 / 倒车抢占 / 普通请求被拒 / 退出回到原页面）。
- `tst_ui`：UI 冒烟测试——用真实 `CarService` 驱动各页面，验证 CAN 帧更新仪表、
  空调"改值→（debounce 后）发指令→ack→刷新 UI"全链路，并把页面离屏渲染验证
  （`QT_QPA_PLATFORM=offscreen`，无头运行）。

CI：`.github/workflows/ci.yml`（GitHub Actions）在每次 push/PR 自动构建 + 跑全部测试；
本地/云服务器可用 `sudo ./scripts/ci.sh` 跑同一套门禁。

## 生产部署注意事项

- eglfs 下字体：RK3566 上若无中文字体，先安装 `fonts-noto-cjk`（本项目界面已中文化，必须有 CJK 字体）。
- 摄像头流建议直接使用 ISP 输出 NV12，禁止额外 `videoconvert` 到 RGB（带宽
  高）；本骨架为通用性保留了转换点，可针对 OV5695 驱动裁剪。
- 代码风格：提交前 `./scripts/format.sh`（基于 `.clang-format`），
  `./scripts/format.sh --check` 校验。
- `deployment/smart-cockpit.service` 以 root 运行以访问 KMS/DRM 与 CAN；
  生产环境可按需收紧。
