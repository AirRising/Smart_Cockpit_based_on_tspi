[返回学习路线](README.md) · 前置：[06 控件与仪表](06-widgets-and-dashboard.md) · 下一篇：[08 媒体播放](08-media-playback.md)

# 07｜空调命令闭环：防抖、确认、超时与取消

配套：[本章练习题](exercises/07-climate-command-flow.md) · [全部题集](exercises/README.md)

本篇使用 **Qt5 + C++17**，目标不是“按钮能点”，而是“知道车辆有没有确认”。
先读 [05 车辆服务与模拟器](05-vehicle-service-and-simulator.md) 的服务分层；位编码见 [04 CAN 与 DBC](04-can-and-dbc.md)。
所有动手修改都在仓库外；本文不会要求修改项目代码、协议、配置或测试。
本篇完整实验先验证服务，不依赖前面练习中的窗口文件。

## 1. 先分清三个值：已确认、正在编辑、正在等待

假设车辆确认温度为 22℃，你把滑块拖到 24℃。
此时控件可以先显示 24，但不能说车辆已经执行；车辆可能断线或拒绝命令。
因此需要三个角色：

| 数据 | 所属位置 | 含义 |
| --- | --- | --- |
| `m_climate` | CarService | 最近确认/同步的车辆状态，UI 回滚的依据 |
| 控件当前值 | ClimatePage | 用户正在编辑的目标，尚未必发送 |
| `m_pendingClimateTarget` | CarService | 本次等待 ACK 的完整目标快照 |
| `m_pendingRequestId` | ClimatePage | UI 所拥有的本地待处理请求编号，0 表示没有 |

默认 `ClimateState` 来自 [SystemState.h](../src/core/SystemState.h)：
温度 **22**、风量 **3**、模式 **0**、`acOn=true`、`autoMode=false`。
温度有效范围为 **16..30℃**，风量为 **0..7**，模式数值为 **0..3**。
UI 滑块限制范围；服务接收回传时也会 clamp，但发送接口没有替任意调用者全面校验目标。
学习时只发送这些范围内的值，避免“存储的目标”和“回传后被 clamp 的值”永远不一致。

ACK 在这里指**应用层 0x200 的 ack 字段**，不是 CAN 控制器的链路层 ACK 槽。
链路层有应答也不表示空调已执行，更不能证明温度已经达到目标。
本项目把匹配的应用层回传作为“命令已确认”，不建模真实制冷过程。

## 2. 先把 0x200 的命令手算出来

参照 [vehicle.dbc](../config/vehicle.dbc) 与 [内置信号表](../src/core/CanFrameParserCore.cpp)。

| 字段 | 起始位与长度 | 编码位置 |
| --- | --- | --- |
| temp_set | `0\|8` | byte0 |
| fan_speed | `8\|4` | byte1 的 bit0..3 |
| blow_mode | `12\|2` | byte1 的 bit4..5 |
| ac_on | `14\|1` | byte1 的 bit6 |
| auto_mode | `15\|1` | byte1 的 bit7 |
| ack | `16\|1` | byte2 的 bit0 |

这些字段都是 Intel、无符号、factor=1、offset=0。
目标 24℃、风量 5、模式 0、AC 开、自动关：byte0=`18`，byte1=`05+40=45`。
命令为 `18 45 00 00 00 00 00 00`，确认回传为 `18 45 01 00 00 00 00 00`。
发送命令时没有提供 ack，整帧编码先清零，所以该位为 0。
只有两字节的“回传”缺少 ACK 位，整帧解码失败，不应改变任何空调状态。

### 必须认识的文字映射分歧

本次核对 [ClimatePage.cpp](../src/ui/ClimatePage.cpp) 时，当前实现是 **QButtonGroup 按钮组，不是 combo**。
按钮 ID 顺序与 SystemState.h 注释一致：0=面部、1=腿部、2=除霜、3=上下（面+脚）。
但 DBC 的 `CM_ SG_ 512 blow_mode` 注释是：0=吹面、1=面+脚、2=吹脚、3=除霜。
**文字语义尚待统一，本教程不修改任何实现或 DBC。**
解析器忽略 `CM_`，所以不会自动按注释重排编号；“字节能往返”不证明模式语义正确。
本篇完整实验固定模式 0；接实际协议前必须由协议双方确认映射。

## 3. 从零实现时，按状态机顺序写

### 第一步：先写状态比较与服务接口

参考 [CarService.h](../src/core/CarService.h) 的发送、取消接口和三个结果信号。
再看 `ClimateState::operator==()`：温度、风量、模式、AC、自动模式**五项全部比较**。
只比较温度会让“风量错了的 ACK”也通过，这是不完整的确认。
服务只允许一个 pending 请求；先实现串行请求，再考虑未来是否真的需要队列。

### 第二步：sendClimateCommand 不等于发送成功证明

参考 [CarService.cpp](../src/core/CarService.cpp) 的 `sendClimateCommand()`，按以下顺序理解：
1. 已有 pending 则返回 0，不覆盖原目标、不生成新请求。
2. 把五字段放进 values，编码失败也返回 0。
3. CAN 已打开才尝试 `sendFrame()`；未打开或发送失败会记录警告。
4. **即使未发送成功，当前实现仍继续分配非零本地 ID。**
5. 保存完整 target、设置 pending，启动 600ms 单次 ACK 定时器，返回 ID。

因此非零 ID 只表示“本地已受理并开始等待”，不是“帧已经传输”或“车辆已经执行”。
ID 由 `m_climateRequestSeq` 递增产生，跳过 0；它没有编码进 CAN payload。
`--sim` 不打开 CAN，且不生成空调 ACK，通常会经过 600ms 后超时。
日志里即使出现 `sent` 字样，也要结合前面的发送警告判断，不能当作传输证据。

### 第三步：收到回传时先验字段，再决定是否确认

`handleFrame200()` 先 decode、`hasAllSignals()`、clamp，构造 `echoed`。
下面为 **源码摘录**，不是可单独编译的文件：

```cpp
if (ack) {
    if (!m_pendingClimate || echoed != m_pendingClimateTarget)
        return;
    m_pendingClimate = false;
    m_ackTimer.stop();
    m_climate = echoed;
    emit climateAckOk(m_climateRequestSeq);
    emit climateStateChanged(m_climate);
} else if (!m_pendingClimate && echoed != m_climate) {
    m_climate = echoed;
    emit climateStateChanged(m_climate);
}
```

逐行要点：先拒绝不匹配，再清 pending 和计时器，最后提交已确认状态并通知 UI。
成功时先赋 `m_climate`，所以 UI 在 `climateAckOk` 槽里读 getter 已能拿到新状态。
匹配指**解码并 clamp 后的五个状态字段**匹配，并非 payload 每一位逐字节相等。

| 当前状态 | 收到内容 | 行为 |
| --- | --- | --- |
| 等待中 | ack=true 且五字段匹配 | 确认、停 timer、提交状态 |
| 等待中 | ack=true 但任一字段不同 | 忽略，继续等原目标 |
| 等待中 | ack=false | 忽略，不以普通状态上报打断待处理命令 |
| 空闲 | ack=true | 忽略迟到或无主 ACK |
| 空闲 | ack=false 且状态变化 | 作为车辆面板主动状态同步 |
| 空闲 | ack=false 且值重复 | 不重复通知状态变化 |

### 第四步：超时与取消是两种不同结果

600ms timer 到期时，服务清 pending 并发 `climateAckTimeout(id)`，不提交 target。
因为已确认状态从未被提前改掉，“回滚”主要是 UI 恢复显示 `car->climate()`。
`cancelClimateCommand(id)` 要求：有 pending、id 非零、id 等于当前请求序号。
通过后**停 timer、清 pending、发 climateCommandCancelled(id)**；失败返回 false。
`stop()` 也会停止计时器、关闭总线并取消当前请求；取消不会再冒充超时事件。

取消仅是**本地取消等待**，无法撤回已经发给车辆的命令，也不会发送反向恢复指令。
取消后空闲的匹配 ack=true 仍被忽略；随后 ack=false 状态上报可以同步车辆真实状态。
没有 wire 事务 ID：旧请求和新请求目标若完全相同，迟到的旧 ACK 无法被辨认。
本地 requestId 能过滤旧 UI 通知，却不能解决线上“同目标迟到 ACK”的歧义。

### 第五步：最后接 UI 的 250ms 防抖

参考 [ClimatePage.h](../src/ui/ClimatePage.h) 和 [ClimatePage.cpp](../src/ui/ClimatePage.cpp)。
建议在练习副本按以下顺序写方法，而不是先复制绘图代码：
1. `readControls()`：把控件读成五字段目标；`applyControls()`：用确认状态回填。
2. `applyControls()` 先停防抖 timer，并用 `m_syncing` 防止回填触发新命令。
3. `onControlChanged()`：同步中或已有本地 pending 时返回，否则重启 250ms 单次 timer。
4. `issueClimateCommand()`：防抖到期才读最终值并提交，返回 0 则显示“发送失败，已回滚”。
5. 非零 ID 存入 `m_pendingRequestId`，启用取消按钮，显示等待状态。
6. ACK、超时、取消槽各自核对 ID；只处理属于自己的当前请求。

以下为结果槽共同使用的 **源码摘录**：

```cpp
if (!m_pendingRequestId || requestId != m_pendingRequestId)
    return;
```

不能只写“收到 ACK 就解锁”，否则 0 或旧请求的延迟通知会解锁新的请求。
成功显示“已确认”；超时显示“超时未确认，已回滚”；取消明确显示 **“已取消”**。
这三个分支都会清本地 ID、禁用取消按钮、恢复已确认控件值。
防抖从最后一次调节算 250ms；600ms 从服务受理算起，不是从首次拖动算起。

当前 UI 仍有边界：循环风按钮没有协议字段，也未连接到命令发送流程。
`setPanelEnabled()` 没禁用温度 +/- 按钮；等待中仍可能改变禁用滑块的数值显示。
`m_pendingRequestId` 可阻止再次发请求，但不能据此宣称“等待时所有控件完全锁定”。
本篇说明现状，不宣称已经修复循环风、锁定或模式映射问题。

## 4. 动手：100ms 自动注入 ACK，不与手速比赛

下面是只依赖 QtCore 的实验，不调用 `car.start()`，不接 CAN、不发送任何真实帧。
定时器在受理后约 100ms 注入正确 ACK，远早于 600ms；不要让新手手输 cansend 抢时限。
四种运行模式分别观察确认、超时、手动取消和 stop 取消。

```bash
PROJECT=/home/air/qt/Smart_Cockpit_based_on_tspi
HOST_BUILD="$HOME/build/smart-cockpit-host"
WORK="$HOME/smart-cockpit-lab"
mkdir -p "$WORK/07"
```

保存 `$WORK/07/CMakeLists.txt`，这是该文件的**完整内容**：

```cmake
cmake_minimum_required(VERSION 3.16)
project(climate_lab LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt5 5.15 REQUIRED COMPONENTS Core)
if(NOT DEFINED PROJECT_PATH)
    message(FATAL_ERROR "Pass -DPROJECT_PATH=/absolute/project/path")
endif()
set(CORE "${PROJECT_PATH}/src/core")
add_executable(climate_lab main.cpp
    "${CORE}/CanFrameParserCore.cpp" "${CORE}/CanFrameParser.cpp"
    "${CORE}/CanManager.cpp" "${CORE}/CarService.cpp"
    "${CORE}/CanManager.h" "${CORE}/CarService.h"
)
target_include_directories(climate_lab PRIVATE "${CORE}")
target_link_libraries(climate_lab PRIVATE Qt5::Core)
```

保存 `$WORK/07/main.cpp`，下面为**完整可运行入口**，不是测试源码的省略版本：

```cpp
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include "CarService.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QString mode = app.arguments().value(1, QStringLiteral("ack"));
    if (mode != "ack" && mode != "timeout" && mode != "cancel" && mode != "stop") return 2;
    sc::CarService car;
    const sc::ClimateState confirmed = car.climate();
    sc::ClimateState target = confirmed;
    target.temperature = 24;
    target.fanSpeed = 5;
    int ackCount = 0, timeoutCount = 0, cancelCount = 0, failures = 0;
    quint32 request = 0;
    auto check = [&](bool ok) { if (!ok) { ++failures; qWarning() << "check failed"; } };
    QObject::connect(&car, &sc::CarService::climateAckOk, &app, [&](quint32 id) {
        ++ackCount; check(id == request); qInfo() << "ACK" << id;
    });
    QObject::connect(&car, &sc::CarService::climateAckTimeout, &app, [&](quint32 id) {
        ++timeoutCount; check(id == request); qInfo() << "TIMEOUT" << id;
    });
    QObject::connect(&car, &sc::CarService::climateCommandCancelled, &app, [&](quint32 id) {
        ++cancelCount; check(id == request); qInfo() << "CANCELLED" << id;
    });
    auto payload = [&](const sc::ClimateState &s, bool ack) {
        const QHash<QString, double> values{
            {QStringLiteral("temp_set"), double(s.temperature)},
            {QStringLiteral("fan_speed"), double(s.fanSpeed)},
            {QStringLiteral("blow_mode"), double(s.blowMode)},
            {QStringLiteral("ac_on"), s.acOn ? 1.0 : 0.0},
            {QStringLiteral("auto_mode"), s.autoMode ? 1.0 : 0.0},
            {QStringLiteral("ack"), ack ? 1.0 : 0.0}
        };
        QByteArray bytes;
        check(car.parser()->encode(0x200, values, &bytes));
        return bytes;
    };
    request = car.sendClimateCommand(target);
    check(request != 0 && car.hasPendingClimate());
    check(car.sendClimateCommand(target) == 0);
    check(!car.cancelClimateCommand(0));
    check(!car.cancelClimateCommand(request + 1));
    QTimer::singleShot(50, &app, [&] {
        sc::ClimateState wrong = target;
        wrong.fanSpeed = 4;
        car.onCanFrame(0x200, payload(wrong, true));
        check(car.hasPendingClimate() && car.climate() == confirmed);
        qInfo() << "wrong fan ACK ignored";
    });
    QTimer::singleShot(100, &app, [&] {
        if (mode == "ack") car.onCanFrame(0x200, payload(target, true));
        else if (mode == "cancel") check(car.cancelClimateCommand(request));
        else if (mode == "stop") car.stop();
    });
    QTimer::singleShot(900, &app, [&] {
        const bool success = mode == "ack";
        check(!car.hasPendingClimate());
        check(ackCount == (success ? 1 : 0));
        check(timeoutCount == (mode == "timeout" ? 1 : 0));
        check(cancelCount == ((mode == "cancel" || mode == "stop") ? 1 : 0));
        check(car.climate() == (success ? target : confirmed));
        const auto before = car.climate();
        car.onCanFrame(0x200, payload(target, true));
        check(car.climate() == before);
        sc::ClimateState panel = target;
        panel.temperature = 26;
        car.onCanFrame(0x200, payload(panel, false));
        check(car.climate() == panel);
        qInfo() << "idle panel sync=26; failures=" << failures;
        car.stop();
        app.exit(failures ? 1 : 0);
    });
    return app.exec();
}
```

编译后逐次运行，每次约 0.9 秒结束，不需要人工在 ACK 窗口中操作：

```bash
cmake -S "$WORK/07" -B "$WORK/07/build" -DPROJECT_PATH="$PROJECT"
cmake --build "$WORK/07/build" --parallel 2
"$WORK/07/build/climate_lab" ack
"$WORK/07/build/climate_lab" timeout
"$WORK/07/build/climate_lab" cancel
"$WORK/07/build/climate_lab" stop
```

### 逐步解释与预期结果

1. 不开 CAN 却得到非零 ID，随后第二次发送返回 0，说明 pending 没被替换。
2. 0 与错误编号不能取消请求；50ms 时注入“仅风量不同”的 ACK，仍然等待。
3. ack 模式在 100ms 注入正确五字段，打印 `ACK 1`，确认温度 24、风量 5。
4. timeout 模式不注入正确 ACK，约 600ms 打印 `TIMEOUT 1`，保留默认状态。
5. cancel/stop 模式约 100ms 打印 `CANCELLED 1`，过了 600ms 也不应再出现 TIMEOUT。
6. 900ms 时先验证结果，再验证空闲 ack=true 被忽略、ack=false 能同步到 26。

每次末尾都应是 `failures= 0`；“CAN not open”警告是本实验特意保留的正常现象。
Qt 定时器不是硬实时：若主线程被阻塞或调试器暂停，100ms 回调可能延迟；不要用断点制造超时后误判协议。
练习 A：在副本里改错 autoMode 而不是 fanSpeed，继续验证五字段比较。
练习 B：把 100ms 改为 700ms，在 ack 模式观察迟到 ACK 不能确认；检查失败是预期的反例。
练习 C：恢复 100ms；比较 cancel 与 timeout 日志，解释为何“取消”不能显示成“超时”。

## 5. 把同样思路验收到真实 ClimatePage

完整实验验证服务；UI 防抖、旧通知过滤和取消文案应再对照项目现有测试。
[tests/tst_climateservice.cpp](../tests/tst_climateservice.cpp) 覆盖五字段匹配、取消编号、停 timer、stop、缺字段和防抖。
[tests/tst_ui.cpp](../tests/tst_ui.cpp) 覆盖 UI 往返、取消后新请求、0/旧 ID 通知、拒绝发送不锁 UI。
如具备主项目完整依赖，可先只构建这些目标并指定测试函数；完整回归的方法见 [11 测试与调试](11-testing-and-debugging.md)。

```bash
cmake -S "$PROJECT" -B "$HOST_BUILD" -DCMAKE_BUILD_TYPE=Debug \
  -DSMART_COCKPIT_USE_QT6=OFF -DBUILD_TESTING=ON
cmake --build "$HOST_BUILD" --target tst_climateservice tst_ui --parallel 2
QT_QPA_PLATFORM=offscreen "$HOST_BUILD/tests/tst_climateservice" \
  ackRequiresFullTarget cancelRequiresMatchingIdAndStopsTimeout debounceCoalescesRapidChanges
QT_QPA_PLATFORM=offscreen "$HOST_BUILD/tests/tst_ui" \
  climateRoundTripThroughUi climateCancelThenNewRequest climateRejectedSendDoesNotLockUi
```

这些是读者可选的局部验证命令；不要求改测试文件，也不需要真实 CAN。
观察要点：四次快速调节只形成一个请求；取消显示“已取消”，后续新请求仍能完成。
旧 ID 或 0 的三个结果通知都不能解锁新请求；外部请求导致 UI 发送返回 0 时应回滚而不是锁死。
同步注入帧会同步发出结果信号；Qt5 的 `QSignalSpy::wait()` 不会替你捕捉已经发生的那次发射。
参考测试在同步注入后直接检查 count，只有确实等待未来 timer 时才调用 wait。

## 6. 排错与最终验收

| 现象 | 先检查什么 |
| --- | --- |
| --sim 调温总超时 | 模拟器没有 0x200 ACK，这是默认边界，不是返回 ID 为 0 |
| ACK 位为 1 仍不确认 | pending 是否存在；五字段是否匹配；数据是否至少含 ACK 所在字节 |
| 取消后又看到超时 | 是否用匹配 ID 真正取消；是否混入旧 UI 通知；检查 timer 与信号来源 |
| UI 无缘无故又发命令 | 回填是否使用 m_syncing，是否停止未发送的防抖 timer |
| 模式数字对、文字不对 | SystemState/UI 与 DBC 注释不一致，必须另行统一协议语义 |
| Qt 定时器完全不触发 | QCoreApplication/事件循环是否运行，主线程是否被阻塞 |

- [ ] 能区分控件目标、pendingTarget 与已确认状态，能手算命令/ACK 的不同字节。
- [ ] 能解释 250ms 与 600ms 的起算点，且不把非零 ID 当作传输成功。
- [ ] 四种实验模式结果正确，错误字段 ACK 不确认，取消不再超时。
- [ ] UI 只接受匹配本地 ID；0/old 通知不影响新请求，取消状态显示“已取消”。
- [ ] 明白无 wire ID 的同目标旧 ACK 歧义、本地取消不撤回车辆命令。
- [ ] 已知循环风未接协议、+/- 未全锁、模式注释待统一；未宣称它们已修复。
- [ ] 所有文件和构建产物均在练习区/外部构建目录，没有改动仓库实现。

[返回学习路线](README.md) · 回看：[06 控件与仪表](06-widgets-and-dashboard.md) · 继续：[08 媒体播放](08-media-playback.md)
