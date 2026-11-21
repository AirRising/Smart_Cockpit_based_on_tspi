[返回学习路线](README.md) · 前一篇：[04 CAN 与 DBC](04-can-and-dbc.md) · 下一篇：[06 控件与仪表](06-widgets-and-dashboard.md)

# 05｜从原始帧到车辆状态：服务、线程与模拟器

配套：[本章练习题](exercises/05-vehicle-service-and-simulator.md) · [全部题集](exercises/README.md)

基准环境为 **Qt5 + C++17**；先写不带界面的车辆实验，区分“收到字节”“解析成功”“状态改变”。
默认路线不需要 CAN、管理员权限或开发板；虚拟总线是最后的可选实验。
本篇提供独立 QtCore 实验，再把验证过的服务接入下一章的界面。

## 1. 为什么还需要 CarService

上一章的解析器只会告诉你 `speed=300`，不负责决定仪表是否应该显示 300。
服务层负责业务限制、保存最近有效状态，再通知界面；界面不应该直接读取 CAN socket。
这相当于三个角色：收件员只收信，翻译员解释字节，业务员决定哪些结果可以采纳。

```text
SocketCAN → CanManager 接收线程 → 队列信号 → CarService 主线程
                                              ↓
                                    解析、校验、限制范围
                                              ↓
                                 speedChanged 等 → UI
```

模拟器走另一条入口：`SignalSimulator → CarService::onCanFrame()`。
它在进程内注入 payload，没有经过 Linux CAN socket。
所以“仪表在动”和“真实 CAN 链路正常”是两项不同的验收目标。

## 2. 从零写时，先定状态，再接 I/O

### 第一步：定义只读状态接口与变化信号

参考 [CarService.h](../src/core/CarService.h)：先列 speed、rpm、fuel、gearPosition、reverseEngaged、steeringAngle。
为每个值提供 getter；需要界面刷新时再发 `speedChanged()` 等信号。
初始速度、转速、油量、方向盘角均为 0，挡位为 P，倒车标志为 false。
不要让 UI 保存一份与服务相互竞争的车辆状态。
空调还多了确认状态和待处理请求，放到 [07 空调命令链路](07-climate-command-flow.md) 单独学习。

### 第二步：只写 onCanFrame 分发，不急着开 CAN

参考 [CarService.cpp](../src/core/CarService.cpp) 的 `onCanFrame()`。
下面是**设计伪代码**，不是可编译文件：

```text
收到一帧：
    对特定高位错误标记直接返回
    更新最后收帧时间；首次收帧发出连接变化通知
    ID 0x100 → handleFrame100：仪表
    ID 0x200 → handleFrame200：空调
    ID 0x300 → handleFrame300：挡位
    ID 0x400 → handleFrame400：方向盘
    其他 ID → 只记录未处理
```

方法名里的 100/200/300/400 指十六进制帧族，不是十进制 100..400。
`extended` 参数当前没有参与分发；不要据此宣称业务已完整区分扩展帧。
最后收帧时间在具体字段解析前更新，因此“最近收到帧”不等于“最近收到有效车速”。
接口公开用于诊断与测试；直接调用仍应在 CarService 所属的主线程进行。

### 第三步：先完整校验，再 clamp，最后按变化通知

`handleFrame100()` 的顺序是 `decode → hasAllSignals → qBound → compare → emit`。
以下为**源码摘录**，只能放回相应成员函数上下文阅读：

```cpp
if (!hasAllSignals(s, {"speed", "engine_rpm", "fuel_level", "left_indicator",
                       "right_indicator", "hazard", "mil_engine", "mil_abs",
                       "mil_airbag"}))
    return;
const double speed = qBound(0.0, s.value(QStringLiteral("speed")), 240.0);
```

1. `decode` 失败：如短帧，直接保留上一帧状态。
2. `hasAllSignals` 失败：如 DBC 把 engine_rpm 改名，也保留上一帧状态。
3. `qBound` 做 clamp，即把速度限制在 0..240，转速 0..8000，油量 0..100。
4. 只有值变化才 emit；double 使用 `qFuzzyCompare`，整数和布尔直接比较。

不能把 `QHash::value()` 的默认零当成“缺失字段的真实零”。
重复收到相同车速不会重复触发速度信号，这叫 change-only，而不是丢帧。
方向盘处理同样先要求 `steering_angle` 存在，再限制到 -900..900°。
空调帧也有完整字段检查，但 ACK 分支属于另一套命令状态机。

### 第四步：挡位必须拆出两个概念

当前 `handleFrame300()` **直接读取 byte0**，不通过 DBC 的 `gear_signal` 解码。
因此修改 DBC 的挡位起始位不会自动改变这段业务代码。
以下为**源码摘录**：

```cpp
const quint8 raw = static_cast<quint8>(data.at(0));
const bool reverse = (raw & 0x01) != 0;
const int position = qBound(0, (raw >> 1) & 0x07, 3);
```

进入前已有 `data.isEmpty()` 检查；一字节足够，实验统一发送 8 字节以便阅读。
bit0 是“倒车已接合”；bits1..3 才是 P/R/N/D 编码，两者不要混为一谈。

| byte0 | 二进制低四位 | position | reverse |
| --- | --- | --- | --- |
| `00` | `0000` | 0 = P | false |
| `03` | `0011` | 1 = R | true |
| `04` | `0100` | 2 = N | false |
| `06` | `0110` | 3 = D | false |

`01` 只置倒车位，position 仍为 P；它不等价于完整的 R 编码 `03`。
position 超过 3 会被 clamp 到 3；服务不会自动纠正所有矛盾的倒车组合。

## 3. 再接收帧线程：不要把 QThread 对象当作工作线程

参考 [CanManager.h](../src/core/CanManager.h) 与 [CanManager.cpp](../src/core/CanManager.cpp)。
按 `open → run → sendFrame → closeBus` 的顺序阅读并在练习副本实现。
`open()` 建立 raw CAN socket，查询接口索引，bind，然后调用 `start()`。

- `CanManager` 是 QThread 子类，但这个 **QObject 对象仍属于创建它的主线程**。
- 只有重写的 `run()` 在接收线程执行；普通方法不会因为写在 QThread 子类里自动切线程。
- `run()` 使用 `select()`，超时是 100ms，以便周期性检查中断请求。
- 可读时执行 `read()`，增加接收计数，然后发出 `frameReceived`。
- CarService 构造函数明确使用 `Qt::QueuedConnection`，槽在接收者的主线程运行。
- 主线程的事件循环不能被长时间阻塞，否则排队帧和计时器都要等待。

以下为 [CarService.cpp](../src/core/CarService.cpp) 的**源码摘录**：

```cpp
connect(m_can, &CanManager::frameReceived,
        this, &CarService::onCanFrame, Qt::QueuedConnection);
```

`closeBus()` 请求中断、最多等待 1000ms，再关闭 socket。
当前 mutex 只包住发送时的 `write()`，不是整个对象的自动线程安全保证。
例如接收计数是普通成员，getter、socket 生命周期没有统一受这把锁保护。
不要随意从任意线程同时 open/close/send，或把诊断计数当作严格同步的统计接口。

**已知边界：扩展 ID 发送未修复。**
当前 `sendFrame()` 无论 extended 是否为 true，都先执行 `id & CAN_SFF_MASK`。
随后仅补 `CAN_EFF_FLAG`，并没有保留完整 29 bit ID。
本篇只使用 `vcan0` 上的标准帧，不提供扩展帧“已支持”的演示。

## 4. 动手：一个 QtCore 就能运行的车辆服务实验

先定义变量并创建仓库外目录；不复制或覆盖仓库代码。

```bash
PROJECT=/home/air/qt/Smart_Cockpit_based_on_tspi
HOST_BUILD="$HOME/build/smart-cockpit-host"
WORK="$HOME/smart-cockpit-lab"
mkdir -p "$WORK/05"
cmake --version
pkg-config --modversion Qt5Core
```

用编辑器保存 `$WORK/05/CMakeLists.txt`，以下是该文件的**完整内容**。
`PROJECT_PATH` 只是只读源码位置；`AUTOMOC` 为带 `Q_OBJECT` 的类生成元对象代码。

```cmake
cmake_minimum_required(VERSION 3.16)
project(vehicle_lab LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt5 5.15 REQUIRED COMPONENTS Core)
if(NOT DEFINED PROJECT_PATH)
    message(FATAL_ERROR "Pass -DPROJECT_PATH=/absolute/project/path")
endif()
set(CORE "${PROJECT_PATH}/src/core")
add_executable(vehicle_lab
    main.cpp
    "${CORE}/CanFrameParserCore.cpp" "${CORE}/CanFrameParser.cpp"
    "${CORE}/CanManager.cpp" "${CORE}/CarService.cpp" "${CORE}/SignalSimulator.cpp"
    "${CORE}/CanManager.h" "${CORE}/CarService.h" "${CORE}/SignalSimulator.h"
)
target_include_directories(vehicle_lab PRIVATE "${CORE}")
target_link_libraries(vehicle_lab PRIVATE Qt5::Core)
```

保存 `$WORK/05/main.cpp`，以下是**完整可运行入口**；使用运行时检查，不依赖 assert 开关。
默认先注入边界帧，再让真实 SignalSimulator 运行约 1.2 秒。
`--vcan` 分支只接收已有 `vcan0`，不会创建接口，也不会发送帧。

```cpp
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include "CarService.h"
#include "SignalSimulator.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    sc::CarService car;
    int changes = 0;
    QObject::connect(&car, &sc::CarService::speedChanged, &app,
                     [&](double speed) { ++changes; qInfo() << "speed" << speed; });
    QObject::connect(&car, &sc::CarService::gearChanged, &app,
                     [](int gear) { qInfo() << "gear" << gear; });
    QObject::connect(&car, &sc::CarService::reverseChanged, &app,
                     [](bool on) { qInfo() << "reverse" << on; });
    QObject::connect(&car, &sc::CarService::steeringAngleChanged, &app,
                     [](double angle) { qInfo() << "angle" << angle; });
    if (app.arguments().contains(QStringLiteral("--vcan"))) {
        if (!car.start(QStringLiteral("vcan0"))) return 2;
        qInfo() << "Receive standard test frames on vcan0 for 30 seconds";
        QTimer::singleShot(30000, &app, [&] {
            car.stop();
            qInfo() << "socket frames" << car.framesReceived();
            app.quit();
        });
        return app.exec();
    }
    const QByteArray good = QByteArray::fromHex("7800b80b64010000");
    car.onCanFrame(0x100, good);
    car.onCanFrame(0x100, good);
    car.onCanFrame(0x100, good.left(2));
    if (car.speed() != 120 || changes != 1) return 3;
    car.onCanFrame(0x100, QByteArray::fromHex("ffffb80b64010000"));
    if (car.speed() != 240 || changes != 2) return 4;
    const int bytes[] = {0x00, 0x03, 0x04, 0x06};
    for (int position = 0; position < 4; ++position) {
        QByteArray gear(8, '\0');
        gear[0] = static_cast<char>(bytes[position]);
        car.onCanFrame(0x300, gear);
        if (car.gearPosition() != position ||
            car.reverseEngaged() != (position == 1)) return 5;
    }
    sc::CarService partial;
    partial.onCanFrame(0x100, good);
    if (!partial.parser()->loadDbcText(QStringLiteral(
        "BO_ 256 Lab: 8 ECU\n"
        " SG_ speed : 0|16@1+ (1,0) [0|240] \"km/h\" ECU\n"))) return 6;
    partial.onCanFrame(0x100, QByteArray::fromHex("c800000000000000"));
    if (partial.speed() != 120) return 7;
    qInfo() << "duplicate, short, clamp, gear, partial DBC: passed";
    car.setSimulatedMode(true);
    sc::SignalSimulator sim(&car);
    sim.start();
    QTimer::singleShot(1200, &app, [&] {
        sim.stop();
        const bool ok = car.speed() > 0 && car.canConnected();
        car.stop();
        qInfo() << "sim speed" << car.speed() << "socket frames" << car.framesReceived();
        app.exit(ok ? 0 : 8);
    });
    return app.exec();
}
```

编译和运行只使用练习目录里的 CMake 工程，不需要 GStreamer：

```bash
cmake -S "$WORK/05" -B "$WORK/05/build" -DPROJECT_PATH="$PROJECT"
cmake --build "$WORK/05/build" --parallel 2
"$WORK/05/build/vehicle_lab"
```

### 如何读实验结果

先出现 `speed 120`、`speed 240`；重复帧和短帧不再通知。随后挡位变化，并打印 `duplicate, short, clamp, gear, partial DBC: passed`。
模拟速度逐步上升，约 1.2 秒停止，`socket frames` 为 0；没开 socket 仍能更新状态，不是伪造计数。
练习 A：只在实验中把 `good.left(2)` 改为 `good.left(7)`，解释为什么完整字段已经足够。
练习 B：把 `ffff` 改成 `c800`，将对应预期改为 200，验证 clamp 只限制越界值。

## 5. 理解 --sim 的时钟与能力边界

参考 [SignalSimulator.h](../src/core/SignalSimulator.h) 和 [SignalSimulator.cpp](../src/core/SignalSimulator.cpp)。
实现顺序是：构造 100ms 定时器 → start 清零累计时间 → tick 加 100 → 编码并注入三种帧。
每次 tick 生成 **0x100、0x300、0x400**；不会生成 0x200，更不会生成空调 ACK。
一圈为 60000ms：0..8s 加速，8..30s 巡航，30..40s 减速，随后停车。
**45..52s 是倒车窗口**，发送 `03`；52s 后回 P，下一圈重新开始。
这是按 tick 累加的模拟时间；主线程忙时，不保证与墙上时钟严格同步。

`setSimulatedMode(true)` 让 `canConnected()` 对健康判断返回 true，并不打开 CAN。
`framesReceived()` 来自 CanManager 的 socket 接收计数，因此纯 `--sim` 中可能始终为 0。
三帧每 100ms 是模拟注入节奏，不能当作实测总线吞吐量或 CAN 带宽证明。
模拟倒车时方向盘模型当前为零；不要期待它自动产生复杂倒车转向轨迹。

如已具备主项目依赖，可在仓库外构建真实 UI；依赖包括 Qt5 Widgets/DBus/Test 和 GStreamer 开发包。
不要使用仓库自带的 build 目录，也不要运行安装或部署脚本。

```bash
cmake -S "$PROJECT" -B "$HOST_BUILD" -DCMAKE_BUILD_TYPE=Debug \
  -DSMART_COCKPIT_USE_QT6=OFF -DBUILD_TESTING=ON
cmake --build "$HOST_BUILD" --target smart-cockpit --parallel 2
mkdir -p "$WORK/logs"
"$HOST_BUILD/smart-cockpit" --sim --log-dir "$WORK/logs"
```

该命令需要可用的桌面显示环境；无显示时优先使用前面的 QtCore 实验。
进入仪表页观察一圈，再进入空调页调温：**默认模拟器下空调通常会超时回滚**。
原因不是 CAN 解码失败，而是没有车辆或测试程序注入 ACK；第 07 篇会自动注入。

## 6. 可选：仅在获授权的主机上验证 vcan0

vcan 是 Linux 内核提供的虚拟 CAN，不连接真实车辆；先确保装有 can-utils。
**禁止向实车发送测试帧；不要把下面的 vcan0 替换成 can0，也不要运行自动 CAN 配置脚本。**
默认实验到上一节即可完成；下面命令只展示给有管理员授权的读者，不会自动执行。
先只读检查 `ip -details link show vcan0`；已有接口必须确认类型是 vcan，不能仅凭名字判断。
只有确认接口不存在、内核支持且获准修改本机网络时，才逐条手工执行：

```bash
# 可选管理员步骤：仅用于新建本机虚拟接口，不配置真实 CAN。
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set dev vcan0 up
ip -details link show vcan0
```

提前打开三个终端并准备好命令，变量按第 4 节定义；先开接收程序，再开观察器，最后发送。
终端 A：运行 `"$WORK/05/build/vehicle_lab" --vcan`，30 秒后自行退出。
终端 B：只读观察，最多 8 帧或 30 秒结束：

```bash
timeout 30s candump -n 8 vcan0
```

终端 C：发送恰好 8 个标准测试帧，不使用无限循环：

```bash
cansend vcan0 100#7800B80B64010000
cansend vcan0 100#7800B80B64010000
cansend vcan0 100#7800
cansend vcan0 300#0300000000000000
cansend vcan0 400#9CFF000000000000
cansend vcan0 300#0400000000000000
cansend vcan0 300#0600000000000000
cansend vcan0 300#0000000000000000
```

预期：candump 看见 8 帧，A 只通知一次 speed=120；短帧不清零，angle=-10，挡位最终回 P。
标准 R 帧触发 reverse=true，后续 N 帧使其变回 false；实验程序没有 UI，不会弹出相机页。
没有其他发送者且所有帧在接收窗口内到达时，停止接收后的计数应为 8。
若终端准备耗时导致窗口结束，重新启动 A/B 后再发送这一组，仍保持有限次数。
`timeout` 到期可能返回 124，表示观察窗口结束，不是 CAN 解码错误；不自动删除接口。

## 7. 排错与验收

| 现象 | 优先检查 |
| --- | --- |
| undefined reference to vtable | CMake 是否开启 AUTOMOC，是否包含对应 Q_OBJECT 头文件 |
| 模拟器不动 | 是否调用 app.exec()；是否阻塞主线程；是否执行 sim.start() |
| 模拟器计数为 0 | 正常，它绕过 CanManager；不能据此否定状态更新 |
| candump 有帧而服务不更新 | 是否标准 ID、长度足够、字段名齐全；重复值本来就不发变化信号 |
| vcan0 not found | 回默认无 CAN 实验；不要自行改真实网络或尝试绕过权限 |
| 空调没“已确认” | 模拟器不产生 ACK，转第 07 篇定时注入实验 |

对照 [服务测试](../tests/tst_climateservice.cpp) 的 `partialDbcKeepsLastKnownGood()` 和 [UI 测试](../tests/tst_ui.cpp) 的 `simulatorDrivesInstrument()`，无需运行全量测试。

- [ ] 能画出真实 CAN 与进程内模拟的两条不同数据路径。
- [ ] 能解释 QThread 对象归属、run 执行线程以及 QueuedConnection 的作用。
- [ ] 短帧、缺字段、重复值、越界值和 R/N/D/P 的实验结果符合预期。
- [ ] 不把模拟注入计数当作总线性能，不声称扩展 ID 发送已修复。
- [ ] 所有练习产物位于仓库外，未向真实 CAN 发送测试帧。

[返回学习路线](README.md) · 接着写界面：[06 控件与仪表](06-widgets-and-dashboard.md) · 命令闭环：[07 空调命令链路](07-climate-command-flow.md)
