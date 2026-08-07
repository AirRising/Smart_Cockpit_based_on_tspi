# Smart Cockpit (Qt Widgets / eglfs-KMS / SocketCAN / GStreamer)

智能汽车仪表盘项目骨架，目标硬件：泰山派 RK3566（Cortex-A55, 4GB RAM），
Debian 11/12，10.1" 屏，`eglfs + KMS/DRM`，无 X11。界面完全使用 C++ + Qt
Widgets，**不使用 QML**。

## 功能模块

| 模块 | 说明 |
| --- | --- |
| 数字仪表 | 车速 0-240 km/h、转速 0-8000 rpm、油量 0-100%、转向灯（左/右/双闪）、故障灯（发动机/ABS/气囊），数据来自 CAN 0x100 |
| 中控多媒体 | GStreamer `playbin` 播放本地 MP3，进度/曲目信息，BlueZ A2DP 蓝牙输出（bluealsa） |
| 空调面板 | 温度 16-30℃、风量 0-7、吹风模式、AC/AUTO；双向 CAN 0x200，等待车辆回传 ACK，超时回滚 UI |
| 倒车影像 | R 挡（CAN 0x300 bit0）全屏显示，GStreamer appsink 拉取 NV12 流转 RGBA，QOpenGLWidget 渲染，叠加随方向盘角度变化的轨迹线 |
| CAN 通信 | SocketCAN（vcan0/can0），按 DBC 解析 0x100/0x200/0x300/0x400，内置默认信号表，也可加载 .dbc |
| 系统状态机 | Normal / Degraded / Emergency，HealthMonitor 轮询各服务，驱动顶部横幅 |
| 日志 | 异步写 `/var/log/smart-cockpit/`，Info/Warn/Error，单文件 10MB 滚动 |
| 部署 | CMake + aarch64 交叉工具链 + systemd 单元 + 一键部署脚本 |

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
│   │   ├── CanTypes.h           # CanSignalDef + 帧 ID
│   │   ├── CanFrameParser.h/.cpp# DBC 解析/编码（0x100-0x400）
│   │   ├── CanManager.h/.cpp    # SocketCAN 接收线程（阻塞 read）
│   │   ├── CarService.h/.cpp    # 车辆状态汇总 + 空调确认/回滚
│   │   └── HealthMonitor.h/.cpp # 状态机轮询
│   ├── logging/
│   │   └── AsyncLogger.h/.cpp   # 异步文件日志 + 10MB 滚动
│   ├── media/
│   │   ├── BlueZScanner.h/.cpp  # BlueZ D-Bus 设备扫描
│   │   └── MediaService.h/.cpp  # GStreamer playbin 封装
│   ├── camera/
│   │   ├── CameraService.h/.cpp # appsink NV12 -> RGBA
│   │   └── VideoWidget.h/.cpp   # QOpenGLWidget + 动态轨迹线
│   └── ui/
│       ├── MainWindow.h/.cpp    # 横幅 + QStackedWidget
│       ├── ScreenManager.h/.cpp # 优先级页面管理
│       ├── InstrumentPage.h/.cpp
│       ├── GaugeWidget.h/.cpp   # QPainter 圆形仪表
│       ├── ClimatePage.h/.cpp
│       ├── MediaPage.h/.cpp
│       └── ReverseCameraPage.h/.cpp
└── tests/
    ├── CMakeLists.txt
    ├── tst_canframeparser.cpp   # DBC 解析/编码测试
    └── tst_climateservice.cpp   # 空调 ACK/超时回滚测试
```

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
```

未指定 `--camera` 时使用 `videotestsrc` 模拟摄像头；未指定 `--can` 时默认
`vcan0`。桌面环境运行即可看到四个页面（F1/F2/F3 切换，R 帧触发倒车全屏）。

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
gstreamer1.0-plugins-good bluez bluealsa systemd`，以及
`libqt5gui5` 的 eglfs 平台插件。R 挡触发依赖 CAN 0x300 的 bit0。

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

- `tst_canframeparser`：0x100/0x300 解码、DBC 文本加载、0x200 编码回环。
- `tst_climateservice`：发送 0x200 指令 → 收到 ack 后 `climateAckOk` 且状态
  更新；超时后 `climateAckTimeout` 且 UI 回滚为旧值（测试直接注入帧，不依赖
  真实 CAN）。

## 生产部署注意事项

- eglfs 下字体：RK3566 上若无中文字体，先安装 `fonts-noto-cjk`，或将 HMI
  文案改为英文资源。
- 摄像头流建议直接使用 ISP 输出 NV12，禁止额外 `videoconvert` 到 RGB（带宽
  高）；本骨架为通用性保留了转换点，可针对 OV5695 驱动裁剪。
- A2DP 需要 `bluealsa` 服务：`systemctl enable bluealsa`，并由
  `BlueZScanner` 在设备连接时自动切换到 `bluealsa:...` sink。
- `deployment/smart-cockpit.service` 以 root 运行以访问 KMS/DRM 与 CAN；
  生产环境可按需收紧。
