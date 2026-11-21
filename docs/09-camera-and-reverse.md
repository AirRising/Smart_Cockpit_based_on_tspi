# 09｜从测试图像到倒车页面抢占

配套：[本章练习题](exercises/09-camera-and-reverse.md) · [全部题集](exercises/README.md)

[返回学习路线](README.md) · [上一章：媒体播放](08-media-playback.md) · [下一章：日志与健康监控](10-logging-and-health.md)

## 1. 先修、目标与安全边界

本章使用 **Qt5、C++17、GStreamer 1.0**，先学会用测试源，再考虑真实摄像头。
你应了解 QObject 信号槽、GUI 线程、Qt 事件循环，以及对象生命周期。
不需要此前练习中的教学类；下文观察器直接引用项目已有的摄像头和视频控件。

完成后应能解释并实现四条链：

- `videotestsrc/v4l2src → appsink → QImage`：一帧从哪里来。
- `流线程 → Qt 排队调用 → GUI`：为什么不能直接在回调里绘制窗口。
- `故障 → stop → 空图`：为什么宁可显示 NO SIGNAL，也不能留下过期画面。
- `reverseChanged → Urgent → ScreenManager`：倒车如何抢占普通页面。

只在主机测试源或专用实验设备上练习，不在行驶车辆上调试。
轨迹线只是示意，不是经过标定的泊车辅助，不能据此判断实际安全距离。
故障注入只解释测试进程里的做法，不提供向生产摄像头或车辆注入故障的命令。

## 2. 目录、依赖与从零实现顺序

```bash
export WORK="$HOME/smart-cockpit-lab"
export PROJECT=/home/air/qt/Smart_Cockpit_based_on_tspi
export HOST_BUILD="$HOME/build/smart-cockpit-host"
mkdir -p "$WORK/camera-viewer" "$WORK/logs" "$HOME/build"
```

准备 Qt5 Widgets、CMake、C++ 编译器、pkg-config，以及 GStreamer core/app/video 开发包。
运行时需要 videotestsrc、appsink；真实设备还需要 v4l2src 与 videoconvert 插件。
视频窗口使用 QOpenGLWidget，最终图像验收必须有可用 OpenGL 的桌面或开发板显示环境。

| 顺序 | 在 WORK 中新建/实现的文件 | 先完成的能力 |
| --- | --- | --- |
| 1 | `CameraService.h` | start/stop、状态、frameReady/cameraError 接口 |
| 2 | `CameraService.cpp` | 创建测试管线、拿到 appsink，再做释放 |
| 3 | 同一服务实现 | NV12 转换、线程排队、generation、防止旧帧回流 |
| 4 | `VideoWidget.h/.cpp` | 空图提示、绘制 QImage、转角示意线 |
| 5 | `ReverseCameraPage.h/.cpp` | 连接视频和车辆转角，布置底部提示 |
| 6 | `ScreenManager.h/.cpp` 与窗口接线 | 普通请求与 Urgent 抢占 |

每轮先用小分辨率验证，再增加功能；不要第一次就复制整个摄像头文件。
下面的完整观察器作为参照基线；自己重写时可替换练习 CMake 中的对应源文件。

## 3. 从 start 读懂管线

阅读 [CameraService.h](../src/camera/CameraService.h)、[CameraService.cpp](../src/camera/CameraService.cpp)。
`start()` 默认参数是 **1280×720、30 fps**；设备路径为空时使用 `videotestsrc pattern=ball`。
指定设备时使用 `v4l2src device=... io-mode=4`，随后 videoconvert 转为 NV12。
NV12 是亮度平面 Y 加交错 UV 平面，不能把整个缓冲区直接当作 RGB 图片。

**结构示意，非可直接运行命令；尺寸来自 start 参数：**

```text
空设备：videotestsrc is-live=true pattern=ball
        → video/x-raw,format=NV12,width=W,height=H,framerate=FPS/1 → appsink
实设备：v4l2src device=设备路径 io-mode=4
        → video/x-raw,width=W,height=H,framerate=FPS/1
        → videoconvert → video/x-raw,format=NV12 → appsink
```

启动过程按依赖顺序写：先清理旧资源，再 parse 管线，取 sink 与 bus，注册回调，切 PLAYING。
解析失败、sink 不存在、状态请求失败都应清理资源，不能留下半初始化对象。
如果服务已经 running，当前 start 直接返回 true，不会热切换新的设备或分辨率。
想换参数，先由服务所属线程 stop，再 start。

appsink 的低延迟设置：

- `max-buffers=2`：限制 appsink 内部积压的样本数量。
- `drop=true`：队列满时丢旧样本，避免持续追赶过去的画面。
- `sync=false`：不在 sink 中按时钟额外等待。
- 这些设置不等于给 Qt 排队事件也加了容量限制，更不等于端到端零延迟。

摄像头在 [main.cpp](../src/main.cpp) 启动时就调用 `camera.start()`，不是挂 R 挡才启动。
倒车只切换页面；离开倒车页也没有自动停止采集，所以服务是启动后常驻采集。
失败后会停止；当前没有自动重连机制，可由调用方修正条件后手动再次 start。
实际 MainWindow 没有摄像头重启按钮，后面的观察器专门提供这个实验入口。

## 4. handleSample：CPU 转换与线程边界

appsink 的 `new_sample` 回调运行在 GStreamer 流线程，不在 GUI 线程。
回调 pull 一个 GstSample，交给 `handleSample()`，处理完再 unref 样本。

从零实现时把一次处理拆成七步：

1. 捕获当前 `m_streamGeneration`，给这帧贴上“属于哪一轮流”的标记。
2. 从 sample 取得 buffer/caps，通过 GstVideoInfo 得到视频格式信息。
3. 用 `gst_video_frame_map()` 映射只读缓冲区。
4. 分别读取 Y/UV 平面地址与各自 stride，创建拥有独立存储的 QImage。
5. 按行、按像素在 CPU 上转换；目标行步长用 QImage::bytesPerLine()。
6. unmap 视频帧并记录最近一帧时间，不把外部 buffer 指针交给 UI 留存。
7. 向服务自己的 QObject 线程排队，校验状态和代次后才发 frameReady。

stride 是“相邻两行的字节距离”，可能包含对齐填充，不一定等于图像宽度。
UV 每两像素共享一组 U/V，每两行共享一行色度，索引与 Y 平面不同。
源码采用 BT.601 limited-range 换算，真实设备还需核对色彩空间。另有一个**尚未修复的输出格式问题**：`yuvToRgb()` 返回整数 `0xFFRRGGBB`，却通过 `quint32*` 写入 `QImage::Format_RGBA8888`；在小端机器上，内存实际成为 B、G、R、A，红蓝会互换。黑白小球暴露不了这个问题，必须增加彩色样本验证；后续修正应统一像素写入布局与 QImage 格式，例如核对 `Format_ARGB32` 的适用性，而不是只改变量名。
即便 V4L2 使用某种缓冲区共享模式，这里仍分配 QImage 并 CPU 转换，**不是零拷贝**。

**源码摘录：转换后的排队交付，非完整文件。**

```cpp
QMetaObject::invokeMethod(this, [this, image, generation]() {
    if (m_running && generation == m_streamGeneration.loadAcquire())
        emit frameReady(image);
}, Qt::QueuedConnection);
```

`this` 是服务本身，排队目标是它的 QObject 所属线程；当前 main 中就是 GUI 线程。
不要简单描述为“从流线程直接 emit 给页面”：当前是在排队 lambda 内通过校验后发信号。
按值捕获 QImage 保持图像数据有效；Qt 隐式共享不代表此前没有发生像素转换。
只检查 running 不够：stop 后立即 start，running 又为 true，旧代次仍必须被挡住。
start/stop 也应在服务所属线程调用，不要从任意工作线程直接操作它的 QTimer。

## 5. 失败与 stop：清理顺序本身就是功能

`pollStream()` 每 200 ms 检查 ERROR/EOS；没有终止消息时再检查最近帧年龄。
无帧达到 2500 ms 会超时，实际发现时间还受轮询间隔和事件循环调度影响。
启动时先记录当前时间，因此“启动后一直没有第一帧”也受超时保护。
`start()` 返回 true 不等于已经拿到第一帧，应等到非空 frameReady 再判断有图像。

发生故障时调用 stop，再发 cameraError；不会保留最后一帧假装画面仍然实时。
按源码顺序拆解 `stop()`，写自己的实现时不要只做最后的 unref：

1. 停止轮询 timer，设置 running=false。
2. 原子递增 generation，让已排队旧帧失效。
3. 用空的 `GstAppSinkCallbacks` 结构撤销回调注册。
4. 将管线切到 NULL，停止流活动。
5. 释放 appsink、bus、pipeline 引用并将成员指针置空。
6. 将 lastFrame 时间戳清零，发出空 QImage，让显示端进入 NO SIGNAL。

**源码摘录：撤销回调，不是完整 stop 实现。**

```cpp
GstAppSinkCallbacks callbacks {};
gst_app_sink_set_callbacks(GST_APP_SINK(m_appsink), &callbacks, nullptr, nullptr);
```

这里回调结构指针不能写成 nullptr；是传入“成员全为空的有效结构”。
后两个 nullptr 表示清空用户数据及销毁通知，不是同一层含义。
单纯断开信号不足以撤销已经排队的 lambda，因此还需要 generation 校验。
停止后 lastFrame 时间戳为 0；`lastFrameAgeMs()` 不会因此返回 0，要结合 isRunning 判断。
stop 可重复调用，也会重复发空图，接收端应允许这种幂等清屏行为。

## 6. 完整例子：只看测试源的最小窗口

以下两个文件构成**完整观察器例子**，引用真实 CameraService/VideoWidget，不依赖 CarService。
默认只启动空设备测试源；本例选择 640×360@15 以减轻主机 CPU 压力，不改变服务默认值。

**完整文件：`$WORK/camera-viewer/CMakeLists.txt`**。

```cmake
cmake_minimum_required(VERSION 3.16)
project(camera_viewer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET
    gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
if(NOT DEFINED COCKPIT_SOURCE)
    message(FATAL_ERROR "Pass -DCOCKPIT_SOURCE=/absolute/project/path")
endif()
add_executable(camera-viewer main.cpp
    "${COCKPIT_SOURCE}/src/camera/CameraService.cpp"
    "${COCKPIT_SOURCE}/src/camera/CameraService.h"
    "${COCKPIT_SOURCE}/src/camera/VideoWidget.cpp"
    "${COCKPIT_SOURCE}/src/camera/VideoWidget.h")
target_include_directories(camera-viewer PRIVATE "${COCKPIT_SOURCE}/src/camera")
target_link_libraries(camera-viewer PRIVATE Qt5::Widgets PkgConfig::GST)
```

**完整文件：`$WORK/camera-viewer/main.cpp`**。

```cpp
#include <QApplication>
#include <QDebug>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>
#include "CameraService.h"
#include "VideoWidget.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    sc::CameraService camera;
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *video = new sc::VideoWidget(&window);
    video->setMinimumSize(640, 360);
    auto *status = new QLabel(QStringLiteral("测试源：无真实摄像头"), &window);
    auto *steering = new QSlider(Qt::Horizontal, &window);
    steering->setRange(-900, 900);
    auto *start = new QPushButton("Start test source", &window);
    auto *stop = new QPushButton("Stop / clear", &window);
    layout->addWidget(video, 1);
    layout->addWidget(status);
    layout->addWidget(steering);
    layout->addWidget(start);
    layout->addWidget(stop);
    QObject::connect(&camera, &sc::CameraService::frameReady,
                     video, &sc::VideoWidget::setFrame);
    QObject::connect(&camera, &sc::CameraService::cameraError, &window,
                     [status](const QString &error) {
                         status->setText(error);
                         qWarning() << error;
                     });
    QObject::connect(steering, &QSlider::valueChanged, video,
                     [video](int angle) { video->setSteeringAngle(angle); });
    auto begin = [&] {
        status->setText(QStringLiteral("请求启动测试源"));
        qInfo() << "start:" << camera.start(QString(), 640, 360, 15);
    };
    QObject::connect(start, &QPushButton::clicked, &window, begin);
    QObject::connect(stop, &QPushButton::clicked, &camera, &sc::CameraService::stop);
    window.resize(760, 560);
    window.show();
    begin();
    const int result = app.exec();
    camera.stop();
    return result;
}
```

保存后，在具备桌面与 OpenGL 的会话中执行：

```bash
cmake -S "$WORK/camera-viewer" -B "$WORK/camera-viewer/build" -DCOCKPIT_SOURCE="$PROJECT"
cmake --build "$WORK/camera-viewer/build" --parallel 2
"$WORK/camera-viewer/build/camera-viewer"
```

预期出现移动的测试球和绿色曲线；Stop 后显示 NO SIGNAL，Start 后重新出帧。
反复 Stop/Start 不应闪回旧帧；曲线在空图状态也仍会绘制，这与源码顺序一致。

## 7. VideoWidget 与倒车页面究竟做了什么

阅读 [VideoWidget.cpp](../src/camera/VideoWidget.cpp)、[ReverseCameraPage.cpp](../src/ui/ReverseCameraPage.cpp)。
VideoWidget 继承 QOpenGLWidget，但 `paintGL()` 内是 QPainter 画 QImage，不是自写 YUV shader。
它用互斥锁保护保存的图像，拿到副本后绘制；互斥锁不意味着 QWidget 可以跨线程随意调用。
空图时画 NO SIGNAL，然后仍执行轨迹绘制，不能把“绿线还在”当作信号仍正常。

当前目标矩形按宽度计算高度再与窗口求交，不能宣称所有比例都已实现居中的等比留黑。
在自己的实现中若需要严格等比居中，应单独计算 fit 矩形；本教程未修改项目绘图算法。
倒车页面的标题与 R 标记放在视频区域下面，不覆盖视频；轨迹线则画在视频之上。

轨迹归一化使用 `qBound(-1.0, steeringAngle / 450.0, 1.0)`。
拖到 -450 与 -900 应得到同样的饱和偏移，+450 与 +900 也一样；0 时左右对称。
曲线由固定比例的贝塞尔控制点生成，没有相机内参、畸变校正、轴距或实测距离标定。
不能称为真实车辆运动预测，更不能当作安全泊车功能验收。

## 8. R 挡抢占：不要承诺一定回到上一页

页面只接两类信号：CameraService::frameReady → setFrame；CarService::steeringAngleChanged → setSteeringAngle。
[MainWindow.cpp](../src/ui/MainWindow.cpp) 监听 reverseChanged，true 时请求倒车页 Urgent，false 时 dismiss。
[ScreenManager.cpp](../src/ui/ScreenManager.cpp) 阻止低于当前优先级的请求；Normal 导航不能盖掉 Urgent 倒车页。
抢占只改变显示页面，不改变摄像头启动策略，也不是音频暂停策略。

退出倒车的边界必须说清楚：

- 普通页面请求会保存在 `m_activePriorities`，切到别的普通页时旧请求并未全部清除。
- dismiss 后还有活跃请求，就选择最高优先级；只有集合为空才用 m_previous。
- 多个 Normal 同优先级时遍历 QHash，没有“最近访问顺序”记录。
- 因而退出 R **不保证返回紧邻的上一页**；m_previous 也不是完整历史栈。
- 若练习要实现“返回最近普通页”，应设计独立历史/序号或清理旧请求，并补充测试。

[tst_screenmanager.cpp](../tests/tst_screenmanager.cpp) 的返回用例只留下一个 Media 活跃请求。
它证明该简单情形，不证明“Media → Climate → R → 退出”必然返回 Climate。

## 9. 动手练习与定向测试

**练习 A：测试源和空图。** 用完整观察器反复 Start/Stop，拖动转角滑块测边界。
预期停止后没有残留球画面；超过 ±450° 曲线饱和；不需要摄像头硬件。

**练习 B：整车模拟抢占。** 在桌面上运行以下命令，使用内置模拟器，不发真实 CAN：

```bash
cmake -S "$PROJECT" -B "$HOST_BUILD" -DSMART_COCKPIT_USE_QT6=OFF -DBUILD_TESTING=ON
cmake --build "$HOST_BUILD" --target smart-cockpit --parallel 2
"$HOST_BUILD/smart-cockpit" --sim --log-dir "$WORK/logs"
```

完整构建还需 Qt5 DBus/Test；不传 `--camera` 就是空设备测试源。
模拟周期约 60 秒，45—52 秒为倒车区间；此时点普通导航应被挡住。
退出后记录实际页面，不把“必回上一页”作为当前验收条件。
模拟器在倒车段的转角为 0，曲线转向练习请用观察器滑块，别误判接线失效。

**练习 C：按测试源码理解故障，而不是破坏真实设备。**
阅读 [tst_cameraservice.cpp](../tests/tst_cameraservice.cpp)，它用 64×48@10 的临时测试源。

| 用例 | 测试进程内的方法 | 必须验证什么 |
| --- | --- | --- |
| `streamStartsAndStops` | 等第一帧，stop，再处理排队事件，重复 stop | 非空尺寸正确；停止后仍为空 |
| `terminalMessageClearsFrameAndAllowsRestart` | EOS 行向临时管线送 EOS；ERROR 行向它的 bus 投递人工错误 | 只报一次错误、停止、清空；显式 start 后恢复 |
| `stalledStreamClearsFrame` | 临时管线切 PAUSED，不再正常产帧 | 4000 ms 测试预算内触发无帧超时，再手动重启 |
| `queuedFrameCannotSurviveStop` | 构造 2×2 NV12 sample，handleSample 后立刻 stop，分立即重启/不重启两行 | 排队旧 2×2 图不能交付；重启后可收到新 64×48 图 |
| `failedStartAllowsRetry` | 使用临时目录下不存在的设备路径，再改空设备启动 | 失败资源指针归零，后续测试源可启动 |

测试使用私有成员访问来控制自己的管线，不应把这种入口加入生产 UI。
测试等待用 QTRY，让 Qt 事件循环继续交付消息；不要在 GUI 线程睡眠等待帧。
下面仅运行两个相关测试目标，不是整套测试；先理解用例再按需执行：

```bash
cmake --build "$HOST_BUILD" --target tst_cameraservice tst_screenmanager --parallel 2
ctest --test-dir "$HOST_BUILD" -R '^(tst_cameraservice|tst_screenmanager)$' --output-on-failure
```

摄像头测试使用 QTEST_GUILESS_MAIN，验证帧与生命周期而非 OpenGL 成像。
ScreenManager 的 offscreen 测试只验证页面逻辑；offscreen 通过不能证明最终视频显示正确。
要验收图像比例、颜色、轨迹与实际帧率，仍需桌面或板端目视/测量。

## 10. 排错与验收

| 现象 | 优先检查 |
| --- | --- |
| 测试源也无帧 | videotestsrc/appsink 插件、管线错误、Qt 事件循环是否运行 |
| 大约 2.5 秒后 NO SIGNAL | 启动未出帧或后续停帧；保留 cameraError，再手动重试 |
| 黑窗并提示 OpenGL context 失败 | 显示会话、Qt 平台插件、驱动；不以 offscreen 代替图像验收 |
| 图像错行或偏色 | NV12 平面、stride、色彩矩阵及 QImage 字节布局 |
| stop 后又闪回旧画面 | generation 是否捕获并校验，空图是否确实送达控件 |
| 真设备协商失败 | 在专用实验机核对支持的格式/尺寸/帧率和访问权限，先回测试源隔离问题 |
| 退出倒车回到别的普通页 | 检查 QHash 中残留的同优先级请求，不假定存在历史栈 |

验收清单：

- [ ] 可独立运行测试源观察器，能区分无设备测试与真实摄像头测试。
- [ ] 能画出流线程到服务 QObject 线程的交付过程，解释 stride 与 CPU 转换。
- [ ] 能按顺序说明 stop、空回调结构、generation、NULL/unref 和空图清屏。
- [ ] 能解释 ERROR、EOS、暂停超时、启动无帧及旧帧排队各自如何验证。
- [ ] 确认常驻采集、无自动重连，以及 Urgent 抢占/返回策略的真实边界。
- [ ] 不把 offscreen 当成最终图像验证，不把示意轨迹当成标定结果。
- [ ] 所有练习在 WORK，没有向生产设备注入故障，也没有修改项目源码。

[返回学习路线](README.md) · [上一章：媒体播放](08-media-playback.md) · [下一章：日志与健康监控](10-logging-and-health.md)
