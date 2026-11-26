# Smart Cockpit 开发指南

> 面向第一次接触这个项目的开发者，目标是从零讲清楚"这个车机界面是怎么做出来的"。
> 本文是**当前源码参考**（中文化界面、设计 token 视觉体系、静态层缓存渲染、空调防抖及已有健壮性保护），不再按 V1/V2 拆分版本；已有保护不代表所有缺陷已修复。
> 从零学习请先看同目录的 [14 篇教程导航](README.md)，再边看本文边打开相对链接中的源码对照阅读。
> 已知缺陷与练习汇总见 [14-known-issues-and-exercises.md](14-known-issues-and-exercises.md)；本文保留模块讲解，并标明实现边界。
> 配套 [开发指南综合题](exercises/DEVELOPMENT.md) 提供源码定位、数据流追踪和跨模块验证任务；逐章题目见 [练习题库](exercises/README.md)。

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
- **给司机辅助**：挂 R 档时倒车影像抢占页面区，并叠加随方向盘角度变化的示意轨迹线（主窗口全屏，顶栏/导航仍保留）

关键约束（这也是整个设计的前提）：

| 约束 | 含义 |
| --- | --- |
| 硬件 | 泰山派 RK3566（ARM Cortex-A55，**2GB RAM**，16GB eMMC） |
| 系统 | Ubuntu 20.04，Qt 5.12.8，内核 4.19.232 |
| 显示 | **只有 X11/xcb 可用**：`eglfs` 纯色探针也整屏全黑，`linuxfb` 报 cannot mmap framebuffer；10.1 寸 800×1280 竖屏（DRM 输出名 `card0-DSI-1`） |
| 界面 | **纯 C++ + Qt Widgets，禁止 QML**，无图片素材，图标用 Unicode 字符、质感用 QPainter/QSS |
| 通信 | SocketCAN（`vcan0` 已实测收发+ACK；`can0` 实车**未验证**——需外接 CANFD 收发器并在 dts 使能） |
| 音视频 | GStreamer（MP3 播放 + 摄像头取流） |

---

## 2. 运行环境与硬件

### 目标设备（泰山派 RK3566）

- 4 核 Cortex-A55，2GB RAM，性能中等，**不能随便开高分辨率特效**
- 10.1 寸屏幕（DRM 输出 `card0-DSI-1`，800x1280）。本板 `eglfs` 平台插件不可用，
  实测走 X11/xcb：lightdm 自动登录 `lckfb` 到 XFCE，再在会话里自启应用
- 所以界面代码不能依赖窗口管理器，必须用 `QMainWindow` 自己管布局
- 界面全部用 Qt 布局（`QHBoxLayout`/`QVBoxLayout`/`QGridLayout`），可自适应 800×1280 竖屏与横屏窗口

### 主机调试环境（Ubuntu）

开发时没有实车、没有真摄像头，所以项目做了两个"替身"：

| 真车上 | 开发机上 |
| --- | --- |
| CAN 总线 `can0` | 虚拟 CAN `vcan0`（`scripts/vcan-setup.sh` 创建），或干脆 `--sim` 内置模拟 |
| 摄像头 OV5695 | `videotestsrc` 测试视频源（自带小球动画） |

这就是"主机能跑、真机也能跑"的调试思路：外设被抽象成服务，但**测试源必须显式选择，不是外设失败后自动替换**。[main.cpp](../src/main.cpp) 只有指定 `--sim` 才绕过 CAN；CAN 打开失败不会自动切到模拟器。[CameraService::start](../src/camera/CameraService.cpp) 只有 camera 参数为空（默认不传 `--camera`，或传 `--camera ""`）才用 `videotestsrc`；传入真实设备路径后若启动失败、流出错或超时，会停止并报错，不会换成测试源。

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

`QThread` 是 Qt 的线程类。难点不是"线程绝对不能共享数据"，而是**不能无同步地并发读写共享状态**。`QThread` 对象本身的线程亲和性（affinity）与它的 `run()` 所在线程也不是一回事：本项目 `CanManager` 对象在主线程创建，`run()` 才在接收线程执行。

Qt 的常用做法是：线程里 `emit` 一个信号，另一个线程用 `connect(..., Qt::QueuedConnection)` 来接。`QueuedConnection` 会把参数"打包邮寄"到接收者的事件队列，由接收线程执行槽。本项目让 `CarService` 等服务保持主线程 affinity，靠 queued 投递隔离业务状态；**它不会自动保护所有成员、指针指向的数据或跨线程直接调用**，共享数据仍需原子操作、互斥锁或明确的生命周期约束。

本项目核心代码里有一行（[CarService.cpp](../src/core/CarService.cpp) 构造函数）：

```cpp
connect(m_can, &CanManager::frameReceived,
        this,   &CarService::onCanFrame, Qt::QueuedConnection);
```

`CanManager::run()` 在接收线程 `emit frameReceived(...)`，随后主线程事件循环调用 `CarService::onCanFrame`。**这条接收路径不会直接在工作线程修改车辆业务状态**；不要由此推断整个类或整个项目都没有数据竞争。

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
│   │   ├── CanFrameParserCore.h/.cpp  # 纯 C++ DBC 子集解析/编码，不依赖 Qt
│   │   ├── CanFrameParser.h/.cpp      # Qt 类型适配层，委托 ParserCore
│   │   ├── CanManager.h/.cpp          # CAN 接收线程
│   │   ├── CarService.h/.cpp          # 车辆数据中枢
│   │   ├── SignalSimulator.h/.cpp     # 内置信号模拟器（--sim，无需 CAN）
│   │   └── HealthMonitor.h/.cpp       # 系统状态机
│   ├── logging/
│   │   ├── AsyncLoggerCore.h/.cpp     # 纯 C++ 队列/线程/文件写入
│   │   └── AsyncLogger.h/.cpp         # QString 与 Qt 消息处理器适配
│   ├── media/
│   │   └── MediaService.h/.cpp        # GStreamer MP3 播放
│   ├── camera/
│   │   ├── CameraService.h/.cpp       # GStreamer 摄像头取流 + NV12→RGBA
│   │   └── VideoWidget.h/.cpp         # QOpenGLWidget 画面 + 轨迹线
│   ├── ui/
│   │   ├── MainWindow.h/.cpp          # 主窗口：顶栏 + 页面栈 + 导航 + 状态条
│   │   ├── ScreenManager.h/.cpp       # 页面优先级管理
│   │   ├── Theme.h/.cpp               # 设计 token + 全局 QSS 主题 + 等宽数字字体
│   │   ├── InstrumentPage.h/.cpp      # 数字仪表页（含文件内 LampWidget）
│   │   ├── GaugeWidget.h/.cpp         # QPainter 圆形仪表（静态层缓存）
│   │   ├── BatteryWidget.h/.cpp       # 电池样式电量指示（静态层缓存）
│   │   ├── ClimatePage.h/.cpp         # 空调面板（含文件内 TempDialWidget/IconButton）
│   │   ├── MediaPage.h/.cpp           # 多媒体页（含文件内 AlbumArt/ElidedLabel/TransportButton）
│   │   └── ReverseCameraPage.h/.cpp   # 倒车影像页
└── tests/
    ├── CMakeLists.txt
    ├── tst_core_cxx.cpp               # 纯 C++ 核心测试（解耦守卫，不链接 Qt）
    ├── tst_canframeparser.cpp         # Qt 适配、位布局/截断帧测试
    ├── tst_cameraservice.cpp          # 起停/故障清帧/旧帧隔离/重启测试
    ├── tst_climateservice.cpp         # 空调 ack/回滚/信号守护/取消按钮/防抖测试
    ├── tst_screenmanager.cpp          # 页面优先级抢占测试
    └── tst_ui.cpp                     # UI 冒烟测试（offscreen 离屏渲染）
```

**分层思想**：

```
        ┌────────── ui/（只做显示和交互，不认识硬件）
        │    MainWindow, 4 个页面, 仪表控件, 全局主题
        ├────────── media/ camera/（外设封装，暴露信号/方法）
        │    GStreamer 播放、摄像头
        ├────────── core/（业务中枢，不认识 UI）
        │    CanFrameParser, CanManager, CarService, HealthMonitor
        └────────── logging/（工具，谁都能用）
             AsyncLogger
```

方向依赖：**ui 依赖 core/media/camera，业务代码不依赖 ui**。这样单元测试可以不启动界面直接测业务核心。

[CMakeLists.txt](../CMakeLists.txt) 还把实现分成两个构建目标：`smart_cockpit_cxx` 只包含 [CanFrameParserCore](../src/core/CanFrameParserCore.cpp) 与 [AsyncLoggerCore](../src/logging/AsyncLoggerCore.cpp)，仅用标准库和线程库；`smart_cockpit_core` 则包含 Qt 适配器、服务和 UI，名称里的 core 不意味着它无 Qt。[tests/CMakeLists.txt](../tests/CMakeLists.txt) 共注册 **6 套**测试，包含纯 C++ 层与摄像头服务。

---

## 5. 系统架构与数据流

```
                 ┌─────────────────────────────────────────────┐
                 │                 主线程（GUI）                 │
                 │                                               │
  CAN 接收线程    │  ┌──────────┐          ┌────────────┐         │
 ┌───────────┐ queued│ CarService│          │ CanManager │ 对象    │
 │ run/select│───┼─►└────┬─────┘          └────────────┘         │
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
                 │  CameraService → frameReady → VideoWidget    │
                 └───────────▲─────────────────────────────────┘
                             │ queued 到服务自己的线程，检查 generation
  GStreamer streaming 线程：handleSample → NV12 转 QImage ────────┘
```

两条主要数据流：

### 数据流 A：上行（车 → 界面）—— 仪表/倒车触发

```
vcan0 收到 0x100 仪表帧
  → CanManager 接收线程 emit frameReceived(0x100, bytes)
  → (QueuedConnection) CarService::onCanFrame
  → CanFrameParser 按 DBC 解出 speed/rpm/...
  → emit speedChanged(...) 等变化信号
  → InstrumentPage 更新仪表

vcan0 收到 0x300 挡位帧
  → CanManager 接收线程 emit frameReceived(0x300, bytes)
  → (QueuedConnection) CarService::onCanFrame
  → 直接读取首字节的倒车 bit 与 PRND 位段
  → gearChanged 更新挡位；reverseChanged 触发 MainWindow 请求/撤销倒车页
```

### 数据流 B：下行（界面 → 车）—— 空调指令

```
用户在空调页拖温度滑杆
  → ClimatePage::onControlChanged → 重启 250ms debounce 定时器（连续调节合并）
  → 定时器到期 → ClimatePage::issueClimateCommand
  → CarService::sendClimateCommand(target)  → 组帧 → CanManager::sendFrame 写 vcan0
  → sendClimateCommand 返回非 0 本地 requestId，进入 pending（返回 0 则拒绝并回滚 UI）
  → 车或外部测试程序回发 0x200，ack=1，回显五字段与 target 相同
  → 回到上行通道 → CarService::handleFrame200 确认 → emit climateAckOk(requestId)
  → 空调页匹配 requestId 后解锁；600ms 超时 → climateAckTimeout(requestId) → 页面回滚
  → 用户取消 → cancelClimateCommand(requestId) → climateCommandCancelled(requestId)
```

这里的回执来自车辆或外部测试注入，**内置 `--sim` 不生成 0x200 ACK**。理解这两条流之后，再逐模块看代码。

---

## 6. 线程模型详解

### 6.1 项目里有几个线程？

| 线程 | 由谁创建 | 干什么 | 生命周期 |
| --- | --- | --- | --- |
| 主线程 | `main()` | GUI 渲染、状态维护、信号槽 | 程序启动到退出 |
| CAN 接收线程 | `CanManager`（QThread 子类） | 阻塞 `select`+`read` 收 CAN 帧 | `open()` 启动，`closeBus()` 结束 |
| 摄像头 streaming 线程 | GStreamer 内部 | 视频帧回调、NV12→RGBA 转换 | `CameraService::start()` 期间 |
| 日志写盘线程 | `AsyncLoggerCore`（std::thread） | 从队列取日志写文件 | `start()` 启动，`stop()` 排空队列并 join |

这是主要执行上下文，不是进程线程总数的穷举；GStreamer、Qt/图形驱动还可能创建内部线程。摄像头的 `m_pollTimer` 是服务所在线程（当前为主线程）的 **200ms QTimer**，检查 bus 的 ERROR/EOS 与 **2500ms** 无帧超时，不是另一个 camera 轮询线程。

### 6.2 线程间通信方式

**规则：业务状态尽量留在主线程，跨线程投递用 queued；确需共享的数据要单独同步。**

- CAN 线程 → 主线程：`CanManager::frameReceived` → `CarService::onCanFrame`，显式 **QueuedConnection**（[CarService.cpp](../src/core/CarService.cpp) 构造函数）。
- 摄像头线程 → 主线程：streaming 线程完成转换后，通过 `QMetaObject::invokeMethod(this, ..., Qt::QueuedConnection)` 投递到 [CameraService](../src/camera/CameraService.cpp) 自己的线程；检查 `m_running` 和 `m_streamGeneration` 后才 `emit frameReady(image)`。当前服务与 `VideoWidget` 同在主线程，最后一段普通连接通常是直接调用，不是从 streaming 线程直接发给 UI。
- 摄像头共享时间戳与 generation 用原子变量；`VideoWidget` 的帧成员用 `QMutex`；CAN 发送的 `write()` 用 `m_sendMutex` 串行化。锁的保护范围各不相同，不能推导出所有状态都安全。
- 日志线程：[AsyncLoggerCore](../src/logging/AsyncLoggerCore.h) 用 **`std::mutex + std::condition_variable + std::deque + std::thread`** 实现生产者-消费者队列，不是 Qt 队列，也不靠信号槽。

仍需注意：[CanManager.h](../src/core/CanManager.h) 的 `m_framesReceived` 目前是普通 `quint64`，工作线程递增、主线程读取，没有原子/锁保护；这是待核对的并发缺口，不能拿它作为共享状态已全部安全的例子。

### 6.3 为什么要"阻塞 read"而不是定时器轮询？

CAN 帧到达是"事件驱动"的，`select()` 等待期间不忙转，有数据就能唤醒。项目在 `CanManager::run()` 里设置 100ms 超时，目的是**及时收帧并定期检查退出请求**；`closeBus()` 请求中断后等待线程退出，再关闭 socket，避免无限阻塞读拖住停止流程。

### 6.4 QImage 跨线程传递为什么安全？

[main.cpp](../src/main.cpp) 注册了 `QImage` 元类型，但**注册不是线程安全开关**。当前 camera 的 queued lambda 按值捕获 `QImage`，靠隐式共享与引用计数延长像素生命周期；转换完成后不再修改这份图像，且不把 GStreamer 映射缓冲区指针交给 UI。[VideoWidget::setFrame](../src/camera/VideoWidget.cpp) 再用 `QMutex` 保护帧成员存取。仍不能并发修改同一个 `QImage` 对象或绕过 GUI 线程调用 QWidget 方法，见 [7.14](#714-cameraservice--videowidget--reversecamerapage--倒车影像)。

---

## 7. 模块逐一讲解

### 7.1 main.cpp —— 程序组装车间

`main()` 的主线是：**解析命令行 → 创建服务 → 连接并启动 → 事件循环 → 正常退出清理**。下例是 [main.cpp](../src/main.cpp) 的**生命周期节选，不是完整可编译实现**：省略了 QApplication/参数定义、主题、日志初始化、元类型注册及可选 DBC 加载。

```cpp
sc::CarService car;
sc::MediaService media;
sc::CameraService camera;
sc::HealthMonitor health(&car, &camera, &media);
sc::MainWindow window(&car, &media, &camera);
QObject::connect(&health, &sc::HealthMonitor::stateChanged,
                 &window, &sc::MainWindow::onSystemStateChanged);
QObject::connect(&health, &sc::HealthMonitor::diagnosticsChanged,
                 &window, &sc::MainWindow::onDiagnostics);
window.showFullScreen();

sc::SignalSimulator *simulator = nullptr;
if (parser.isSet(simOption)) {
    car.setSimulatedMode(true);
    simulator = new sc::SignalSimulator(&car);
    simulator->start();
}
media.init();
if (!parser.isSet(simOption))
    car.start(parser.value(canOption));
camera.start(parser.value(cameraOption));
health.start();

const int exitCode = app.exec();
health.stop();
if (simulator) {
    simulator->stop();
    delete simulator;
}
car.stop();
camera.stop();
media.shutdown();
sc::AsyncLogger::instance().stop();
return exitCode;
```

`app.exec()` 驱动事件循环，不是冻结线程。正常返回后的顺序是 **health → simulator → car → camera → media → logger**；`--sim` 分支不会调用 `car.start()`。源码当前没有把外设启动返回值作为退出条件，也没有自动切换数据源；不要把上述节选当作完整的失败恢复方案。

命令行参数（用 Qt 自带的 `QCommandLineParser`）：

| 参数 | 作用 |
| --- | --- |
| `--can vcan0` | CAN 接口名（不传 `--sim` 时使用） |
| `--camera ""` | 摄像头设备路径，空 = 测试源 |
| `--dbc file` | 用 DBC 文件覆盖内置信号表 |
| `--log-dir dir` | 日志目录 |
| `--media-dir dir` | 音乐目录 |
| `--sim` | 显式用内置信号模拟器驱动仪表/挡位/倒车触发，**不需要 CAN/vcan**，不模拟空调 ACK（见下） |

**信号模拟器（[SignalSimulator](../src/core/SignalSimulator.cpp)）**：`--sim` 模式下程序不起 CAN socket，改由主线程 100ms 定时器把合成帧喂给 `CarService::onCanFrame`。一圈 **60 秒模拟时间**（每次 timeout 累加 100ms，GUI 忙时会慢于真实时间）：0..8s 加速、8..30s 巡航（带转向灯）、30..40s 刹停、40..45s 停车、**45≤t<52s 倒车**、52..60s 停车；每 40 秒的末 3 秒亮发动机灯，每 25 秒的末 3 秒是双闪，**不是每 25 秒倒车**。在内核没有 CAN 模块的设备上，这是演示 HMI 的一种方式，而不是 CAN 失败后的自动降级。

模拟器只生成 **0x100 / 0x300 / 0x400**，不会回复 **0x200 ACK**，所以单独运行 `--sim` 调空调仍会超时回滚。`setSimulatedMode(true)` 只让 `canConnected()` 返回 true；健康监控仍要求 `onCanFrame` 更新的时间戳在 3 秒内。模拟帧绕过 `CanManager`，诊断中的 `frames:0` 可以是正常现象；模拟器停了仍会因时间戳过期进入 Emergency。

### 7.2 数据结构 —— CanTypes.h / SystemState.h

**[CanTypes.h](../src/core/CanTypes.h)**：定义一帧里每个信号的"解释方式"，只依赖标准库，不使用 `QString`/`quint32`。

```cpp
struct CanSignalDef {
    std::string name;
    int startBit = 0;
    int bitLength = 1;
    bool bigEndian = false;
    bool isSigned = false;
    double factor = 1.0;
    double offset = 0.0;
    double minVal = 0.0;
    double maxVal = 0.0;
    std::string unit;
};

namespace FrameId {
    constexpr std::uint32_t ICU_Dynamic = 0x100;
    constexpr std::uint32_t HVAC        = 0x200;
    constexpr std::uint32_t Gear        = 0x300;
    constexpr std::uint32_t Chassis     = 0x400;
}
```

`name`/`unit` 是信号名与单位，`startBit`/`bitLength` 是位布局，`bigEndian=true` 表示 Motorola；`factor`/`offset` 用于物理值换算，`minVal`/`maxVal` 是范围元数据，不等于自动验证规则。

**[SystemState.h](../src/core/SystemState.h)**：定义三个枚举和一个结构体，是 Qt 层的"全局共用词汇"。下列是字段节选；实际 `ClimateState` 还有默认值及五字段相等比较运算符。

```cpp
enum class SystemState { Normal, Degraded, Emergency };   // 状态机三态
enum class PageId { Instrument, Media, Climate, ReverseCamera };  // 四个页面
enum class PagePriority { Low, Normal, High, Urgent };   // 页面抢占优先级
struct ClimateState { int temperature; int fanSpeed; int blowMode;
                      bool acOn; bool autoMode; };        // 空调状态
```

### 7.3 CanFrameParser —— DBC 解析器

**职责**：经典 CAN 数据（最多 8 字节）↔ 一组"信号名→数值"。真正的纯 C++ 算法在 [CanFrameParserCore](../src/core/CanFrameParserCore.cpp)，用 `std::vector<std::uint8_t>`/`std::unordered_map<std::string, double>`；[CanFrameParser](../src/core/CanFrameParser.cpp) 是 Qt 适配层，把 `QByteArray`/`QHash<QString, double>` 转给 core，再转回结果，不依赖 UI/Qt 工作线程。两层分别有测试（见 [8.3](#83-单元测试)）。

内置信号表在 **CanFrameParserCore::installBuiltinTable()**，覆盖 0x100（速度/转速/电量/转向灯/故障灯）、0x200（空调，含 ack 位）、0x300（`gear_signal`）和 0x400（方向盘角度）。虽然表中有 0x300，`CarService` 的挡位处理仍走裸字节解析，不经 DBC（见 [7.5](#75-carservice--车辆数据中枢)）。

**位序（Bit Order）是本项目最容易懵的点**，单独解释：

CAN 信号有两种排布方式（`@1`=小端 Intel，`@0`=大端 Motorola）：

- **Intel 小端**：`startBit` 指向信号最低有效位，遍历时**全局 bit 索引递增**（`startBit+i`），不是按图上的"左到右"判断。`speed` 的 `0|16@1+` 先取 byte0 的 bit0..7，再取 byte1 的 bit0..7；`78 00` 解出 120。
- **Motorola 大端**：`startBit` 指向信号最高有效位，从 `byte=startBit/8`、`bit=startBit%8` 开始，**bit 递减到 0 后，转到下一 byte 的 bit7**，逐位左移累积原始值。不能用 `7-startBit%8` 代替起始 bit。

| Motorola 布局（factor=1、offset=0） | 线上的字节 | 解码值 |
| --- | --- | --- |
| `7\|16@0+` | `12 34`（十六进制 `1234`） | `0x1234` = 4660 |
| `0\|2@0+` | `01 80`（十六进制 `0180`） | 3（byte0 bit0、byte1 bit7） |

看 core 的 `decodeSignal()`/`encodeSignal()` 两套循环及 [tst_core_cxx.cpp](../tests/tst_core_cxx.cpp) 的线字节断言，比只做编码→解码回环更容易发现位序错误。[demo-can.sh](../scripts/demo-can.sh) 的 0x100 注释也可对照。

**物理值换算**：`物理值 = 原始值 × factor + offset`。比如 `fuel_level` 的 factor 是 0.5，所以原始字节 `100` 表示 `50%`。

**DBC 文件加载**：`loadDbcText()` 逐行解析 `BO_`（帧定义）和 `SG_`（信号定义）的**有限子集**；遇到 `BO_` 就立即清空对应帧的信号表，再逐项加入 `SG_`，其他帧保留。**覆盖不是原子操作**：后续行解析失败，前面已清空或写入的表不会回滚；不能把返回 false 理解为"内置表完全没变"。复用信号、复杂属性等任意 DBC 特性不在支持承诺内；业务还依赖固定信号名与语义，并非拿到任何 DBC 都可即插即用。

**布局校验（经典 CAN 边界）**：core 的 `requiredSignalBytes()` 要求 `0≤startBit<64`、`1≤bitLength≤64`，并计算实际占用的最高字节，必须不超过 **8 byte / 64 bit**。Intel 需要 `(startBit+bitLength+7)/8` 字节；Motorola 需要 `startBit/8 + (7-startBit%8+bitLength+7)/8` 字节（整数除法）。即使输入缓冲区超过 8 字节，跨出经典 CAN 布局的信号仍被拒绝；例如 Intel `1|64`、Motorola `6|64` 都非法。

**健壮性（整帧解码全有或全无）**：core 与 Qt 适配层的 `decode()` 在未知帧、非法布局或数据不足时返回 false，**不修改调用者已有的 out**；先验证全部信号再清空/写入结果。这里验证的是信号实际所需字节，不是强制所有帧都满 8 字节，也不是完整校验 `BO_` 声明的 DLC。底层 `decodeSignal()` 的失败返回值却是 `0.0`，**不能区分错误与合法零值**，业务宜使用带 bool 的整帧接口。

`encodeSignal()` 先校验布局，非法时返回 false 且不修改输出，合法后才按需扩容（不一定扩到 8 字节）；整帧 `encode()` 则创建 8 字节零填充数据，只编码提供的键。**布局合法不代表数值安全**：DBC 的 min/max、factor 是否为零/有限数、输入物理值范围与 64 位数值精度并未自动全部验证。调用方仍须验证数值，不能把这些接口当作完整协议验证器。

### 7.4 CanManager —— CAN 接收线程

[CanManager](../src/core/CanManager.cpp) 继承 `QThread`，`run()` 是线程入口。下面两段仅为流程示意，省略了错误检查、变量声明和系统调用参数，不能作为完整实现复制。

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

**发送**也在这里，`sendFrame()` 组装 `struct can_frame` 后用 `m_sendMutex` 保护 `write()`，串行化多个发送调用；这不是为所有 socket 状态加锁。

**未修复：扩展 CAN ID 发送截断**。当前发送逻辑无论 `extended` 是否为 true，都先做 `id & CAN_SFF_MASK`（11 位），然后才添加扩展帧标志，导致 29 位扩展 ID 丢高位。示例使用的 0x100..0x400 标准帧不受此点影响，但不能宣称已完整支持扩展帧。

### 7.5 CarService —— 车辆数据中枢

**全项目最核心的类**，[CarService](../src/core/CarService.cpp) 把 CAN 原始帧变成"语义化信号"。当前在主线程构造并保持 affinity，CAN 接收通过 queued 槽更新状态，模拟器也在主线程调用；**安全前提是遵守这条线程边界，不是信号槽自动保护所有状态**。

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
- `handleFrame300`：`byte0 bit0 = 倒车激活`，变化时 `emit reverseChanged(reverse)`，触发或撤销倒车页面请求；bits1..3 表示 0=P / 1=R / 2=N / 3=D，挡位与倒车都直接走裸字节解析。
- `handleFrame400`：方向盘角度（`0|16@1-` 有符号 ×0.1），喂给轨迹线。
- `handleFrame200`：空调确认逻辑（重点，见 [7.12](#712-climatepage--空调面板重点看)）。

**健壮性（信号守护）**：0x100 / 0x200 / 0x400 在应用数据前，会用 `hasAllSignals()` 校验解析出的键是否齐全。缺字段（例如 DBC 覆盖后字段名不一致）或 `decode()` 已拒绝截断帧时，就**跳过该帧的业务状态更新、保留最后已知有效值**。它不能识别"字段齐全但含义错误"，不是任意 DBC 的适配器。`onCanFrame()` 在分发前已经更新时间戳，所以 CAN 健康也不等于业务数据已经通过校验。

**空调指令下行**（`sendClimateCommand`）：

1. 已有 pending 时返回 **0**，拒绝新 request，不替换当前 target；编码失败也返回 0。
2. 把 temperature/fanSpeed/blowMode/acOn/autoMode 五字段编码成 0x200，尝试发送。**CAN 未打开或 write 失败仍可能返回非 0**，进入等待超时，而不是必定走"发送失败"分支。
3. 递增本地 request ID（跳过 0），记录完整 target，设置 pending 并启动 **600ms** ACK 定时器。下面是函数末段节选：

```cpp
if (++m_climateRequestSeq == 0)
    ++m_climateRequestSeq;
const quint32 requestId = m_climateRequestSeq;
m_pendingClimateTarget = target;
m_pendingClimate = true;
m_ackTimer.start();
return requestId;
```

`handleFrame200()` 只有在 **pending + ack=1 + 回显五字段全部等于 target** 时确认请求，先更新 `m_climate`，再发 `climateAckOk(requestId)` 与 `climateStateChanged`。无 pending 或 target 不符的 ACK 被忽略；`ack=false` 则只在空闲且状态改变时同步车辆主动状态。

`cancelClimateCommand(id)` 仅接受当前非 0 ID：停止 timer、清 pending、发 `climateCommandCancelled(id)`，不修改已确认状态，也不发送 CAN 撤销报文。`stop()` 同样取消在途请求。超时/取消后的 UI 回滚只是恢复已确认值，不保证车辆没有执行原命令。

**协议边界**：request ID 只存在本地信号中，线上没有 transaction ID；它能过滤旧的本地通知，target 比较能挡住不同目标的旧 ACK，**但同目标旧 ACK 与当前请求的 ACK 无法区分**。

### 7.6 HealthMonitor —— 系统状态机

[HealthMonitor](../src/core/HealthMonitor.cpp) 在 `start()` 时先 `poll()` 一次，之后默认每 2 秒轮询三个服务，输出 `Normal / Degraded / Emergency`：

```
CAN 最近 3 秒内没有新帧（或从未连上）→ Emergency
摄像头 / 媒体异常                    → Degraded
都正常                              → Normal
```

判定 CAN 存活只看"连接状态 + 最近帧时间戳新鲜度"（`canConnected() && now - lastFrameTimeMs < 3000`），**不要求轮询间隔内帧计数必须增长**；但即使停车，只要 3 秒无帧仍会告警，事件型总线需要另行评估该阈值。camera 要求 `isRunning()` 且最近帧年龄小于 2500ms；media 只看 `healthy()`，不能据此推断播放、换曲、进度都正常。

`Emergency` 会让主窗口顶栏的状态胶囊变红、显示"CAN 丢失"。这就是状态机驱动 UI 的实现——**UI 不主动查状态，等状态机发信号**。

### 7.7 ScreenManager —— 页面优先级管理

`QStackedWidget` 管的是"叠了一摞页面，显示第几个"，`ScreenManager` 在此基础上加了**优先级抢占**。

核心是 [ScreenManager::requestPage(id, priority)](../src/ui/ScreenManager.cpp)：

```cpp
bool ScreenManager::requestPage(PageId id, PagePriority priority) {
    const PagePriority currentPrio =
        m_activePriorities.value(m_current, PagePriority::Low);
    if (id == m_current) {
        if (priority > currentPrio)
            m_activePriorities.insert(id, priority);
        return true;
    }
    if (priority < currentPrio)
        return false;
    m_previous = m_current;
    m_activePriorities.insert(id, priority);
    switchTo(id);
    return true;
}
```

倒车抢占页面区就是这么写的（[MainWindow::onReverseChanged](../src/ui/MainWindow.cpp)；主窗口全屏，但顶栏/导航/状态条并未隐藏）：

```cpp
void MainWindow::onReverseChanged(bool active) {
    if (active)
        m_screen->requestPage(PageId::ReverseCamera, PagePriority::Urgent);
    else
        m_screen->dismissPage(PageId::ReverseCamera); // 撤请求，再选最高优先级
}
```

`dismissPage()` 移除请求；若移除的是当前页，有剩余请求就选最高优先级，只有请求表为空才用 `m_previous`。**同级请求保存在 QHash 中，没有最近访问次序**，不能保证倒车退出后返回上一页。这是未修复的导航返回问题，单一普通页的测试通过并不覆盖多个同级页面的场景。

### 7.8 Theme —— 设计 token + 一个 QSS 管全界面

整套视觉收敛为两层：**[Theme.h](../src/ui/Theme.h) 的设计 token 常量** + **[Theme.cpp](../src/ui/Theme.cpp) 的一份全局 QSS**，`main.cpp` 启动时应用一次：

```cpp
QApplication::setStyle(QStringLiteral("Fusion"));
app.setStyleSheet(sc::Theme::styleSheet());   // 全局主题
```

**设计 token**（Theme.h，`constexpr const char *`，全项目唯一事实来源）：

| 类别 | token | 值 |
| --- | --- | --- |
| 背景/卡片/分隔线 | `Bg` / `Card` / `CardBorder` | `#0A0E14` / `#161B22` / `#21262D` |
| 文字 | `TextPrimary` / `TextSecondary` / `Disabled` | `#E6EDF3` / `#8B949E` / `#484F58` |
| 强调 | `Accent` / `AccentSoft` / `Cool`（文字态强调） | `#2F81F7` / `#2563EB` / `#38BDF8` |
| 语义 | `Ok` / `Warn` / `Err` / `Warm` | `#22C55E` / `#F59E0B` / `#EF4444` / `#FB923C` |
| 几何 | `RadiusSmall` / `RadiusCard` / `RadiusMax` / `SpacingUnit` | 8 / 12 / 16 / 8 px |

**硬规则**：任何 .cpp 里出现颜色/圆角魔数都算违规——自绘控件一律
`QColor(QLatin1String(Theme::Accent))`，标准控件一律走 QSS。等宽数字用
`Theme::monoFont(pixelSize, weight)`（基于 `QFontDatabase::systemFont(FixedFont)`），
车速、温度、电量、时间读数使用等宽数字，减少同位数刷新时的宽度抖动。这是视觉维护规则，不是"所有旧代码已完全符合"的声明；例如 `VideoWidget` 仍有硬编码颜色，不应据此擅自改动画面或协议。

QSS 和 CSS 很像，选择器靠控件 `setObjectName()` 匹配：

```
QLabel#brandName { ... }       // 匹配 objectName == "brandName" 的 QLabel
QFrame#card { ... }            // 卡片
QPushButton#toggle:checked     // 分段按钮的"选中"伪状态
```

**动态状态用属性选择器**（取代内联 setStyleSheet）：控件 `setProperty()` 一个
动态属性，QSS 按属性值匹配，改属性后 `style()->unpolish/polish` 刷新：

```
QLabel#statePill[state="ok"]    { ... color: #22C55E; ... }   // 系统正常
QLabel#statePill[state="warn"]  { ... color: #F59E0B; ... }   // 系统降级
QLabel#statePill[state="err"]   { ... color: #EF4444; ... }   // CAN 丢失
QLabel#canStatus[connected="true"]  { ... }
QLabel#statusPill[state="warn"] { ... }                        // 空调等待确认
```

```cpp
// MainWindow::onSystemStateChanged 里只剩"文案 + 属性"，没有样式串：
m_statePill->setText(QStringLiteral("系统正常"));
m_statePill->setProperty("state", QStringLiteral("ok"));
m_statePill->style()->unpolish(m_statePill);
m_statePill->style()->polish(m_statePill);
```

主窗口根 widget `objectName = "appRoot"` 铺 `#0A0E14` 纯色底；各页面
`background: transparent` 透出。中文字体回退（Noto Sans CJK SC / WenQuanYi）
列入 QSS 的 `*` 规则。

### 7.9 MainWindow —— 顶栏 + 页面栈 + 底部导航

源码：[MainWindow.cpp](../src/ui/MainWindow.cpp)。

```
┌──────────────────────────────────────────────┐
│ 顶栏 topBar（64px）：SC 智能座舱｜页面标题 …… CAN｜状态｜时钟 │
├──────────────────────────────────────────────┤
│           QStackedWidget（页面区，弹性占满）     │
├──────────────────────────────────────────────┤
│ 底部导航 navBar（64px）：仪表 │ 媒体 │ 空调      │
├──────────────────────────────────────────────┤
│ 状态条 statusStrip（一行小字，诊断摘要/播放错误）  │
└──────────────────────────────────────────────┘
```

- **顶栏**：品牌 Logo（`SC`）+ 品牌名"智能座舱" + 副标"v1.0 · 车载HMI" + 页面标题（仪表盘/多媒体/空调控制/倒车影像）+ CAN 状态胶囊（`●`=已连 / `○`=未连，样式走 `[connected]` 属性选择器）+ 系统状态胶囊（绿"系统正常"/橙"系统降级"/红"CAN 丢失"，样式走 `[state]` 属性选择器，见 7.8）+ 时钟日期。时钟由 `QTimer` 每秒驱动。所有高度/边距/间距均为 8 的倍数。
- **底部导航**：三个 `QToolButton` 放进互斥 `QButtonGroup`，文字是 Unicode 图标 + 中文标签（`◉ 仪表`、`▶ 媒体`、`❄ 空调`）；选中态 = Accent 色文字 + 底部 2px Accent 高亮条。点击导航请求 `ScreenManager` 切页，`pageChanged` 再更新标题，并在存在对应导航按钮时 `setChecked(true)`。倒车页没有导航按钮，普通请求被拒时也没有额外回选逻辑，因此不能承诺所有抢占/拒绝场景的导航选中态都一致。
- **状态条**：很矮的 `QLabel`。两个来源：`HealthMonitor::diagnosticsChanged` 写入 "CAN:OK CAM:OK MEDIA:OK frames:N" 摘要；`MediaPage::mediaError` 信号写入"播放失败：…"（MediaPage 把 `MediaService::errorOccurred` 加上中文前缀后转发，`MainWindow` 直接连到 `m_statusStrip->setText`）。
- **快捷键**：F1/F2/F3 切页（调试用，生产可删）。

### 7.10 GaugeWidget —— QPainter 圆形仪表（静态层缓存）

**QPainter 是 Qt 的 2D 绘图 API**，用在 `paintEvent()` 里画一切。对照 [GaugeWidget.cpp](../src/ui/GaugeWidget.cpp) 与 [BatteryWidget.cpp](../src/ui/BatteryWidget.cpp)，理解这块的关键有两点：**角度与坐标换算**、**静态层缓存**。

仪表盘是"270° 的扇形，从左下角扫到右下角"，代码约定：

```
最小刻度 = 225°  →  最大刻度 = -45°
角度 = 225° - 值占比 × 270°
```

**静态层缓存（RK3566 性能的关键设计）**：表盘上 90% 的内容（表盘底、弧轨道、
红区、全部刻度与数字、label）**不随数值变化**，没必要每帧重画。所以控件分两层：

- `rebuildCache()`（通常在 `resizeEvent` / `configure()` / `setLabel()` 时运行；`paintEvent` 发现空缓存时也会补建）：
  把上述静态内容一次性渲染进成员 `QPixmap m_staticLayer`（DPR 感知），同时算好
  全部几何（弧矩形、中心、笔宽）、构建好**指针路径** `m_needlePath`（指向正上方）、
  备好读数字体 `m_valueFont`（等宽）与目标矩形。渐变、`QFontMetrics`、路径构造
  **只允许出现在这里**。
- `paintEvent()`（每帧）：`drawPixmap(静态层)` → 值弧（单色实心笔，无辉光层）→
  指针（`translate(中心)` + `rotate(90° - 角度)` 后画**预建路径**，每帧零路径构造）→
  中心毂 → 大数字读数 + 单位。

缓存有效时，单帧绘制从反复构建静态刻度/路径，变为"1 次 blit + 少量动态图元"，
目的是降低 CAN 高频更新的绘制开销；这不是 RK3566 板上帧率实测结论。
`setValue()` 仍全量 `update()`（指针旋转覆盖区域不规则），局部更新是否更划算需测量。

值弧跨度：`span = (valueAngleDeg() - 225) * 16`（`drawArc` 角度单位是 1/16 度）。
颜色语义：值弧/指针默认 Accent `#2F81F7`，进入红区（车速 200 / 转速 6000）后切
Err `#EF4444`。

指针旋转的换算（预建路径指向正上方，Qt 坐标系 y 向下）：

```cpp
painter.translate(m_center);
painter.rotate(90.0 - valueAngleDeg());   // 正上方 -> 目标角度
painter.drawPath(m_needlePath);           // 路径在 rebuildCache 构建，每帧只旋转
```

**刻度数字**（静态层内）：先用 `QFontMetricsF::boundingRect(text)` 算出文本真实
宽高，再 `moveCenter` 到刻度位置，避免固定文本框造成的常见裁边。字号按表盘半径
`r` 缩放并设最小字号，使 180px 小表与大表尽量保持比例；极端尺寸/字体仍需渲染核对。

`BatteryWidget`（电量）同构：电池外壳/正极/label 入静态层 + 预建 `m_innerClip`
裁剪路径；每帧只画填充矩形与百分比。阈值语义：>30% Accent 蓝、≤30% Warn 黄、
≤15% Err 红。

### 7.11 InstrumentPage —— 数字仪表页

[InstrumentPage](../src/ui/InstrumentPage.cpp) 是纯"视图"类：构造函数里把需要的 `CarService` 信号连到自己的槽，每个槽只干一件事——更新对应控件。

竖屏**纵向三段**布局（stretch 47 : 30 : 20，车速是第一信息所以占最大头）：

```
┌──────────────────────────┐
│  车速（大圆表 + 超大数字）   │  47
├──────────────────────────┤
│  电机转速（中圆表）          │  30
├──────────────────────────┤
│  车辆状态                  │  20
│  [挡位 D] [电量 50%] [指示灯×6]│
└──────────────────────────┘
```

两个圆表：车速（0~240 km/h，红区 200）、电机转速（0~8000 rpm，红区 6000），
都是 GaugeWidget（见 7.10），视觉权重靠尺寸拉开。挡位是 `QLabel#gearLabel`
（QSS 样式：大字符 + 底部 Accent 高亮条），P/R/N/D 文本逻辑不变。

**指示灯（LampWidget）**：六枚（左转 ◀ / 右转 ▶ / 双闪 ▲ / 电机 / ABS / 气囊）
是文件内私有自绘小控件（`InstrumentPage.cpp` 匿名命名空间，不新增源文件）：
熄灭 = Disabled 色 1px 描边芯片，点亮 = 语义色实心填充（转向/双闪 Warn 黄、
故障灯 Err 红）。因为类型在 .cpp 里，头文件成员用 `QWidget *` 基类指针保存，
`.cpp` 内的 `setLamp(QWidget*, bool)` 自由函数做一次 `static_cast` 回去——
这是"文件内私有类 + 头文件不透明指针"的常用写法。转向灯业务逻辑不变：
`left || hazard`、`right || hazard` 同时点亮左右。

测试锚点（改动 UI 时不能碰）：`speedGauge` / `rpmGauge` / `batteryWidget` /
`gearLabel` 四个 objectName 被 `tst_ui.cpp` 用 `findChild` 依赖。

### 7.12 ClimatePage —— 空调面板（重点看）

[ClimatePage](../src/ui/ClimatePage.cpp) 演示了"**防抖合并 → 发指令 → 等车确认 → 没确认就回滚**"的 UI 闭环；车辆确认需真实 CAN 或测试注入，不能把 `--sim` 当作空调 ECU。
竖屏纵向布局（stretch 42 : 18 : 24 + 固定底行）：

```
┌────────────────────────────┐
│ 温度                        │  42
│      ╭─────────╮            │
│      │   22°C  │  温度圆表   │
│      ╰─────────╯            │
│     [−]        [+]          │
│      ─────●─────            │
├────────────────────────────┤
│ 风速   3  ██████░░░░         │  18
│      ────●─────             │
├────────────────┬───────────┤
│ 吹风模式（2×2）  │ 循环模式    │  24
│ 面部 腿部       │ 内循环     │
│ 除霜 上下       │ 外循环     │
├────────────────┴───────────┤
│ AC 制冷 自动    就绪  [取消待处理]│  固定底行
└────────────────────────────┘
```

**温度圆表（TempDialWidget）**：QPainter 自绘，定义在 `ClimatePage.cpp` 匿名
命名空间（不新增源文件）。与 GaugeWidget 同款**静态层缓存**（表盘/轨道/量程字
入 `m_staticLayer`，每帧只画值弧 + 读数）；颜色是**双色态**——≤20℃ 用 Cool
`#38BDF8`、>20℃ 用 Warm `#FB923C`，无插值无辉光。表盘始终居中正方形，
`sizeHint()` 320×320。`−`/`+` 按钮本质是拨滑杆：

```cpp
connect(m_tempSlider, &QSlider::valueChanged, tempDial, &TempDialWidget::setTemperature);
```

**模式图标键（IconButton）**：吹风模式 4 键（面部/腿部/除霜/上下出风）与内外
循环 2 键是 `QPushButton` 子类（同样文件内私有），只 override `paintEvent`/
`sizeHint`——图标是构造函数里预建的 24×24 单位坐标 QPainterPath（车规 HVAC
线框 glyph），paint 时缩放绘制；选中态 = Accent 描边 + Cool 色图标。checkable、
clicked 走 QPushButton 基类；吹风键另有强制回选及 `onControlChanged()` 连接。

**循环风尚未接入命令**：内/外循环键只有互斥选中，没有连接到 `onControlChanged()`，`ClimateState` 和当前 DBC 也没有循环字段，所以只是本地选中态。另有**吹风语义待统一**：[SystemState.h](../src/core/SystemState.h)/UI 的 0..3 是面部、腿部、除霜、上下，而 [vehicle.dbc](../config/vehicle.dbc) 的 `CM_` 注释写的是吹面、吹面+脚、吹脚、除霜。解析器忽略注释，本文仅记录差异，不擅自决定或修改协议枚举。

**指令防抖（debounce，关键设计）**：如果每次 `valueChanged` 都立即发 CAN 指令，
拖一次滑块会发出一串指令且第一发就锁面板、后续被吞、ACK 后滑块弹回。现在的
做法是拆成两步：

```cpp
void ClimatePage::onControlChanged()
{
    if (m_syncing || m_pendingRequestId)
        return;
    updateReadouts();
    m_debounceTimer.start();
}

void ClimatePage::issueClimateCommand()
{
    if (m_pendingRequestId)
        return;
    m_lastSent = readControls();
    const quint32 requestId = m_car->sendClimateCommand(m_lastSent);
    if (!requestId) {
        m_cancelButton->setEnabled(false);
        setPanelEnabled(true, QStringLiteral("发送失败，已回滚"));
        applyControls(m_car->climate());
        return;
    }
    m_pendingRequestId = requestId;
    m_cancelButton->setEnabled(true);
    setPanelEnabled(false, QStringLiteral("等待车辆确认..."));
}
```

要点：**250ms debounce 窗口内不锁面板**，用户可连续调节，停下后发最后一组值；
非 0 request ID 表示服务接受了请求，之后才开始 **600ms ACK 等待**。返回 0 时页面
立即恢复已确认值，不占用 pending。ACK/超时/取消槽均先检查
`!m_pendingRequestId || requestId != m_pendingRequestId`，过滤 0、旧 ID 或空闲通知。
`applyControls` 开头会停止 debounce，车辆主动状态同步会作废尚未发送的用户输入。

**pending 期间并未完全锁住全部控件**：`setPanelEnabled()` 禁用温度/风量滑块、
AC/自动与吹风键，但 `−`/`+` 不在其中，仍可通过 `setValue()` 改动已禁用的温度滑块
和圆表；pending 守卫阻止了新命令，却不能阻止这次本地显示变化。循环键也未被禁用。
这是尚未修复的交互缺陷，不应宣称 pending 时所有用户输入都已屏蔽。

**取消待处理**：按钮调用 `m_car->cancelClimateCommand(m_pendingRequestId)`，
真正停止服务的 ACK timer 并清 pending；匹配的 `onCommandCancelled(id)` 再清 UI 的
`m_pendingRequestId`、解锁并恢复已确认值，显示 **"已取消"**，不是假装超时。
按钮随本页的非 0 pending ID 使能/禁用；`objectName` 是 `cancelPending`
（测试用 `findChild` 定位），红色样式走 QSS `QPushButton#cancelPending`。

**车回执时**（`onAckOk`）：服务先校验 pending 与完整五字段 target，页面再匹配 ID，
解锁、禁用取消按钮，用**车确认过的值**刷新控件。`ack=false` 不是确认请求：服务
空闲时才允许它同步主动状态，页面也只在没有本页 pending ID 时应用状态更新。

**超时回滚时**（`onAckTimeout`）：匹配 ID 后清本页 pending、解锁，控件恢复成
**之前车确认的旧值**。这是本地 UI 回滚，不是向车辆发一条反向操作。

`m_syncing` 防抖细节不变：`applyControls()` 用代码设控件值时会触发
`valueChanged`，先把 `m_syncing=true` 让 `onControlChanged` 直接 return。
**这是"程序改控件 vs 用户改控件"的经典防抖写法**（与上面的指令 debounce 是
两层不同机制：一个防信号回环，一个合并连续输入）。

**状态丸**：`statusPill` 显示"就绪/等待车辆确认.../已确认/超时未确认，已回滚/
已取消/发送失败，已回滚"，样式走 `[state="ok|warn"]` 属性选择器（见 7.8）。风速大字读数
`fanValue` 用等宽字体 + 纯文本（不再拼 HTML），样式走 `QLabel#fanValue`；
`fanBar`（QProgressBar）样式走 `QProgressBar#fanBar`（6px 细条、Accent chunk）。

### 7.13 MediaPage + MediaService —— 多媒体

竖屏上下结构（播放卡 : 列表卡 = 37 : 63）：

```
┌──────────────────────────────┐
│ 正在播放                      │  37
│  [占位块]  曲名（单行省略）     │
│  120×120   本地音乐库          │
│  00:12 ───●────── 03:45       │
│       ▶(64) ❚❚(56) ■(56)      │
├──────────────────────────────┤
│ 音乐库                        │  63
│  song1.mp3                    │
│  song2.flac                   │
└──────────────────────────────┘
```

**[MediaService](../src/media/MediaService.cpp)** 是 GStreamer 的 Qt 包装：
- `init()`：`gst_init` + `gst_element_factory_make("playbin")`。
- `openFile(path)`：检查文件存在后设 `uri`、设 `audio-sink`，更新曲名，再 `set_state(PLAYING)`。**当前未先退到 READY/NULL，可能在 PLAYING 中直接改 URI**，换曲状态流程存在已知缺陷，不能保证显示的新曲名就是实际播放曲目。
- `pollBus()`：200ms 定时器从 GStreamer bus 取一条消息（EOS/错误/标签/状态变化等），处理后查询进度/时长。**无 message 时提前 return，查询也被跳过**；稳定播放时 bus 安静，进度可能不刷新，这是另一项未修复缺陷，不是已实现的稳定 200ms 进度更新。

**不用额外 GLib 主循环**：本项目不用 `gst_bus_add_watch()` 驱动应用回调，而用 Qt 的 `QTimer` 定时 `gst_bus_pop_filtered()` 手动取 bus 消息；GStreamer 内部 streaming 线程仍照常工作。这样避免应用层再维护一个 GLib 事件循环，不代表 GStreamer 的所有工作都移到了 Qt 主线程。

**[MediaPage](../src/ui/MediaPage.cpp)** 的三个文件内私有控件（匿名命名空间，不新增源文件）：

- **AlbumArtWidget**（`objectName: albumArt`，120×120）：专辑占位块。圆角底板 +
  音符 glyph 入静态层缓存；`onTrackChanged` 时显示**曲名首字符**（等宽粗体，
  空曲名只显示 glyph）。
- **ElidedLabel**：曲名标签。保存完整曲名，`setFullText()` 或自身 `resizeEvent`
  时用 `QFontMetrics::elidedText(Qt::ElideRight)` 重算省略文本——窗口被拉宽
  收窄省略号都不失真。`QFontMetrics` 只出现在这里，不进任何 paintEvent。
- **TransportButton**（`QToolButton` 子类）：播放（64px，Accent 描边主键）/
  暂停 / 停止（56px 次级键）三联，图标是预建 QPainterPath（三角/双竖条/方块），
  热区 ≥48px 满足触屏。点击仍走原路径：播放 → `openSelected()`、暂停 →
  `pause()`、停止 → `stop()`。

进度条左右各一个等宽时间标签（当前 / 总时长）；`m_sliderSyncing` 防回环不变
（进度信号设滑块时不回触发 seek）。播放目录来自环境变量
`SMART_COCKPIT_MEDIA_DIR` 或系统音乐目录。音频输出固定走 ALSA（`alsasink`）。

**错误上报**：`MediaService::errorOccurred` 此前没有消费者（播放失败用户无感知）。
现在 `MediaPage` 把它加上中文前缀后以 `mediaError(QString)` 信号转发，
`MainWindow` 连到状态条：

```cpp
// MediaPage 构造函数：
connect(media, &MediaService::errorOccurred, this, [this](const QString &message) {
    emit mediaError(QStringLiteral("播放失败：") + message);
});
// MainWindow 构造函数（m_statusStrip 创建之后）：
connect(mediaPage, &MediaPage::mediaError, m_statusStrip, &QLabel::setText);
```

### 7.14 CameraService + VideoWidget + ReverseCameraPage —— 倒车影像

**[CameraService](../src/camera/CameraService.cpp)**：GStreamer `appsink` 是"把视频流引到你的程序里"的元件。回调 `onNewSampleCb` 跑在 **GStreamer 的 streaming 线程**上，这也是项目第二个线程交互点。

管线分两种（下面展示默认 1280×720、30fps，设备路径以 `/dev/video0` 为例）：

```cpp
// devicePath 为空
"videotestsrc is-live=true pattern=ball ! video/x-raw,format=NV12,width=1280,height=720,framerate=30/1 ! appsink name=sink"
// devicePath 非空
"v4l2src device=/dev/video0 io-mode=4 ! video/x-raw,width=1280,height=720,framerate=30/1 ! videoconvert ! video/x-raw,format=NV12 ! appsink name=sink"
```

**NV12 → RGBA 转换**在 streaming 线程的 `handleSample()`：NV12 是一块 Y 平面 + 一块 UV 交错平面，代码读取各平面的 stride，用 BT.601 有限范围公式逐像素生成 `QImage`。这是 **CPU 转换与新图像分配，不是 zero-copy**；`io-mode=4` 和 `QOpenGLWidget` 不会让这段 CPU 循环自动消失。当前另有像素格式缺陷：将 `0xFFRRGGBB` 整数写入 `Format_RGBA8888`，在小端机器上红蓝互换；黑白小球不验证颜色正确性，需按 [09 摄像头教程](09-camera-and-reverse.md) 增加彩色样本并统一格式，此处未修改源码。

转换完后按值捕获 image 与 generation，**queued 到 CameraService 自己的线程**，只有仍 running 且 generation 匹配才 `emit frameReady(image)`；随后当前主线程中的 `VideoWidget::setFrame` 更新图像。appsink 设置 `max-buffers=2`、`drop=true`、`sync=false` 以降低积压，但不构成端到端延迟保证。

**停流与故障**：主线程 200ms `pollStream()` 检查 ERROR/EOS，以及 2500ms 无帧超时；故障路径先 `stop()`，再发 `cameraError`。`stop()` 停 timer、增加 generation、解除 callback、把管线置 NULL 并释放引用，最后发空 `QImage`，使界面显示 **NO SIGNAL** 而不是残留旧画面。generation 检查同时挡住 stop 和 stop/restart 之后仍排队的旧流帧。失败后可以再次显式 `start()`，**没有自动重连，也没有失败后切换测试源**。

> 说明：摄像头在 `main.cpp` 中启动时就尝试 `start()`，不以是否倒车为条件；倒车只负责切页。持续运行旨在减少切入时的启动等待，但画面是否可用、CPU/功耗和实际延迟须在目标板验证，不能承诺"挂 R 档必定立即有画面"。

**[VideoWidget](../src/camera/VideoWidget.cpp)**：继承 `QOpenGLWidget`，但没有自己编写 OpenGL 着色器，而用 `QPainter` 在 GL 表面画 `QImage` 并叠加轨迹线；不等于底层完全不用 shader，也不是摄像头零拷贝。要点：
1. **画面缩放**（`paintGL`）：按控件宽度和源宽高比算目标矩形，再与控件区域取交集后 `drawImage`。常见竖屏布局可保留比例，但高度不足时仍可能压缩画面，不能称为通用等比适配算法。
2. **线程边界**：存取 `m_frame` 用 `QMutex` 保护；当前连接在主线程调用 `setFrame`，它还会调用 QWidget 的 `update()`，所以不应从任意工作线程直接调用。
3. **轨迹线**（`drawTrajectory`）：根据方向盘角度算两条贝塞尔曲线，`shift = steerNorm * w * 0.18`（`steerNorm` 为角度/450 并限幅至 ±1），打得越狠偏移越大——这是显示示意，不是标定过的车辆运动预测。

**[ReverseCameraPage](../src/ui/ReverseCameraPage.cpp)**：最"薄"的一个页面，只把 `frameReady` 连到 `VideoWidget::setFrame`、把 `steeringAngleChanged` 连到 `setSteeringAngle`。**页面抢占逻辑在 ScreenManager，页面不碰**。布局是"视频区（stretch 1）+ 底部条带"：条带内"倒车影像"字幕 pill 居中、48×48 红色 R 徽章（Err `#EF4444`，圆角 12）居右——条带在视频区**之外**，任何元素都不遮挡画面。样式全部走 Theme QSS（`#reversePage` / `#reverseBadge` / `#reverseCaption`）。

> GL 红线：`VideoWidget` 是 `QOpenGLWidget`，eglfs 下 GL 上下文敏感——不得改它的 parent、不得重新实例化、页面切换时不做 setParent/deleteLater。

### 7.15 AsyncLogger —— 异步日志

`qInfo()`/`qWarning()` 会被 `installQtMessageHandler()` 重定向进 [AsyncLogger](../src/logging/AsyncLogger.cpp) 的 Qt 适配层，再进入 [AsyncLoggerCore](../src/logging/AsyncLoggerCore.cpp)。日常 `log()` **不直接写盘**，而是入队：

```
调用线程 → std::mutex 加锁 → std::deque 入队 + notify_one
         → std::condition_variable 唤醒 std::thread
         → swap 出一批日志、解锁 → 批量写出 + flush → 滚动
```

好处是把常规文件写入从 UI 线程移走，但不能保证 UI "永远不卡"：入队仍有锁竞争/分配开销，`start()` 在调用线程建目录开文件，`stop()` 还要等待队列排空并 join。`rotateIfNeeded()` 在文件达到 **10 MiB** 时替换旧 `.1` 备份并新开日志，只保留一份备份。队列没有固定容量限制，高负载与停机耗时仍需评估。

---

## 8. 构建、测试、运行

### 8.1 安装依赖（Ubuntu）

教程统一使用 **Qt 5.15、C++17、CMake ≥3.21**。[CMakeLists.txt](../CMakeLists.txt) 虽声明 Qt5 ≥5.12，但当前 `Qt::hex/dec` 用法需要 Qt ≥5.14；下面的 `ctest --test-dir` 也需要 CMake ≥3.20，不能仅凭顶层最低版本声明推断兼容性。`SMART_COCKPIT_USE_QT6` 默认 OFF；Qt6 仅是可选分支，未验证其完整构建/运行兼容性，不把 Qt5 结果等同于 Qt6 验证。

```bash
sudo apt install build-essential cmake pkg-config qtbase5-dev qtbase5-dev-tools \
     libqt5dbus5 libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
     gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
     gstreamer1.0-plugins-ugly gstreamer1.0-libav \
     libqt5test5 libqt5opengl5-dev can-utils gstreamer1.0-tools gstreamer1.0-alsa \
     fonts-noto-cjk python3
```

### 8.2 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSMART_COCKPIT_USE_QT6=OFF
cmake --build build -j$(nproc)
```

也可以直接用预设：

```bash
cmake --preset Debug && cmake --build --preset Debug
```

以上命令在仓库根目录执行。[CMakePresets.json](../CMakePresets.json) 提供 Debug / Release / Cross 三套配置，preset schema v3 需要 **CMake ≥3.21**；普通 `-B build` 配置最低为 3.16。注意普通构建输出是 `build/`，Debug preset 输出是 `build/debug/`，测试与运行时不要混用目录。

### 8.3 单元测试

```bash
ctest --test-dir build --output-on-failure
```

若使用 Debug preset，改用 `ctest --preset Debug` 或 `ctest --test-dir build/debug --output-on-failure`；不要把空测试目录的输出当作全部通过。

**6 个测试可执行文件**（[注册清单](../tests/CMakeLists.txt)；UI/页面测试设置 `QT_QPA_PLATFORM=offscreen`，不需要实体屏幕）：

- **[tst_core_cxx](../tests/tst_core_cxx.cpp)**：只链接无 Qt 的 `smart_cockpit_cxx`，验证 DBC 解析/编码与异步日志落盘，是标准库层的解耦守卫；包含 Intel/Motorola 跨字节、63/64 位边界、非法布局、截断拒绝与 out 不变，直接断言线字节而非只测回环。
- **[tst_canframeparser](../tests/tst_canframeparser.cpp)**：验证 Qt 适配层的 speed/rpm/fuel、挡位、DBC 子集覆盖、编码→解码回环，以及跨字节布局、**截断帧整帧拒绝且保留 out**。
- **[tst_cameraservice](../tests/tst_cameraservice.cpp)**：用小尺寸测试源验证起停与空帧清屏；注入 EOS/ERROR、暂停流触发超时；验证 stop/restart 过滤排队旧帧，以及启动失败后可显式重试。不是实际 V4L2 摄像头、颜色准确性或板上 GL 渲染测试。
- **[tst_climateservice](../tests/tst_climateservice.cpp)**：直接向 `CarService::onCanFrame` 注入 0x200，验证 ACK 五字段匹配、拒绝不匹配/迟到 ACK、空闲 `ack=false` 同步、在途请求拒绝重入、取消 ID 校验/停 timer、stop 取消与 600ms 超时；还测缺字段守护、取消按钮使能、**250ms debounce 合并**（多次输入后只形成一次超时）。这不是物理 CAN 发包验证。
- **[tst_screenmanager](../tests/tst_screenmanager.cpp)**：正常切页、倒车(Urgent)抢占、阻挡低优先级请求；返回测试只含**一个普通活动页**，不证明多个同级请求时能回上一页。
- **[tst_ui](../tests/tst_ui.cpp)**：真实 `CarService` 驱动仪表值/挡位、空调防抖→ACK→UI、取消后新请求、过滤 0/旧 ID 通知、拒绝发送不锁 UI、stop 取消与模拟器更新。离屏内容断言仅覆盖 **InstrumentPage / ClimatePage / MediaPage 三页**，不是完整 MainWindow/倒车 GL 页的视觉验收。

> 注意 Qt 5.15 的 `QSignalSpy::wait()` 只对"wait 期间新发生的信号"返回 true（信号早已发生会返回 false），所以对同步注入的测试要直接断言 `spy.count()`，别依赖 `wait()`。
>
> 等待条件优先用 `QTRY_VERIFY(...)` / `QTRY_COMPARE(...)`（处理事件并重试，默认超时通常为 5 秒）。**`QTest::qWait()` 本来就会处理事件，QTimer 可以触发**；它适合观察一段时间内"不应发生超时"等负向断言，不应误写为纯睡眠。`QThread::msleep()` 才会阻塞当前线程而不主动处理 Qt 事件。

**验证范围**：6 套测试的名称/覆盖来自源码，不是本文更新时新执行的通过报告。ASan/UBSan 只说明被实际运行路径上的内存/未定义行为检查，不能代替线程竞争检测或协议正确性证明；native 主机 UI/离屏渲染也不等于 RK3566 板测。实际 V4L2、SocketCAN、ALSA、eglfs/KMS、触摸与持续性能须另记设备环境和实测结果。

### 8.4 CI / 格式

- **GitHub Actions**：当前工作树未找到 `.github/workflows/ci.yml`，不能承诺 push / PR 会自动跑 CI；即使 [scripts/ci.sh](../scripts/ci.sh) 注释提到该文件，也要先核对实际工作流是否存在。
- **本地/云服务器门禁脚本**（不依赖 GitHub）：`sudo ./scripts/ci.sh` 会执行 apt 更新/安装，再配置 `build/ci`、构建、ctest。它要求 root 并修改主机环境，不是无副作用的检查；依赖已齐时可按 8.2/8.3 手工运行构建与测试。
- **代码格式**：当前工作树没有 `.clang-format`，不能称为已固定项目风格；clang-format 可能读取父目录配置或使用默认风格。[scripts/format.sh](../scripts/format.sh) 默认就地改源码，`./scripts/format.sh --check` 才是检查。仅更新文档时不应顺便格式化其他文件；是否阻断 CI 取决于实际工作流配置。

### 8.5 主机运行

```bash
# 创建虚拟 CAN（会修改主机网络接口，需要 sudo）
./scripts/vcan-setup.sh
# 先运行接收端；默认摄像头参数为空，使用测试源
./build/smart-cockpit --can vcan0
# 接收端运行后，在另一个终端发送一轮示例帧
./scripts/demo-can.sh
```

效果：[demo-can.sh](../scripts/demo-can.sh) 的 0x100 让仪表显示 120km/h / 3000rpm / 50% 电量 + 左转向灯；0x300 的 bit0 请求倒车页，0x400 设置 +300° 轨迹线。脚本**只发一轮，不是持续信号源**，3 秒后 CAN 时间戳会过期。示例 `0x01` 只设置倒车 bit，PRND 位仍为 P；模拟器用的 `0x03` 才同时表示 R 标签与倒车。

倒车激活时 F1/F2/F3 普通请求会被拒绝；需后续 0x300 清除倒车 bit 才能退出。脚本中固定的 0x200 ACK 也只在已有匹配 target 的 pending 请求时有效，不能确认任意空调命令。使用 Debug preset 时可执行文件路径改为 `./build/debug/smart-cockpit`。

**没有 CAN/vcan 也能看效果**（例如板子内核缺 vcan 模块、或只想快速演示）：

```bash
./build/smart-cockpit --sim
```

显式选择后会跑 60 秒一圈的"加速→巡航→刹停→倒车→停车"演示（45..52 秒倒车），驱动仪表/挡位/倒车切页；不包含空调 ACK 或真实硬件验证。

> 离屏截图验证 UI：用 `QT_QPA_PLATFORM=offscreen` 跑一个临时程序，创建服务 + 页面后 `widget->grab()` 存 PNG（本仓库不包含此工具，属调试技巧）。

---

## 9. 交叉编译与部署

### 9.1 准备

> **先核对，勿直接一键部署。** 当前交叉工具链和部署文件存在下列阻断项/设备假设；本文只解释现状，不表示已修好或已板测。完整检查顺序见 [12-cross-build-and-deployment.md](12-cross-build-and-deployment.md)。

- 把泰山派匹配的 Debian rootfs 放到 `$SDK_SYSROOT`（内含目标架构头文件、库、`usr/lib/aarch64-linux-gnu/pkgconfig`、GStreamer 插件等）。
- 安装 `g++-aarch64-linux-gnu`，确认实际编译器名称为 `aarch64-linux-gnu-g++`。
- 可选：Qt 不在 sysroot 里时，设 `$SDK_QT_ROOT` 指向目标架构 Qt5；Qt 构建时使用的主机工具也需匹配，不能混用 x86 与 aarch64 库。

### 9.2 交叉编译

[工具链文件](../cmake/toolchain-aarch64-linux-gnu.cmake) 当前写成 `TOOLCHAIN_PREFIX=aarch64-linux-gnu`，拼 `${TOOLCHAIN_PREFIX}gcc`/`g++` 后少了连字符，得到不存在的常规工具名 `aarch64-linux-gnugcc`/`aarch64-linux-gnug++`。正确名称需要末尾 `-`，**需另行修正工具链，本次文档更新没有修改该文件**。

下列是完成工具链核对后的构建流程，不是当前文件无需修改即可执行成功的承诺：

```bash
export SDK_SYSROOT=/opt/taisanpi/sysroot
export SDK_QT_ROOT=/opt/taisanpi/qt5
cmake --preset Cross -DSMART_COCKPIT_USE_QT6=OFF
cmake --build --preset Cross -j$(nproc)
```

`SDK_QT_ROOT` 可选，路径均须换成本机实际路径。[Cross preset](../CMakePresets.json) 输出在 **`build/arm/`** 且关闭测试；手动 `cmake -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-linux-gnu.cmake -DSMART_COCKPIT_USE_QT6=OFF -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release` 则输出到 **`build-arm/`**，不是同一个目录。[deploy.sh](../deployment/deploy.sh) 默认读后者，不能把两种路径混用。

工具链的关键概念仍是：`CMAKE_SYSROOT` 指向 rootfs；`CMAKE_FIND_ROOT_PATH_MODE_LIBRARY/INCLUDE/PACKAGE=ONLY` 查目标依赖，而 **PROGRAM=NEVER** 让构建时工具在主机查找；`PKG_CONFIG_LIBDIR` 指向 aarch64 的 `.pc`，`PKG_CONFIG_SYSROOT_DIR` 给路径加 sysroot 前缀。意图是隔离主机库与目标库，实际还须检查环境变量、绝对路径与所选包，不能只凭这几个设置宣称绝无混库。

### 9.3 部署

**不要直接照抄 `deployment/deploy.sh` 一键部署命令。** 脚本会用 rsync 覆盖二进制、systemd 单元和 KMS 配置，随后 `daemon-reload`、`enable`、`restart` 服务；这会立即改变目标设备运行状态。先按 [交叉构建与部署教程](12-cross-build-and-deployment.md) 完成路径、权限、显示设备、CAN/摄像头配置、依赖和回退方案核对。

当前模板要特别检查：

- [deploy.sh](../deployment/deploy.sh) 未先创建 **`/etc/smart-cockpit`** 就向其中复制 KMS JSON；它只创建 `/var/log/smart-cockpit`，新设备上复制可能直接失败。
- [smart-cockpit.service](../deployment/smart-cockpit.service) 用 **root** 运行，设 `QT_QPA_PLATFORM=eglfs` 并自动重启整个进程。最小权限、设备节点权限与实际启动条件需单独设计。
- [kms-config.json](../deployment/kms-config.json) 硬编码 **`/dev/dri/card0`、`HDMI-A-1`、`1280x800@60`**，并非本文目标竖屏的通用配置；须以板上实际 DRM connector/mode 为准。
- `ExecStart` 只传 **`--can can0`，没有 `--camera`**，因此仍使用测试源，不是真摄像头。`SMART_COCKPIT_CAN=can0` 环境变量当前也未被 `main.cpp` 读取，实际接口由命令行决定。systemd 的进程重启不等于 camera 流故障自动重连。

设备侧依赖包括 `libqt5widgets5 libgstreamer1.0-0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good systemd fonts-noto-cjk`、可用的 eglfs/KMS/EGL 驱动，以及实际音频格式所需的 ugly/libav 等插件与 ALSA 支持。中文需安装 `fonts-noto-cjk` 或等效 CJK 字体，否则可能显示方块。这只是依赖类别参考，不是适用于所有板卡镜像的完整 apt 清单。

---

## 10. 新手常见问题排查

| 现象 | 原因 / 解法 |
| --- | --- |
| 编译报错 `qHash` 找不到 | `enum class` 当 `QHash` 的 key，Qt5 需要自己在枚举所在命名空间写 `qHash` 重载（看 `SystemState.h` 结尾） |
| 编译报错 `signals` 是保留字 | Qt 把 `signals` 定义为宏（展开成 `public`），**变量名别叫 signals**，改叫 `vals` |
| 编译报错 `QThread::start(lambda)` | `start()` 不是接收 lambda 的 API（也不能把它当作 Qt6 写法）；可用 `QThread::create`（Qt ≥5.10）或 `std::thread`，本项目 AsyncLoggerCore 用后者 |
| 链接报错 `undefined reference to gst_video_*` | CMake 缺 `gstreamer-video-1.0`，`pkg_check_modules` 里加上 |
| 启动报 `interface vcan0 not found` | 没建 vcan0，先核对 `scripts/vcan-setup.sh`（需 sudo）；也可显式选 `--sim`。CAN 失败不会自动模拟，默认模式会无数据并报健康异常 |
| `QSocketNotifier: Can only be used with threads started with QThread` | 检查 notifier 所在线程是否有 Qt 事件分发器，结合调用栈/插件定位；不能无条件当作无害警告忽略 |
| 仪表不动 | 检查 `candump vcan0` 有没有帧；帧 ID 对不对（0x100）；DBC 位序对不对 |
| 空调发指令没反应 | 先等 250ms debounce，再查 `Climate command <id> sent` 和 CAN 发送警告；这条日志不等于 write 成功。0x200 要 ack=1 且五字段匹配 pending target，600ms 超时回滚；`--sim` 不发 ACK（见 7.5/7.12） |
| 「取消待处理」按钮是灰的 | 正常现象：它只在有指令等待车辆确认（pending）时才使能，ACK 或超时后自动禁用 |
| 倒车不抢占页面 | 0x300 byte0 bit0 必须为 1；`0x03` 同时设置 R 挡标签，`0x01` 只置倒车 bit。看日志 `Reverse engaged`；主窗口顶栏/导航仍保留 |
| 倒车画面显示 NO SIGNAL | stop、EOS/ERROR 或 2500ms 无帧超时会清空画面；查 camera 错误与设备/管线。非空真实路径失败不会回退测试源，需排障后显式重新 start（或重启应用） |
| 退出倒车没有回到上一页 | 同级活动请求在 QHash 中无最近次序，当前不能保证返回顺序；不是已修复功能（见 7.7） |
| 中文字体方块 | 装 `fonts-noto-cjk`（与平台插件无关，本板在 xcb 下同样需要） |
| 仪表刻度数字只显示中间 | 早期版本用固定矩形画刻度数字会裁边；现用 `QFontMetricsF` 计算真实文本矩形（在静态层缓存里绘制，见 7.10） |
| 温度圆表太小 / 被裁切 | 表盘按 `min(w,h)` 取居中正方形，`sizeHint()` 返回 320×320。注意：加 `Qt::AlignCenter` 对齐标志会阻止控件拉伸、锁死在最小尺寸 |
| 媒体页曲库是空的 | 本机音乐目录没有 mp3/flac/wav/ogg。用 `--media-dir 你的目录` 指定，或往 `~/Music` 放两个文件 |
| 状态条显示"播放失败：…" | `MediaService` 的 GStreamer 错误经 `MediaPage::mediaError` 转发到状态条（见 7.13），检查文件是否损坏、设备上是否装了 `gstreamer1.0-plugins-ugly/libav` |
| 某些图标显示成方块 □ | 导航/指示灯的 Unicode 字符（◉❄◀）在极老字体下缺字形，装 `fonts-noto-core`；空调模式键与播放控制键是 QPainter 路径，与字体无关。项目不依赖任何图片资源 |
| 系统状态出现 Emergency | 检查 `canConnected()` 与最近帧时间戳，3 秒无帧或未连接就会触发；`--sim` 只替代连接判断，仍依赖模拟器持续更新时间戳 |
| 板子内核没有 CAN/vcan，HMI 怎么演示 | 用 `./smart-cockpit --sim`：内置信号模拟器直接驱动仪表/挡位/倒车，不依赖 CAN 模块；不代表真实 CAN/摄像头已验证 |
| 鼠标键盘没反应 | eglfs 可支持输入，检查 libinput/evdev 支持、设备节点权限及环境；桌面主机可用已安装的 xcb/Wayland 后端调试。linuxfb 不能等价验证依赖 GL 的倒车页 |

**尚未修复的已知项至少包括**：同优先级导航返回、循环风无命令、pending 时温度 `−`/`+` 未禁用、扩展 CAN ID 发送截断、媒体换曲状态流程、媒体无 bus 消息时进度不刷新。细目和练习见 [14-known-issues-and-exercises.md](14-known-issues-and-exercises.md)，不能把本文的保护机制或测试覆盖理解为全部 bug 已修好。

---

## 11. 如何扩展这个项目

### 11.1 增加一个新的 CAN 帧（比如 0x500 车门状态）

1. **[CanTypes.h](../src/core/CanTypes.h)**：在 `FrameId` 中加 `constexpr std::uint32_t Doors = 0x500;`，保持无 Qt 依赖。
2. **[CanFrameParserCore.cpp](../src/core/CanFrameParserCore.cpp)** 的 `installBuiltinTable()`：加一组 `CanSignalDef`；不要把内置表加回 Qt 适配层。
3. **[CarService](../src/core/CarService.cpp)**：加状态成员 + `handleFrame500()` + 对应信号；在 `onCanFrame` 的 switch 加 case；把新信号名加进该处理器的 `hasAllSignals` 键列表。
4. **UI**：在 [InstrumentPage](../src/ui/InstrumentPage.cpp) 加一个 `QLabel`，连上新信号，沿用 Theme 视觉体系。
5. **[config/vehicle.dbc](../config/vehicle.dbc)**：加 `BO_`/`SG_` 行，并明确字段单位、位序与语义；不擅自改变既有协议。
6. **[demo-can.sh](../scripts/demo-can.sh)**：加相应 `cansend` 示例；同时补纯 C++ 编解码线字节、截断 out 保留与服务/UI 更新测试。

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

解析器只支持 `BO_`/`SG_` 子集，逐帧覆盖内置表。**信号名齐全只是必要条件**：还须对齐布局、缩放、单位和业务语义，检查范围/数值输入；`CarService` 的 0x300 仍直接读裸字节。加载中途失败不会自动恢复原表，`main.cpp` 也只记录警告后继续启动，不应把失败配置用于真实车辆。吹风枚举与 DBC 注释差异需先统一（见 7.12），再用已知报文逐项验证，不是任意 DBC 即插即用。

---

## 附：一句话总结每个类

| 类 | 一句话 |
| --- | --- |
| `CanFrameParserCore` | 纯 C++ DBC 子集编解码，严格位布局边界，整帧 decode 失败保留 out |
| `CanFrameParser` | QString/QByteArray/QHash 与纯 C++ core 之间的适配层 |
| `CanManager` | 一个线程 + 一个 socket，收帧发帧 |
| `CarService` | 帧 → 语义化信号的中枢，带信号存在性守护，也是空调指令的出口 |
| `HealthMonitor` | 每 2 秒体检，输出 绿/橙/红（CAN 按 3 秒新鲜度判定） |
| `ScreenManager` | 页面优先级抢占；同级请求的返回顺序尚不保证 |
| `Theme` | 设计 token + 全局 QSS + 等宽数字字体，`main` 里应用一次 |
| `MainWindow` | 顶栏 + 页面栈 + 底部导航 + 状态条 + 快捷键 |
| `GaugeWidget` | 静态层缓存的 QPainter 圆表（值弧 + 红区 + 旋转指针） |
| `BatteryWidget` | 静态层缓存的电池电量条（≤30% 黄 / ≤15% 红） |
| `InstrumentPage` | 竖屏三段：车速 / 转速 / 状态卡（挡位 + 电量 + LampWidget 指示灯） |
| `ClimatePage` | 温度圆表 + 风量 + 图标模式键 + "debounce→发指令→等确认→超时回滚，可取消" |
| `MediaPage` | 播放卡（AlbumArt/ElidedLabel/TransportButton）+ 列表 + mediaError 上报 |
| `MediaService` | GStreamer/ALSA 播放封装；换曲、进度仍有已知缺陷 |
| `SignalSimulator` | `--sim` 的 100/300/400 信号源，60 秒一圈，无 200 ACK |
| `CameraService` | appsink CPU 转图、queued generation 检查、故障清帧，无自动重连 |
| `VideoWidget` | GL 表面上的 QPainter 图像、NO SIGNAL 与示意轨迹线 |
| `AsyncLoggerCore` | 标准库互斥队列 + 写盘线程，批量写入/滚动/退出排空 |
| `AsyncLogger` | Qt 日志消息与纯 C++ 日志核心的适配层 |
