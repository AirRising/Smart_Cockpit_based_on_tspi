# Smart Cockpit 开发指南

> 面向第一次接触这个项目的开发者，目标是从零讲清楚"这个车机界面是怎么做出来的"。
> 本文档描述**当前代码的真实状态**（中文化界面、温度圆表、循环模式、健壮性加固），不再按 V1/V2 拆分版本。
> 建议边看文档边打开源码对照阅读。

---

## 目录

1. 这个项目是什么
2. 运行环境与硬件
3. 前置概念（先搞懂这些）
4. 目录结构总览
5. 系统架构与数据流
6. 线程模型详解
7. 模块逐一讲解
8. 构建、测试、运行
9. 交叉编译与部署
10. 新手常见问题排查
11. 如何扩展这个项目

---

## 1. 这个项目是什么

这是一个**智能汽车仪表盘（Smart Cockpit）**的 Qt 项目，跑在车载嵌入式设备上。它做三件事：

- **给司机看车况**：车速、转速、电量、转向灯、故障灯（来自 CAN 总线）
- **给司机操作**：空调面板（按了按键要通过 CAN 发指令给车，等车确认）
- **给司机辅助**：挂 R 档时全屏显示倒车影像，并叠加随方向盘角度变化的轨迹线

关键约束（这也是整个设计的前提）：

| 约束 | 含义 |
| --- | --- |
| 硬件 | 泰山派 RK3566（ARM Cortex-A55，4GB RAM） |
| 系统 | Debian 11/12 |
| 显示 | `eglfs + KMS/DRM`，**没有 X11/Wayland**；10.1 寸 800×1280 竖屏 |
| 界面 | **纯 C++ + Qt Widgets，禁止 QML**，无图片素材，图标用 Unicode 字符、质感用 QPainter/QSS |
| 通信 | SocketCAN（`vcan0` 调试 / `can0` 实车） |
| 音视频 | GStreamer（MP3 播放 + 摄像头取流） |

---

## 2. 运行环境与硬件

### 目标设备（泰山派 RK3566）

- 4 核 Cortex-A55，4GB RAM，性能中等，**不能随便开高分辨率特效**
- 10.1 寸屏幕，用 KMS/DRM 直接驱动（`eglfs` 平台插件），绕开桌面环境
- 所以界面代码不能依赖窗口管理器，必须用 `QMainWindow` 自己管布局
- 界面全部用 Qt 布局（`QHBoxLayout`/`QVBoxLayout`/`QGridLayout`），可自适应 800×1280 竖屏与横屏窗口

### 主机调试环境（Ubuntu）

开发时没有实车、没有真摄像头、没有真蓝牙，所以项目做了三个"替身"：

| 真车上 | 开发机上 |
| --- | --- |
| CAN 总线 `can0` | 虚拟 CAN `vcan0`（`scripts/vcan-setup.sh` 创建） |
| 摄像头 OV5695 | `videotestsrc` 测试视频源（自带小球动画） |
| 蓝牙 A2DP 音箱 | 随便一个 ALSA 声卡 |

这就是"主机能跑、真机也能跑"的调试思路：**所有外设都被抽象成"服务"，主机上找不到就自动降级**。

---

## 3. 前置概念（先搞懂这些）

### 3.1 Qt 的信号与槽（Signal & Slot）

Qt 里对象间通信的主要方式。类似"广播+订阅"：

```cpp
// 发信号的一方（CarService）：
signals:
    void speedChanged(double kmh);   // 声明一个信号，注意是"声明"，不用写实现

// 发的时候：
emit speedChanged(120.0);            // 广播"车速变了"

// 收信号的一方（InstrumentPage）：
connect(car, &CarService::speedChanged, this, &InstrumentPage::onSpeedChanged);
```

好处：**收发双方互相不认识**，CarService 不知道仪表页存在，仪表页只关心"有信号来了我更新 UI"。

### 3.2 Qt 的线程与跨线程通信

`QThread` 是 Qt 的线程类。难点在于：**QThread 里的代码和主线程的代码不能直接共享数据**（会数据竞争）。

Qt 的标准做法是：线程里 `emit` 一个信号，另一个线程用 `connect(..., Qt::QueuedConnection)` 来接。`QueuedConnection` 会把信号"打包邮寄"到接收者所在线程的事件队列，由接收线程自己取出来执行——**自动完成线程安全切换**。

本项目核心代码里有一行（`CarService.cpp` 构造函数）：

```cpp
connect(m_can, &CanManager::frameReceived,
        this,   &CarService::onCanFrame, Qt::QueuedConnection);
```

`CanManager` 的接收线程 `emit frameReceived(...)`，主线程的 `CarService::onCanFrame` 被调用。**两边的代码在不同线程跑，但数据不会乱**。

### 3.3 SocketCAN 与 CAN 帧

CAN 是汽车的总线协议，`SocketCAN` 是 Linux 内核把 CAN 接口做成"类 socket"的模块。项目用 `socket(PF_CAN, SOCK_RAW, CAN_RAW)` 打开原始套接字，然后 `read()` 一帧一帧读。

一帧 CAN 数据包核心是：
- `can_id`：帧 ID（比如 0x100、0x200），用来区分"这是什么数据"
- `data[8]`：最多 8 个字节的数据，**具体含义要靠 DBC 文件解释**

### 3.4 DBC 与信号解析

CAN 报文是"裸字节"，车厂会提供 DBC 文件说明**每一位代表什么**。比如：

```
SG_ speed : 0|16@1+ (1,0) [0|240] "km/h" ICU
```

意思是："speed 这个信号，从第 0 位开始占 16 位，小端（`@1`）无符号（`+`），物理值 = 原始值 × 1 + 0，范围 0~240 km/h"。

所以**同样 8 个字节，用不同 DBC 解释，读出的值完全不同**。项目里 `CanFrameParser` 就是干这个活的（详见 [7.3](#73-canframeparser--dbc-解析器)）。

### 3.5 GStreamer

一个媒体处理框架。播放 MP3 用它的 `playbin`（一条"全自动管线"），拿摄像头画面用 `appsink`（把画面数据"吸"到自己的程序里）。项目里 GStreamer 只作为 C 库被调用，不涉及自己的 pipeline 脚本。

### 3.6 eglfs / KMS

嵌入式 Linux 显示方案：
- **KMS**：Linux 内核的显示输出接口，直接控制屏幕
- **eglfs**：Qt 的显示后端，通过 EGL 直接在 KMS 上画，**没有 X11**

对你写代码的影响只有一个：程序启动时通过环境变量 `QT_QPA_PLATFORM=eglfs` 指定，业务代码不用关心。

### 3.7 强类型枚举（enum class）

`enum class` 不能直接当整数用，所以 `SystemState.h` 文件末尾给 `PageId`、`PagePriority` 补了 `qHash`，否则 `QHash<PageId,...>` 在 Qt5 下编不过。

---

## 4. 目录结构总览

```
tspi/
├── CMakeLists.txt                     # 构建入口（CMake）
├── CMakePresets.json                  # 预设构建配置（Debug/Release/Cross）
├── config/
│   └── vehicle.dbc                    # 示例 DBC 文件（可用 --dbc 加载覆盖内置表）
├── cmake/
│   └── toolchain-aarch64-linux-gnu.cmake  # 交叉编译工具链文件
├── deployment/
│   ├── smart-cockpit.service          # systemd 开机自启单元
│   ├── kms-config.json                # KMS 显示配置
│   └── deploy.sh                      # 一键部署脚本
├── scripts/
│   ├── vcan-setup.sh                  # 主机创建虚拟 CAN
│   └── demo-can.sh                    # 向 vcan0 发模拟帧
├── src/
│   ├── main.cpp                       # 程序入口，组装一切
│   ├── core/                          # 与界面无关的"业务核心"
│   │   ├── CanTypes.h                 # 帧 ID、信号结构体定义
│   │   ├── SystemState.h              # 枚举、空调状态结构体
│   │   ├── CanFrameParser.h/.cpp      # DBC 解析/编码
│   │   ├── CanManager.h/.cpp          # CAN 接收线程
│   │   ├── CarService.h/.cpp          # 车辆数据中枢
│   │   └── HealthMonitor.h/.cpp       # 系统状态机
│   ├── logging/
│   │   └── AsyncLogger.h/.cpp         # 异步日志
│   ├── media/
│   │   ├── MediaService.h/.cpp        # GStreamer MP3 播放
│   │   └── BlueZScanner.h/.cpp        # 蓝牙设备扫描（当前未在 main 中接线）
│   ├── camera/
│   │   ├── CameraService.h/.cpp       # GStreamer 摄像头取流 + NV12→RGBA
│   │   └── VideoWidget.h/.cpp         # QOpenGLWidget 画面 + 轨迹线
│   ├── ui/
│   │   ├── MainWindow.h/.cpp          # 主窗口：顶栏 + 页面栈 + 导航 + 状态条
│   │   ├── ScreenManager.h/.cpp       # 页面优先级管理
│   │   ├── Theme.h/.cpp               # 全局 QSS 主题
│   │   ├── InstrumentPage.h/.cpp      # 数字仪表页
│   │   ├── GaugeWidget.h/.cpp         # QPainter 圆形仪表
│   │   ├── ClimatePage.h/.cpp         # 空调面板（含自绘温度圆表）
│   │   ├── MediaPage.h/.cpp           # 多媒体页
│   │   └── ReverseCameraPage.h/.cpp   # 倒车影像页
└── tests/
    ├── CMakeLists.txt
    ├── tst_canframeparser.cpp         # DBC 解析/截断帧测试
    └── tst_climateservice.cpp         # 空调 ack/回滚/信号守护测试
```

**分层思想**：

```
        ┌────────── ui/（只做显示和交互，不认识硬件）
        │    MainWindow, 4 个页面, 仪表控件, 全局主题
        ├────────── media/ camera/（外设封装，暴露信号/方法）
        │    GStreamer 播放、摄像头、蓝牙
        ├────────── core/（业务中枢，不认识 UI）
        │    CanFrameParser, CanManager, CarService, HealthMonitor
        └────────── logging/（工具，谁都能用）
             AsyncLogger
```

方向依赖：**ui 依赖 core/media/camera，core 不依赖 ui**。这样单元测试可以不启动界面直接测 core。

---

## 5. 系统架构与数据流

```
                 ┌─────────────────────────────────────────────┐
                 │                 主线程（GUI）                 │
                 │                                               │
  CAN 接收线程    │  ┌──────────┐  信号   ┌────────────┐          │
 ┌───────────┐   │  │ CarService│◄────────│ CanManager │  ← 阻塞读 vcan0
 │  (QThread) │   │  └────┬─────┘         └────────────┘          │
 └───────────┘   │       │ 信号（speedChanged...）                │
                 │       ▼                                       │
                 │  ┌───────────────┐  requestPage(Urgent)      │
                 │  │ MainWindow     │─────────────────────┐    │
                 │  │ 顶栏+QStackedWidget                 ▼    │
                 │  └───────────────┘             ┌────────────┐ │
                 │        ▲                       │ScreenManager│ │
                 │        │ 页面切换               └────────────┘ │
                 │  ┌─────┴─────┬─────────┬──────────┐           │
                 │  │Instrument │ Media   │ Climate  │ Reverse  │
                 │  └───────────┴─────────┴──────────┘  Camera  │
                 │                                               │
                 │  摄像头 GStreamer streaming 线程 ──信号──► VideoWidget
                 └─────────────────────────────────────────────┘
```

两条主要数据流：

### 数据流 A：上行（车 → 界面）—— 仪表/倒车触发

```
vcan0 收到 0x100 帧
  → CanManager 接收线程 emit frameReceived(0x100, bytes)
  → (QueuedConnection) CarService::onCanFrame
  → CanFrameParser 按 DBC 解出 speed/rpm/...
  → emit speedChanged(120.0) / reverseChanged(true) ...
  → InstrumentPage 更新仪表 / MainWindow 请求倒车页
```

### 数据流 B：下行（界面 → 车）—— 空调指令

```
用户在空调页拖温度滑杆
  → ClimatePage::onControlChanged
  → CarService::sendClimateCommand(target)  → 组帧 → CanManager::sendFrame 写 vcan0
  → 车（或模拟程序）回发 0x200 帧，ack 位置 1
  → 回到上行通道 → CarService::handleFrame200 确认 → emit climateAckOk
  → 空调页解锁；若超时 → climateAckTimeout → 页面回滚
```

理解这两条流之后，整个项目就等于懂了 80%。下面逐模块看代码。

---

## 6. 线程模型详解

### 6.1 项目里有几个线程？

| 线程 | 由谁创建 | 干什么 | 生命周期 |
| --- | --- | --- | --- |
| 主线程 | `main()` | GUI 渲染、状态维护、信号槽 | 程序启动到退出 |
| CAN 接收线程 | `CanManager`（QThread 子类） | 阻塞 `select`+`read` 收 CAN 帧 | `open()` 启动，`closeBus()` 结束 |
| 摄像头 streaming 线程 | GStreamer 内部 | 视频帧回调、NV12→RGBA 转换 | `CameraService::start()` 期间 |
| 日志写盘线程 | `AsyncLogger`（std::thread） | 从队列取日志写文件 | `start()` 启动，`stop()` 结束 |

### 6.2 线程间通信方式

**规则：跨线程只走信号槽（QueuedConnection），不共享成员变量。**

- CAN 线程 → 主线程：`CanManager::frameReceived` → `CarService::onCanFrame`，**QueuedConnection**（`CarService.cpp` 构造函数）
- 摄像头线程 → 主线程：`CameraService::frameReady(QImage)` → `VideoWidget::setFrame`（`frameReady` 是队列信号 + QImage 已 `qRegisterMetaType`，跨线程自动用队列投递）
- 日志线程：通过 `QMutex + QWaitCondition + QQueue` 的**生产者-消费者队列**，不涉及 Qt 信号

### 6.3 为什么要"阻塞 read"而不是定时器轮询？

CAN 帧到达是"事件驱动"的，用阻塞 `select()` 能立刻发现新帧、还不占 CPU。项目在 `CanManager::run()` 里做了超时 100ms 的 `select`，目的是**既能及时收帧，又能定期检查退出请求**（否则 `stop()` 永远等不到线程结束）。

### 6.4 QImage 跨线程传递为什么安全？

`qRegisterMetaType<QImage>("QImage")`（`main.cpp`）告诉 Qt："QImage 可以在队列信号里当参数传"。传的时候走的是**隐式共享 + 引用计数**：发送线程和接收线程只是共享同一份像素数据，谁都不会去改它，所以安全。（`VideoWidget::setFrame` 里还是用 `QMutex` 再保护了一层，见 [7.14](#714-cameraservice--videowidget--reversecamerapage--倒车影像)。）

---

## 7. 模块逐一讲解

### 7.1 main.cpp —— 程序组装车间

`main()` 做的事就三件：**解析命令行 → 创建服务 → 连接起来跑起来**。

```cpp
// 创建"三驾马车"：车况、多媒体、摄像头
sc::CarService car;
sc::MediaService media;
sc::CameraService camera;

// 状态机监控这三者健康
sc::HealthMonitor health(&car, &camera, &media);

// 主窗口把这些服务喂给各页面
sc::MainWindow window(&car, &media, &camera);

// 状态机变化 → 主窗口状态胶囊
QObject::connect(&health, &HealthMonitor::stateChanged,
                 &window, &MainWindow::onSystemStateChanged);

// 启动顺序：先显示，再开外设
window.showFullScreen();
media.init();
car.start(parser.value(canOption));   // 默认 "vcan0"
camera.start(parser.value(cameraOption));
health.start();

// 事件循环：Qt 程序到这里就"卡住"，全靠信号槽被驱动
return app.exec();
```

命令行参数（用 Qt 自带的 `QCommandLineParser`）：

| 参数 | 作用 |
| --- | --- |
| `--can vcan0` | CAN 接口名 |
| `--camera ""` | 摄像头设备路径，空 = 测试源 |
| `--dbc file` | 用 DBC 文件覆盖内置信号表 |
| `--log-dir dir` | 日志目录 |
| `--media-dir dir` | 音乐目录 |

### 7.2 数据结构 —— CanTypes.h / SystemState.h

**CanTypes.h**：定义一帧里每个信号的"解释方式"。

```cpp
struct CanSignalDef {
    QString name;      // 信号名，如 "speed"
    int startBit;      // 起始位
    int bitLength;     // 占用位数
    bool bigEndian;    // true=大端(Motorola) false=小端(Intel)
    bool isSigned;
    double factor;     // 换算系数
    double offset;     // 偏移量
    double minVal, maxVal;
    QString unit;
};

namespace FrameId {   // 帧 ID 常量
    constexpr quint32 ICU_Dynamic = 0x100;  // 仪表
    constexpr quint32 HVAC        = 0x200;  // 空调
    constexpr quint32 Gear        = 0x300;  // 挡位
    constexpr quint32 Chassis     = 0x400;  // 方向盘角度
}
```

**SystemState.h**：定义三个枚举和一个结构体，是"全局共用词汇"。

```cpp
enum class SystemState { Normal, Degraded, Emergency };   // 状态机三态
enum class PageId { Instrument, Media, Climate, ReverseCamera };  // 四个页面
enum class PagePriority { Low, Normal, High, Urgent };   // 页面抢占优先级
struct ClimateState { int temperature; int fanSpeed; int blowMode;
                      bool acOn; bool autoMode; };        // 空调状态
```

### 7.3 CanFrameParser —— DBC 解析器

**职责**：把 8 字节 CAN 数据 ↔ 一组"信号名→数值"。纯算法类，不依赖 Qt 线程/UI，所以最好写单元测试（见 [8.3](#83-单元测试)）。

内置信号表在 `installBuiltinTable()`，覆盖 0x100（速度/转速/电量/转向灯/故障灯）、0x200（空调，含 ack 位）、0x400（方向盘角度）。0x300 挡位走裸字节解析（见 [7.5](#75-carservice--车辆数据中枢)）。

**位序（Bit Order）是本项目最容易懵的点**，单独解释：

CAN 信号有两种排布方式（`@1`=小端 Intel，`@0`=大端 Motorola）：

- **Intel 小端**：`startBit` 是信号的最低有效位，位序号从左到右数。`speed` 在 `0|16@1+`：位 0~7 是第 0 字节，位 8~15 是第 1 字节，直接按小端拼起来。
- **Motorola 大端**：`startBit` 指向最高有效位，且字节内位序是反的（第 7 位在最左）。解码时要从最高位往最低位移。

看 `decodeSignal()` 里两套循环就明白了。`demo-can.sh` 里发 0x100 帧时注释也写了字节排布，可对照验证。

**物理值换算**：`物理值 = 原始值 × factor + offset`。比如 `fuel_level` 的 factor 是 0.5，所以原始字节 `100` 表示 `50%`。

**DBC 文件加载**：`loadDbcText()` 逐行解析 `BO_`（帧定义）和 `SG_`（信号定义），会**覆盖**同帧 ID 的内置表。这就是"拿到真实 DBC 文件就能替换默认信号表"的实现。

**健壮性（全有或全无）**：`decode()` 会先校验数据长度能否覆盖该帧的所有信号，任一信号所需字节超界就**整帧拒绝**返回 false，而不是把解不出的信号静默当 0。这样调用方（CarService）能保留"最后有效状态"，避免显示半真半假的数据。

### 7.4 CanManager —— CAN 接收线程

继承 `QThread`，`run()` 是线程入口。

```cpp
bool CanManager::open(const QString &iface) {
    m_socket = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);   // 建原始 socket
    ::ioctl(m_socket, SIOCGIFINDEX, &ifr);            // 找接口索引
    ::bind(m_socket, ...);                            // 绑定到 vcan0/can0
    start();                                          // 启动接收线程
    return true;
}
```

接收循环（`run()`）要点：

```cpp
while (!isInterruptionRequested()) {
    ::select(m_socket + 1, &readFds, ...);   // 阻塞等待，100ms 超时
    if (ready > 0 && FD_ISSET(m_socket, &readFds)) {
        ::read(m_socket, &frame, sizeof(frame));  // 读一帧
        emit frameReceived(id, data, extended);   // 交给主线程
    }
}
```

**发送**也在这里，`sendFrame()` 用 `m_sendMutex` 保护（主线程可能在收帧的同时发帧），组装 `struct can_frame` 后 `write()`。

### 7.5 CarService —— 车辆数据中枢

**全项目最核心的类**，把 CAN 原始帧变成"语义化信号"。它只住在主线程，所有状态都被信号槽保护。

按帧 ID 分发（`onCanFrame`）：

```cpp
switch (id) {
case FrameId::ICU_Dynamic: handleFrame100(data); break;  // 仪表
case FrameId::HVAC:        handleFrame200(data); break;  // 空调
case FrameId::Gear:        handleFrame300(data); break;  // 挡位
case FrameId::Chassis:     handleFrame400(data); break;  // 方向盘角度
}
```

- `handleFrame100`：解出 speed/rpm/fuel/转向灯/故障灯，**值没变就不发信号**（避免 UI 无谓刷新）。
- `handleFrame300`：`byte0 bit0 = R 挡`，变了就 `emit reverseChanged(true)`——这是触发倒车全屏的源头。挡位与倒车直接走裸字节解析。
- `handleFrame400`：方向盘角度（`0|16@1-` 有符号 ×0.1），喂给轨迹线。
- `handleFrame200`：空调确认逻辑（重点，见 [7.12](#712-climatepage--空调面板重点看)）。

**健壮性（信号守护）**：0x100 / 0x200 / 0x400 在应用数据前，会用 `hasAllSignals()` 校验解析出的键是否齐全。只要缺一个（例如用真实 DBC 覆盖后字段名不一致，或帧不完整），就**整帧跳过、保留最后已知有效值**，绝不显示残缺数据。这是"拿到真实 DBC 就能用"这个目标的兜底——字段名对不上时，宁可保持旧值也不显示错误值。

**空调指令下行**（`sendClimateCommand`）：

```cpp
quint32 CarService::sendClimateCommand(const ClimateState &target) {
    // 1. 把 UI 值组帧
    // 2. 通过 m_can->sendFrame(0x200, payload) 发出去
    // 3. 记下"我发了，在等回执"
    m_pendingClimate = true;
    m_ackTimer.start();          // 600ms 倒计时
    return requestId;
}
```

### 7.6 HealthMonitor —— 系统状态机

每 2 秒 `poll()` 一次，轮询三个服务，输出 `Normal / Degraded / Emergency`：

```
CAN 最近 3 秒内没有新帧（或从未连上）→ Emergency
摄像头 / 媒体异常                    → Degraded
都正常                              → Normal
```

判定 CAN 存活只看"连接状态 + 最近帧时间戳新鲜度"（`canConnected() && now - lastFrameTimeMs < 3000`），**不要求轮询间隔内帧计数必须增长**——否则静止停放或事件型总线会误报红色告警。

`Emergency` 会让主窗口顶栏的状态胶囊变红、显示"CAN 丢失"。这就是状态机驱动 UI 的实现——**UI 不主动查状态，等状态机发信号**。

### 7.7 ScreenManager —— 页面优先级管理

`QStackedWidget` 管的是"叠了一摞页面，显示第几个"，`ScreenManager` 在此基础上加了**优先级抢占**。

核心是 `requestPage(id, priority)`：

```cpp
bool ScreenManager::requestPage(PageId id, PagePriority priority) {
    PagePriority currentPrio = m_activePriorities.value(m_current, Low);
    if (priority < currentPrio)
        return false;   // 被更紧急的页面挡住了（比如正在倒车）
    m_previous = m_current;
    m_activePriorities.insert(id, priority);
    switchTo(id);
    return true;
}
```

倒车触发全屏就是这么写的（`MainWindow::onReverseChanged`）：

```cpp
void MainWindow::onReverseChanged(bool active) {
    if (active)
        m_screen->requestPage(ReverseCamera, Urgent);  // 最高优先级，必然抢屏
    else
        m_screen->dismissPage(ReverseCamera);          // 退出倒车，回到原来页面
}
```

### 7.8 Theme —— 一个 QSS 管全界面

整套配色和控件样式收敛成一个全局样式表，`main.cpp` 启动时应用一次：

```cpp
QApplication::setStyle(QStringLiteral("Fusion"));
app.setStyleSheet(sc::Theme::styleSheet());   // 全局主题
```

- `Theme.h`：调色板常量（`Accent` 青色、`Amber` 琥珀、`Danger` 红色……）+ `styleSheet()`
- `Theme.cpp`：`styleSheet()` 返回一个很大的 QSS 字符串（raw string，用 `R"qss(...)qss"` 包起来避免转义地狱）

QSS 和 CSS 很像，区别是"选择器"靠控件设置 `setObjectName()` 来匹配：

```
QLabel#brandName { ... }       // 匹配 objectName == "brandName" 的 QLabel
QFrame#card { ... }            // 卡片
QPushButton#mode:checked       // 分段按钮的"选中"伪状态
```

主窗口根 widget `objectName = "appRoot"`，QSS 给它画一整屏深蓝渐变；各页面设置 `background: transparent` 透出渐变。全局 QSS 里 `* { color: #EAF0F8; }` 统一了默认文字色，控件只管排版。中文字体回退（Noto Sans CJK SC / WenQuanYi）也已列入 QSS。

### 7.9 MainWindow —— 顶栏 + 页面栈 + 底部导航

```
┌──────────────────────────────────────────────┐
│ 顶栏 topBar（60px）：SC 智能座舱｜页面标题 …… CAN｜状态｜时钟 │
├──────────────────────────────────────────────┤
│           QStackedWidget（页面区，弹性占满）     │
├──────────────────────────────────────────────┤
│ 底部导航 navBar（64px）：仪表 │ 媒体 │ 空调      │
├──────────────────────────────────────────────┤
│ 状态条 statusStrip（一行小字，显示诊断摘要）      │
└──────────────────────────────────────────────┘
```

- **顶栏**：品牌 Logo（`SC`）+ 品牌名"智能座舱" + 副标"v1.0 · 车载HMI" + 页面标题（仪表盘/多媒体/空调控制/倒车影像）+ CAN 状态胶囊（`●`=已连 / `○`=未连）+ 系统状态胶囊（绿"系统正常"/橙"系统降级"/红"CAN 丢失"）+ 时钟日期。时钟由 `QTimer` 每秒驱动。
- **底部导航**：三个 `QToolButton` 放进互斥 `QButtonGroup`，文字是 Unicode 图标 + 中文标签（`◉ 仪表`、`▶ 媒体`、`❄ 空调`）。导航选中态和 `ScreenManager` 双向同步（`pageChanged` 信号 → `setChecked(true)` 并更新标题），无论快捷键、导航还是倒车抢占切页，选中态都一致。
- **状态条**：很矮的 `QLabel`，`HealthMonitor::diagnosticsChanged` 把 "CAN:OK CAM:OK MEDIA:OK frames:N" 摘要写进来。
- **快捷键**：F1/F2/F3 切页（调试用，生产可删）。

### 7.10 GaugeWidget —— QPainter 圆形仪表

**QPainter 是 Qt 的 2D 绘图 API**，用在 `paintEvent()` 里画一切。理解这块的关键是**角度与坐标换算**。

仪表盘是"270° 的扇形，从左下角扫到右下角"，代码约定：

```
最小刻度 = 225°  →  最大刻度 = -45°
角度 = 225° - 值占比 × 270°
```

`paintEvent` 依次画：外圈 rim（径向渐变）→ 内盘 face → 轨道 arc → 红区 redline → 值弧（先半透明粗弧"发光"，再实心弧）→ 刻度线 → 数字 → 锥形指针（线性渐变 + 中心轮毂）→ 读数（标签/大数字/单位）。

值弧跨度：`span = (valueAngleDeg() - 225) * 16`（`drawArc` 角度单位是 1/16 度）。值越大终点越接近 -45°。电量 15% 以下是红区，所以电量表超低也会亮红弧。

指针用三角函数算终点（注意 Qt 坐标系 y 轴向下，所以 y 方向取 `-qSin`）：

```cpp
angleRad = qDegreesToRadians(valueAngleDeg());
painter.drawLine(center,
                 center + QPointF(qCos(angleRad) * len,
                                  -qSin(angleRad) * len));
```

**刻度数字**：字号 = `max(9, int(r * 0.16) - 2)`（比中央读数小两号），随表盘半径缩放。绘制时先用 `QFontMetricsF::boundingRect(text)` 算出文本真实宽高，再 `moveCenter` 到刻度位置，**任何尺寸都不会被裁剪**（早期版本用固定 48×20 矩形，大表下数字被裁掉左右边缘）。读数、标签、单位字号也都按 `r` 比例缩放，180px 小表和填满屏幕的大表永远成比例。

`setValue()` 会 `update()` 触发重绘。**仪表本身只负责"画"，数据由 InstrumentPage 喂**。

### 7.11 InstrumentPage —— 数字仪表页

纯"视图"类：构造函数里把 `CarService` 的每个信号连到自己的槽，每个槽只干一件事——更新对应控件。

```
┌──────────────┐  ┌──────────────┐  ┌──────────────────┐
│  车速         │  │  电机转速     │  │  车辆状态         │
│   (大仪表)     │  │   (大仪表)     │  │  ┌────┐ ┌────┐  │
│              │  │              │  │  │ 挡位 │ │ 电量│  │
│              │  │              │  │  │  P  │ │ (表) │  │
│              │  │              │  │  └────┘ └────┘  │
│              │  │              │  │  ◀  ▶  ▲  电机 ABS 气囊 │
└──────────────┘  └──────────────┘  └──────────────────┘
     权重 3            权重 3               权重 2
```

三个仪表：车速（0~240，红区 200）、电机转速（0~8000，红区 6000）、电量（0~100%，红区 15）。挡位是一个 84px 圆形 `QLabel`（琥珀描边 + P/R/N/D）。六个指示灯（左转/右转/双闪/电机/ABS/气囊）是 `QLabel` 圆角胶囊，点亮/熄灭就是切换两套 QSS 背景色。转向灯逻辑：`left || hazard`、`right || hazard` 同时点亮左右。

> 竖屏适配提示：本页按横屏"三卡并排"设计，在 800×1280 竖屏下三表会被压缩到约 200px、上下留白较多。属于已知取舍，可按需改成上下叠放。

### 7.12 ClimatePage —— 空调面板（重点看）

这一页演示了"**UI → 发指令 → 等车确认 → 没确认就回滚**"的完整闭环，还包含一个自绘温度圆表。

```
┌────────────────────────┐  ┌───────────────┐
│ 温度                    │  │ 吹风模式       │
│     ╭─────────╮         │  │ 面部  脚部    │
│     │   22°C  │  圆表   │  │ 除霜 面部+脚部 │
│     ╰─────────╯         │  ├───────────────┤
│    [−]         [+]      │  │ 循环模式       │
│     ─────●─────          │  │ 内循环  外循环 │
├────────────────────────┤  └───────────────┘
│ 风速                    │
│   4  ████████░░░░        │  ← QProgressBar（琥珀渐变，0~7）
│   ────●─────             │
└────────────────────────┘
AC 制冷  自动          就绪   [取消待处理]
```

左列（温度:风速 = 权重 3:1）占大头，右列是吹风模式 + 循环模式两张卡。

**温度圆表（TempDialWidget）**：温度主显示是一个用 QPainter **自绘的圆形表盘**，内联定义在 `ClimatePage.cpp` 的匿名命名空间（不新增源文件，所以 `CMakeLists.txt` 不用改）：
- 270° 弧形轨道，颜色随设定温度从**蓝（16°C）渐变到橙红（30°C）**，当前值画一段高亮弧，底部标注 16 / 30
- 中心大号温度数字 + `°C`
- 表盘始终是**居中正方形**（按 `min(width, height)` 计算），`sizeHint()` 返回 400×400，竖屏/横屏/小窗都不会被裁切
- `−`/`+` 按钮本质是去拨滑杆，滑杆 `valueChanged` 同时更新表盘并触发 `onControlChanged`：

```cpp
connect(m_tempSlider, &QSlider::valueChanged, tempDial, &TempDialWidget::setTemperature);
```

> 坑：给控件加了 `Qt::AlignCenter` 对齐标志后，控件会**停止拉伸、卡在最小尺寸**。要"居中但可长大"的控件，应该提供合理的 `sizeHint()` 而不是用对齐标志。

**循环模式**：右列新增的卡片，`内循环 / 外循环` 两个分段按钮放进互斥 `QButtonGroup`，默认内循环。这是**纯 UI 层的本地状态**（`ClimateState` 结构体没有该字段），不参与 ACK/回滚链路。

**用户操作时**（`onControlChanged`）：

```cpp
if (m_syncing || m_pending) return;      // 正在同步/等待中，忽略再次操作
m_lastSent = readControls();             // 读当前控件值
m_car->sendClimateCommand(m_lastSent);   // 组帧发 CAN
m_pending = true;                        // 锁定面板
setPanelEnabled(false, "等待车辆确认...");
```

**车回执时**（`onAckOk`）：解锁面板，用**车确认过的值**（`m_car->climate()`）刷新控件。

**超时回滚时**（`onAckTimeout`）：同样解锁，但控件恢复成**之前车确认的旧值**——用户改的那个值被"吞掉"了，这就是回滚。

这里有个精妙的小细节 `m_syncing`：`applyControls()` 用代码设置控件值时，会触发控件的 `valueChanged` 信号，导致 `onControlChanged` 被误调用、又发一条指令。所以设置控件前先把 `m_syncing=true`，让 `onControlChanged` 直接 return。**这是"程序改控件 vs 用户改控件"的经典防抖写法**。

### 7.13 MediaPage + MediaService —— 多媒体

```
┌─────────────────────┐  ┌───────────────────┐
│ 音乐库              │  │ 正在播放          │
│  song1.mp3          │  │   (专辑占位块 ♪)   │
│  song2.flac         │  │   曲名             │
│  ...                │  │  ──●──── 00:12/03:45│
│                     │  │   ▶  ❚❚  ■        │
└─────────────────────┘  └───────────────────┘
```

**MediaService** 是 GStreamer 的 Qt 包装：
- `init()`：`gst_init` + `gst_element_factory_make("playbin")`
- `openFile(path)`：设 `uri`、设 `audio-sink`，然后 `set_state(PLAYING)`
- `pollBus()`：200ms 定时器从 GStreamer bus 上取消息（EOS/错误/标签/状态变化），并查询进度/时长

**不用 GLib 主循环**：GStreamer 默认要在自己的 GLib 事件循环里跑，但项目是 Qt 程序，所以用一个 `QTimer` 定时 `gst_bus_pop_filtered()` 手动取消息，把 GLib 从 Qt 里"请出去"，两个事件循环不会打架。

**MediaPage** 是文件列表（音乐库）+ 播放器卡（正在播放）：专辑占位块 + 曲名 + 进度滑杆 + 时间 + 圆形控制键（`▶` 青色主键 44px、`❚❚`、`■` 40px 次级键，全用 Unicode 字符）。播放目录来自环境变量 `SMART_COCKPIT_MEDIA_DIR` 或系统音乐目录。

**BlueZScanner**：通过 D-Bus 连 BlueZ（Linux 蓝牙栈），用 ObjectManager 列出设备，设备连上时发 `deviceConnected` 信号，然后 `MediaService::setBluetoothDevice()` 把音频输出切到 `bluealsa` 的 A2DP 设备。**注意：当前 `main.cpp` 尚未实例化 `BlueZScanner`，蓝牙链路是"编译了但没接线"的预留功能**。

### 7.14 CameraService + VideoWidget + ReverseCameraPage —— 倒车影像

**CameraService**：GStreamer `appsink` 是"把视频流引到你的程序里"的元件。回调 `onNewSampleCb` 跑在 **GStreamer 的 streaming 线程**上，这也是项目第二个线程交互点。

管线分两种：

```cpp
// 无设备（主机调试）：测试源
"videotestsrc is-live=true pattern=ball ! video/x-raw,format=NV12,width=1280,height=720 ! appsink"
// 有设备（真机）：V4L2 摄像头
"v4l2src device=/dev/video0 io-mode=4 ! video/x-raw,width=1280,height=720 ! videoconvert ! video/x-raw,format=NV12 ! appsink"
```

**NV12 → RGBA 转换**在 `handleSample()`：NV12 是一块 Y 平面 + 一块 UV 交错平面，转换公式是标准 BT.601 有限范围。转换完的 `QImage` 通过 `emit frameReady(image)` 发到主线程（QImage 已注册元类型，可安全跨线程）。为降低延迟，appsink 设置了 `max-buffers=2`、`drop=true`、`sync=false`。

> 说明：摄像头在 `main.cpp` 里随开机无条件 `start()`，倒车时才切到倒车页显示画面。保持常开换来了"挂 R 档立刻出画面"的可靠性，代价是持续占用少量 CPU/功耗——这是有意的取舍。

**VideoWidget**：继承 `QOpenGLWidget`，但**没用任何 OpenGL 着色器**——它只是借用 OpenGL 的纹理上传把 `QImage` 高效画到屏幕上，再用 `QPainter` 在 GL 表面上面叠加轨迹线。要点：
1. **画面缩放**（`paintGL`）：`drawImage` 到按宽高比算出的目标矩形，保持画面不变形。
2. **线程安全**：`setFrame` 可能从任意时刻被调用，存取 `m_frame` 都用 `QMutex` 保护。
3. **轨迹线**（`drawTrajectory`）：根据方向盘角度算两条贝塞尔曲线，`shift = steerNorm * w * 0.18`（`steerNorm = 角度/450`），打得越狠偏移越大——模拟"车要往哪拐"。

**ReverseCameraPage**：最"薄"的一个页面，只把 `frameReady` 连到 `VideoWidget::setFrame`、把 `steeringAngleChanged` 连到 `setSteeringAngle`，再叠一个红色 R 徽章 + 半透明字幕条"倒车影像"。**全屏抢占逻辑在 ScreenManager，页面不碰**。

### 7.15 AsyncLogger —— 异步日志

`qInfo()`/`qWarning()` 会被 `installQtMessageHandler()` 重定向进 `AsyncLogger`，**不直接写盘**，而是丢进一个队列：

```
其他线程 → log() 加锁入队 + wakeOne → 写盘线程被唤醒 → 批量写出 → 滚动
```

好处：**UI 线程永远不会因为写磁盘而卡顿**。`rotateIfNeeded()` 在文件超过 10MB 时改名成 `.1` 并新建。

---

## 8. 构建、测试、运行

### 8.1 安装依赖（Ubuntu）

```bash
sudo apt install build-essential cmake qtbase5-dev qtbase5-dev-tools \
     libqt5dbus5 libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
     gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
     gstreamer1.0-plugins-ugly gstreamer1.0-libav \
     libgstreamer-plugins-base1.0-dev libqt5test5 can-utils
```

### 8.2 构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

也可以直接用预设：

```bash
cmake --preset Debug && cmake --build --preset Debug
```

（`CMakePresets.json` 提供 Debug / Release / Cross 三套配置。）

### 8.3 单元测试

```bash
ctest --test-dir build --output-on-failure
```

两个测试可执行文件：

- **tst_canframeparser**：手搓 8 字节数据，验证解码出的 speed/rpm/fuel 对不对；再加载一段 DBC 文本验证覆盖逻辑；编码→解码回环；以及**截断帧被整帧拒绝**（数据过短时不静默解出 0）。
- **tst_climateservice**：直接调 `CarService::onCanFrame` 注入带 ack 的 0x200 帧，验证 `climateAckOk` 触发、状态更新；不注入则等 600ms 超时验证回滚；以及 **DBC 覆盖缺字段时保持最后有效状态、不产生错误信号**。

> 注意 Qt 5.15 的 `QSignalSpy::wait()` 只对"wait 期间新发生的信号"返回 true（信号早已发生会返回 false），所以对同步注入的测试要直接断言 `spy.count()`，别依赖 `wait()`。

### 8.4 主机运行

```bash
# 创建虚拟 CAN
./scripts/vcan-setup.sh              # 需要 sudo
# 另一个终端：持续发模拟帧
./scripts/demo-can.sh

# 运行（默认 vcan0，摄像头用测试源）
./build/smart-cockpit --can vcan0
```

效果：`demo-can.sh` 发的 0x100 帧会让仪表显示 120km/h / 3000rpm / 50% 电量 + 左转向灯亮；0x300 帧会让屏幕切到"倒车全屏"（测试小球画面 + 轨迹线）；按 F1/F2/F3 切换页面。

> 离屏截图验证 UI：用 `QT_QPA_PLATFORM=offscreen` 跑一个临时程序，创建服务 + 页面后 `widget->grab()` 存 PNG（本仓库不包含此工具，属调试技巧）。

---

## 9. 交叉编译与部署

### 9.1 准备

- 把泰山派的 Debian rootfs 放到 `$SDK_SYSROOT`（内含 `usr/lib/aarch64-linux-gnu/pkgconfig` 等）
- 安装 `g++-aarch64-linux-gnu`
- 可选：Qt 不在 sysroot 里时，设 `$SDK_QT_ROOT`

### 9.2 交叉编译

```bash
export SDK_SYSROOT=/opt/taisanpi/sysroot
export SDK_QT_ROOT=/opt/taisanpi/qt5      # 可选
cmake --preset Cross
cmake --build --preset Cross -j$(nproc)
```

等价命令：`cmake -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-linux-gnu.cmake -DSMART_COCKPIT_USE_QT6=OFF`。

工具链文件的关键设置：`CMAKE_SYSROOT` 指向 rootfs，`CMAKE_FIND_ROOT_PATH_MODE_*` 设为 `ONLY`（只从 sysroot 找依赖，不会混入主机库），`PKG_CONFIG_LIBDIR` 指向 aarch64 的 pkgconfig。

### 9.3 部署

```bash
./deployment/deploy.sh root@192.168.1.100 build-arm/smart-cockpit
```

脚本用 rsync 传二进制 + systemd 单元 + KMS 配置，然后 `systemctl enable/restart`。systemd 单元里设了 `QT_QPA_PLATFORM=eglfs` 等环境变量。

设备侧要装：`libqt5widgets5 libgstreamer1.0-0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good bluez bluealsa systemd fonts-noto-cjk`（**`fonts-noto-cjk` 必须有**，否则中文显示为方块），以及 eglfs 平台插件。

---

## 10. 新手常见问题排查

| 现象 | 原因 / 解法 |
| --- | --- |
| 编译报错 `qHash` 找不到 | `enum class` 当 `QHash` 的 key，Qt5 需要自己在枚举所在命名空间写 `qHash` 重载（看 `SystemState.h` 结尾） |
| 编译报错 `signals` 是保留字 | Qt 把 `signals` 定义为宏（展开成 `public`），**变量名别叫 signals**，改叫 `vals` |
| 编译报错 `QThread::start(lambda)` | 那是 Qt6 API。Qt5 用 `std::thread` 或 `QThread::create`（本项目 AsyncLogger 用 `std::thread`） |
| 链接报错 `undefined reference to gst_video_*` | CMake 缺 `gstreamer-video-1.0`，`pkg_check_modules` 里加上 |
| 启动报 `interface vcan0 not found` | 没建 vcan0，先跑 `scripts/vcan-setup.sh`（需 sudo）；无 vcan 也能跑，只是没数据 |
| `QSocketNotifier: Can only be used with threads started with QThread` | Qt 5.15 + Widgets 的无害已知警告，忽略即可 |
| 仪表不动 | 检查 `candump vcan0` 有没有帧；帧 ID 对不对（0x100）；DBC 位序对不对 |
| 空调发指令没反应 | 看日志有没有 `climate command sent`；确认 0x200 回执帧的 ack 位是 1；超时后会打印回滚日志 |
| 倒车不进全屏 | 0x300 帧 byte0 的 bit0 必须为 1（`0x01`）；看日志 `Reverse engaged` |
| 中文字体方块 | eglfs 下装 `fonts-noto-cjk` |
| 仪表刻度数字只显示中间 | 早期版本用固定 48×20 矩形画刻度数字，大表会裁边；现改为 `QFontMetricsF` 计算真实文本矩形（见 7.10） |
| 温度圆表太小 / 被裁切 | 表盘按 `min(w,h)` 取居中正方形，`sizeHint()` 返回 400×400。注意：加 `Qt::AlignCenter` 对齐标志会阻止控件拉伸、锁死在最小尺寸（见 7.12） |
| 媒体页曲库是空的 | 本机音乐目录没有 mp3/flac/wav/ogg。用 `--media-dir 你的目录` 指定，或往 `~/Music` 放两个文件 |
| 某些图标显示成方块 □ | Unicode 字符（◉❄▶♪）在极老字体下缺字形，装 `fonts-noto-core`；项目本身不依赖任何图片资源 |
| 系统状态误报 Emergency | 只会在 CAN 连接且 3 秒无新帧、或从未连上时出现；确认总线在发周期帧 |
| 鼠标键盘没反应 | eglfs 默认没输入，调试时用 `QT_QPA_PLATFORM=linuxfb` 或 x11 后端跑 |

---

## 11. 如何扩展这个项目

### 11.1 增加一个新的 CAN 帧（比如 0x500 车门状态）

1. **CanTypes.h**：加 `constexpr quint32 Doors = 0x500;`
2. **CanFrameParser.cpp** `installBuiltinTable()`：加一组 `CanSignalDef`
3. **CarService**：加状态成员 + `handleFrame500()` + 对应信号；在 `onCanFrame` 的 switch 里加 case；别忘了把新信号名加进 `hasAllSignals` 的键列表
4. **UI**：在 InstrumentPage 加一个 `QLabel`，连上新信号
5. **config/vehicle.dbc**：加 `BO_`/`SG_` 行，方便用 `--dbc` 覆盖
6. **demo-can.sh**：加一条 `cansend vcan0 500#...`

> 注意：一旦用 `--dbc` 覆盖，CarService 依赖的**信号名必须和内置表一致**（`speed`、`engine_rpm`、`fuel_level` 等），否则 `hasAllSignals` 会整帧拒绝、保持旧值——这是刻意的安全兜底，不是 bug。

### 11.2 增加一个新页面

1. `SystemState.h` 的 `PageId` 加一个枚举
2. 新建 `XxxPage.h/.cpp`
3. `MainWindow.cpp`：`new XxxPage` + `registerPage`
4. 需要抢占逻辑就把 `PagePriority` 调大

### 11.3 增加一个健康检查项

`HealthMonitor::poll()` 里加一个服务引用和健康判断，再决定它失败算 `Degraded` 还是 `Emergency`。

### 11.4 换真实 DBC

```bash
./build/smart-cockpit --dbc config/vehicle.dbc
```

解析器支持 `BO_`/`SG_` 子集，覆盖内置表。**前提是信号名与内置表一致**（见 11.1 的注意事项）。

---

## 附：一句话总结每个类

| 类 | 一句话 |
| --- | --- |
| `CanFrameParser` | 8 字节 CAN 数据 ↔ 信号数值（按 DBC 规则，截断帧整帧拒绝） |
| `CanManager` | 一个线程 + 一个 socket，收帧发帧 |
| `CarService` | 帧 → 语义化信号的中枢，带信号存在性守护，也是空调指令的出口 |
| `HealthMonitor` | 每 2 秒体检，输出 绿/橙/红（CAN 按 3 秒新鲜度判定） |
| `ScreenManager` | 页面抢占（倒车永远最大） |
| `Theme` | 全局配色 + 整套 QSS，`main` 里应用一次 |
| `MainWindow` | 顶栏 + 页面栈 + 底部导航 + 状态条 + 快捷键 |
| `GaugeWidget` | 用 QPainter 画的圆表（辉光值弧 + 红区 + 锥形指针） |
| `InstrumentPage` | 车速 / 电机转速 / 电量三张卡片 + 挡位 + 指示灯 |
| `ClimatePage` | 温度圆表 + 风量进度条 + 吹风/循环模式 + "发指令→等确认→超时回滚" |
| `MediaService` | GStreamer 播放器封装 |
| `BlueZScanner` | D-Bus 蓝牙扫描（预留，未接线） |
| `CameraService` | appsink 取流 + NV12 转 RGBA |
| `VideoWidget` | OpenGL 画面 + QPainter 轨迹线 |
| `AsyncLogger` | 队列写日志，不卡 UI |
