# 13 · 从空目录逐步组装完整项目

配套：[本章练习题](exercises/13-build-from-zero.md) · [全部题集](exercises/README.md)

[学习路线](README.md) · 上一章：[交叉编译与部署](12-cross-build-and-deployment.md) · 下一章：[综合练习与验收](14-known-issues-and-exercises.md)

## 本章的用法

这是贯穿 01～12 的施工清单，而不是要求你最后一天一次性写完所有文件。每个里程碑都应“能编译、能说明输入输出、能运行至少一个实验”。**不要先创建几十个空类，再把无法编译的工程叫作完成。**

02、03、06 的完整小程序是独立练习；下面重建工程使用真实模块名和公共接口。参考仓库是可以随时查阅的完整实现，不是要你背下来；先按步骤自己写，再对照差异。章节里的摘录不能代替完整 `.cpp`。

## 1. 创建自己的工程，保留参考仓库

```bash
PROJECT=/home/air/qt/Smart_Cockpit_based_on_tspi
WORK="$HOME/smart-cockpit-lab"
ls "$PROJECT/src"
mkdir -p "$WORK/cockpit/src/core" "$WORK/cockpit/src/ui" \
  "$WORK/cockpit/src/camera" "$WORK/cockpit/src/media" \
  "$WORK/cockpit/src/logging" "$WORK/cockpit/tests" \
  "$WORK/cockpit/config"
```

如果已有 `WORK/cockpit`，先检查，不覆盖既有代码。源码写在 `cockpit`，构建目录使用 `WORK/build-cockpit`；不要直接修改参考项目来替代从零练习。

以下模块除明确指出的 `.h` 外，均指同名 `.h/.cpp`。文件名和类名保持一致可以直接复用参考测试；自己改接口也可以，但必须同步调用和测试。

## 2. 里程碑 A：只用 C++ 解释字节

学习 [04](04-can-and-dbc.md)，依次实现：

1. [CanTypes.h](../src/core/CanTypes.h)：用 `std::uint32_t` 定义四个帧 ID；用结构体描述名称、起始位、长度、字节序、符号、系数和单位。不要引入 Qt。
2. [CanFrameParserCore](../src/core/CanFrameParserCore.h)：先写内置信号表，再写单信号解码/编码、整帧接口，最后实现 DBC 子集读取。
3. 先验证 Intel `7|2` 与 Motorola `7|16` 的**确切字节**；加入短帧、负起始位、越过第 8 字节等失败用例。
4. 实现“先校验所有字段，再改输出”，保证一个坏字段不会把半帧状态显示给用户。

此阶段先用 04 的独立 g++ 实验编译自己的核心实现，不需要为了测字节先写完整窗口。

**完成证据**：能手算 `0x12 0x34` 的两种字节序解释；测试拒绝越界而输出仍保持原值。encode→decode 互相通过不算足够证据。

## 3. 里程碑 B：让 Qt 收到有意义的车辆状态

学习 [05](05-vehicle-service-and-simulator.md)，实现顺序：

| 文件 | 先写什么 | 再写什么 |
| --- | --- | --- |
| [SystemState.h](../src/core/SystemState.h) | 页面/系统枚举，ClimateState 默认值 | 完整比较、Qt 元类型与枚举哈希 |
| [CanFrameParser](../src/core/CanFrameParser.h) | Qt 容器与 std 容器转换 | 将解析工作委托给 Core，不复制一套算法 |
| [CanManager](../src/core/CanManager.h) | socket 创建、索引查询、绑定、失败释放 | run 的 select/read、发送、退出等待 |
| [CarService](../src/core/CarService.h) | 状态 getter 与信号，构造/析构 | onCanFrame 分发、字段完整性、限幅与变化通知 |
| [SignalSimulator](../src/core/SignalSimulator.h) | QTimer 起停 | 编码 100/300/400 并喂入同一入口 |

此时可以临时用一个 `QLabel` 订阅 `speedChanged`，像 03 那样先看数字，不必提前实现最终仪表。`CarService` 构造依赖 `CanManager` 的实现，但用 `setSimulatedMode(true)` 和模拟器时不需要打开 CAN socket。

先实现读取链路，空调发送接口可在下一里程碑加入；不要在最终交付里保留假返回值。若提前声明尚未实现的方法，不调用它，也不声称相应功能已经完成。

**完成证据**：同一帧注入两次不重复产生无意义变化信号；缺失字段不会把显示归零；R 挡原始字节 `03` 能被解释为倒车。

## 4. 里程碑 C：构造真实的页面和空调闭环

先做 [06](06-widgets-and-dashboard.md) 的自绘实验，再写：

1. [Theme](../src/ui/Theme.h)：集中颜色、间距、字体，先能返回一份有效 QSS。
2. [GaugeWidget](../src/ui/GaugeWidget.h)：范围、数值更新、坐标换算、绘制；正确后再把静态部分缓存。
3. [BatteryWidget](../src/ui/BatteryWidget.h)：百分比限幅、阈值颜色和尺寸改变后的缓存失效。
4. [InstrumentPage](../src/ui/InstrumentPage.h)：布局与信号连接；不要把 CAN 解码复制进页面。
5. [ScreenManager](../src/ui/ScreenManager.h)：页面注册、请求、优先级判断、撤销。普通页面先用空 QWidget 测试抢占，不急着组装 MainWindow。
6. 按 [07](07-climate-command-flow.md) 完成 `CarService` 的 `sendClimateCommand`、`cancelClimateCommand` 和 0x200 回执处理。
7. [ClimatePage](../src/ui/ClimatePage.h)：先实现 `readControls/applyControls`，再加 250ms 防抖，最后实现 requestId 过滤与三条结束路径。

**完成证据**：成功确认、超时、取消都使页面回到可用状态；取消 24℃ 后请求 28℃，24℃ 的旧回执不能确认新请求。同目标旧回执不可区分要写明是协议限制。

不要把当前参考实现中的导航同级返回问题、循环风按钮或未完全禁用的温度按钮当成推荐设计；14 给出了下一步改进题目。

## 5. 里程碑 D：外设不只要能启动，还要能停

按 [08](08-media-playback.md)、[09](09-camera-and-reverse.md) 实现：

| 文件 | 最小成功路径 | 必须同时理解的失败路径 |
| --- | --- | --- |
| [MediaService](../src/media/MediaService.h) | init、打开本地音频、播放/暂停、seek | 错误消息、EOS、换 URI 的状态条件 |
| [MediaPage](../src/ui/MediaPage.h) | 扫描文件、选择、操作、显示时长 | 空目录、不可解码文件、进度与标题不一致 |
| [CameraService](../src/camera/CameraService.h) | 测试源→NV12→QImage→主线程信号 | EOS、ERROR、无帧超时、部分启动失败、停止后旧帧 |
| [VideoWidget](../src/camera/VideoWidget.h) | 显示图像、画辅助轨迹 | 空图 NO SIGNAL、尺寸变化、真实 GL 环境 |
| [ReverseCameraPage](../src/ui/ReverseCameraPage.h) | 连接图像与转角 | 流停止后不残留看似实时的画面 |

不要靠先忽略析构和 stop 来获得一个“看起来能运行”的版本；外设生命周期就是功能的一部分。CameraService 的 generation 必须跨停止/重启改变，而不是只检查布尔 running。

**完成证据**：测试源有画面，故障后旧画面消失，再次 start 可出新画面；媒体缺陷与实际播放状态能够通过实验识别。当前参考播放器仍有待修问题，见 08/14。

## 6. 里程碑 E：把全系统连接起来

学习 [10](10-logging-and-health.md)：

1. [AsyncLoggerCore](../src/logging/AsyncLoggerCore.h)：start→入队→等待/批量写→停止并 join，先做单条消息可靠排空测试，再考虑轮转。
2. [AsyncLogger](../src/logging/AsyncLogger.h)：Qt 字符串和消息处理器适配，不再创建第二套写盘队列。
3. [HealthMonitor](../src/core/HealthMonitor.h)：时间戳、连接/运行状态、优先级判定、信号输出。
4. [MainWindow](../src/ui/MainWindow.h)：构造四页并注册、顶栏/导航/状态条、倒车请求与撤销、系统状态展示。
5. 实现 `src/main.cpp`，遵循下一节的组装顺序。

此时再把前面的临时 QLabel 演示入口替换为真实 MainWindow，不让一堆临时开关留在最终主程序里。

## 7. 最小集成入口：完整文件

以下是**学习工程的 `src/main.cpp` 完整内容**，用于模块齐全后的集成检查。它固定使用模拟器和测试视频、普通窗口，**不是参考工程全部命令行功能的替代品**；不需要 CAN、不接受 `--camera` 等自定义参数。

```cpp
#include <QApplication>
#include <QDir>
#include <QImage>
#include "AsyncLogger.h"
#include "CameraService.h"
#include "CarService.h"
#include "HealthMonitor.h"
#include "MainWindow.h"
#include "MediaService.h"
#include "SignalSimulator.h"
#include "SystemState.h"
#include "Theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("cockpit-lab"));
    QApplication::setStyle(QStringLiteral("Fusion"));
    app.setStyleSheet(sc::Theme::styleSheet());
    qRegisterMetaType<QImage>("QImage");
    qRegisterMetaType<sc::SystemState>("sc::SystemState");
    qRegisterMetaType<sc::ClimateState>("sc::ClimateState");
    sc::AsyncLogger::instance().start(QDir::homePath() + "/.local/state/cockpit-lab");
    sc::installQtMessageHandler();

    sc::CarService car;
    sc::MediaService media;
    sc::CameraService camera;
    sc::HealthMonitor health(&car, &camera, &media);
    sc::SignalSimulator simulator(&car);
    sc::MainWindow window(&car, &media, &camera);
    QObject::connect(&health, &sc::HealthMonitor::stateChanged,
                     &window, &sc::MainWindow::onSystemStateChanged);
    QObject::connect(&health, &sc::HealthMonitor::diagnosticsChanged,
                     &window, &sc::MainWindow::onDiagnostics);
    car.setSimulatedMode(true);
    window.resize(800, 1280);
    window.show();
    simulator.start();
    media.init();
    camera.start(QString(), 640, 360, 10);
    health.start();

    const int result = app.exec();
    health.stop();
    simulator.stop();
    car.stop();
    camera.stop();
    media.shutdown();
    sc::AsyncLogger::instance().stop();
    return result;
}
```

选择 640×360@10 是降低学习机的转换负载，参考服务默认是 1280×720@30。注意这个入口也不会自动确认空调命令，成功路径仍用 07 的受控回执实验验证。

## 8. 模块齐全后的完整 CMake 文件

下面是**学习工程根目录的 `CMakeLists.txt` 完整内容**。仅在上述源码文件都已实现后使用；更早的里程碑应像 03 一样只列出当前已经写好的源文件。

```cmake
cmake_minimum_required(VERSION 3.16)
project(cockpit_lab LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
option(BUILD_TESTING "Build tests" OFF)
find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets DBus)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET
    gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
add_library(smart_cockpit_cxx STATIC
    src/core/CanFrameParserCore.cpp
    src/logging/AsyncLoggerCore.cpp)
target_include_directories(smart_cockpit_cxx PUBLIC src src/core src/logging)
target_link_libraries(smart_cockpit_cxx PUBLIC Threads::Threads)
add_library(smart_cockpit_core STATIC
    src/core/CanFrameParser.cpp
    src/core/CanManager.cpp
    src/core/CarService.cpp
    src/core/HealthMonitor.cpp
    src/core/SignalSimulator.cpp
    src/logging/AsyncLogger.cpp
    src/media/MediaService.cpp
    src/camera/CameraService.cpp
    src/camera/VideoWidget.cpp
    src/ui/ScreenManager.cpp
    src/ui/Theme.cpp
    src/ui/GaugeWidget.cpp
    src/ui/BatteryWidget.cpp
    src/ui/InstrumentPage.cpp
    src/ui/ClimatePage.cpp
    src/ui/MediaPage.cpp
    src/ui/ReverseCameraPage.cpp
    src/ui/MainWindow.cpp)
target_include_directories(smart_cockpit_core PUBLIC
    src src/core src/logging src/media src/camera src/ui)
target_link_libraries(smart_cockpit_core PUBLIC
    smart_cockpit_cxx Qt5::Widgets Qt5::DBus PkgConfig::GST Threads::Threads)
add_executable(smart-cockpit src/main.cpp)
target_link_libraries(smart-cockpit PRIVATE smart_cockpit_core)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
```

编译和运行自己的工程：

```bash
cmake -S "$WORK/cockpit" -B "$WORK/build-cockpit" \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=OFF
cmake --build "$WORK/build-cockpit" -j2
"$WORK/build-cockpit/smart-cockpit"
```

## 9. 把学习入口补成参考项目功能

对照 [参考 main.cpp](../src/main.cpp)，亲手加入：

1. `QCommandLineParser`，定义 `--can/--camera/--dbc/--log-dir/--media-dir/--sim`。
2. 解析后启动日志，传入用户选择的可写目录。
3. 外部 DBC 在服务创建后、开始接收数据前加载；失败要提示，不能宣称加载是事务性的。
4. `--media-dir` 设置后再构造扫描目录的页面。
5. 只有 `--sim` 分支才启动模拟器，否则 `car.start(iface)`；两条输入链不能混为一谈。
6. 将 camera 的固定测试参数改为实际命令行设备选择；理解空路径仍表示测试源。
7. 显示模式按场景选择，全屏不是 EGL/KMS 驱动设置。
8. 在 `app.exec()` 返回后停止定时器、模拟器、CAN、摄像头和媒体，最后排空日志。

加一个参数就跑一次对应场景，不要把未实现的参数写进帮助文字。可用 `diff -u` 比较自己的文件与参考文件，逐处解释差异，而不是直接整目录覆盖。

## 10. 接入测试和配置，最后才部署

按 [11](11-testing-and-debugging.md) 在 `tests/` 实现测试，写 [测试构建清单](../tests/CMakeLists.txt) 中对应目标。至少覆盖核心字节、Qt 适配、空调、页面抢占、相机生命周期和 UI 六个方面。

完成 `tests/CMakeLists.txt` 后再打开开关：

```bash
cmake -S "$WORK/cockpit" -B "$WORK/build-cockpit" -DBUILD_TESTING=ON
cmake --build "$WORK/build-cockpit" -j2
ctest --test-dir "$WORK/build-cockpit" --output-on-failure
```

把自己理解后的 DBC 写到 `config/vehicle.dbc`，核对与内置表的信号名、布局和单位一致。不要把注释里的吹风模式映射冲突当成已经统一的车辆协议。

最后按照 [12](12-cross-build-and-deployment.md) 准备目标库与部署，而不是把宿主机二进制直接复制到 ARM 板上。

## 完成标准

你应能从自己的工程目录构建运行，说明每个线程何时开始/结束，并现场演示一条正常链路与一条失败链路。未修复问题、未连接硬件以及仅做离屏测试的项目必须单独记录，不能用“和参考 UI 一样”代替工程验收。

继续 [14 已知边界、综合练习与毕业验收](14-known-issues-and-exercises.md)。
