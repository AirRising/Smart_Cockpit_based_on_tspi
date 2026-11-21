# 11 · 测试与调试：从“能运行”走到“有证据”

配套：[本章练习题](exercises/11-testing-and-debugging.md) · [全部题集](exercises/README.md)

[返回学习路线](README.md) · [上一章：10 日志与健康](10-logging-and-health.md) · [下一章：12 交叉构建与部署](12-cross-build-and-deployment.md)

本章采用 Qt 5 / C++17，先认识测试，再按纯算法、服务、界面顺序建立证据。
下面的命令都是教学命令，不表示本次文档编写已经运行测试或修复缺陷。
练习文件只放在仓库外；不要为了“变绿”修改业务期望或跳过失败用例。

## 1. 新手概念：测试不是把程序启动一次

- 单元测试：输入已知数据，检查一个小单元的输出及副作用。
- 集成测试：检查解析器、服务、信号槽、控件之间的连接。
- 冒烟测试：尽快发现启动、构造、绘制等大问题，不证明功能全部正确。
- 回归测试：把曾经出错的场景固定下来，防止以后再次发生。
- 边界测试：专门检查最短帧、位跨字节、重复操作、过期消息和停止重启。
- 断言：不满足期望就失败，而不是打印结果后让人肉眼判断。
- 测试替身/注入：不用真实设备，直接传入一帧或向 GStreamer bus 注入消息。
- Sanitizer：编译时插入检查，发现本次运行触发的内存错误或未定义行为。

测试应当描述输入、动作、期望三件事，不只写一个“没有崩溃”的判断。
旧用例全部通过，也可能漏掉 CAN 位序、错误 ACK 或停止后的旧帧等 bug。
编码后再解码的回环尤其可能“两个错误互相抵消”，必须增加独立的字节预期。

## 2. 按实现顺序找到六个测试入口

注册事实以 [tests/CMakeLists.txt](../tests/CMakeLists.txt) 为准，目前是六个 CTest 测试。
CTest 名称带 `tst_` 前缀；每个可执行文件内部还可能包含多个函数和数据行。

| 顺序 / CTest 名 | 真实文件 | 已覆盖的重点 |
| --- | --- | --- |
| 1 `tst_core_cxx` | [tst_core_cxx.cpp](../tests/tst_core_cxx.cpp) | 纯 C++ CAN 边界、无效布局、日志正常排空 |
| 2 `tst_canframeparser` | [tst_canframeparser.cpp](../tests/tst_canframeparser.cpp) | Qt 适配、DBC、截断拒绝、精确字节 |
| 3 `tst_cameraservice` | [tst_cameraservice.cpp](../tests/tst_cameraservice.cpp) | 首帧、EOS/ERROR、卡流、旧帧、失败重试 |
| 4 `tst_climateservice` | [tst_climateservice.cpp](../tests/tst_climateservice.cpp) | target 匹配、请求 ID、取消、防抖、回滚 |
| 5 `tst_screenmanager` | [tst_screenmanager.cpp](../tests/tst_screenmanager.cpp) | 普通切页、倒车抢占、简单返回 |
| 6 `tst_ui` | [tst_ui.cpp](../tests/tst_ui.cpp) | 仪表、空调 UI 链路、模拟器、页面绘制冒烟 |

复写顺序：先完成被测模块接口，再写最小失败输入，再补实现，最后增加边界。
纯 C++ 的 `CHECK` 累计失败并返回非零；Qt 测试使用 `QVERIFY/QCOMPARE`。
Qt 测试类继承 QObject，测试函数放在 `private slots`，由 Qt 元对象系统发现。
`QTEST_GUILESS_MAIN` 提供 QCoreApplication；`QTEST_MAIN` 在本项目中提供 GUI 应用。
含 `Q_OBJECT` 的类需要 moc；项目中的 `CMAKE_AUTOMOC` 会负责生成代码。

**源码摘录：纯 C++ 测试的注册方式。**

```cmake
add_executable(tst_core_cxx tst_core_cxx.cpp)
target_link_libraries(tst_core_cxx PRIVATE smart_cockpit_cxx)
add_test(NAME tst_core_cxx COMMAND tst_core_cxx)
```

它只链接不含 Qt 的 `smart_cockpit_cxx`，是核心层依赖边界的守卫。
但 [顶层 CMakeLists.txt](../CMakeLists.txt) 在配置阶段仍先查 Qt 与 GStreamer。
“目标不链接 Qt”不等于“直接配置整个项目无需安装 Qt/GStreamer 开发依赖”。

## 3. 普通用户外置构建与定向运行

依赖准备好之后使用以下教学命令；本章不执行安装，不用仓库内旧 build。

```bash
export PROJECT=/home/air/qt/Smart_Cockpit_based_on_tspi
export HOST_BUILD="$HOME/build/smart-cockpit-host"
export WORK="$HOME/smart-cockpit-lab"
mkdir -p "$WORK/testing" "$HOME/build"
cmake -S "$PROJECT" -B "$HOST_BUILD" \
  -DCMAKE_BUILD_TYPE=Debug -DSMART_COCKPIT_USE_QT6=OFF -DBUILD_TESTING=ON
cmake --build "$HOST_BUILD" --parallel 2
ctest --test-dir "$HOST_BUILD" -N
ctest --test-dir "$HOST_BUILD" --output-on-failure \
  -R '^tst_(core_cxx|canframeparser)$'
ctest --test-dir "$HOST_BUILD" --output-on-failure -R '^tst_cameraservice$'
```

`-N` 只列出测试，不执行；预期能列出六个名字，不是六个内部断言。
`-R` 按正则筛选 CTest 名称；末尾 `$` 避免意外选中其他名字。
如遇生成器冲突，换一个新的外置构建目录，不要强行混用旧缓存。
配置失败时先修依赖路径，`ctest` 显示零测试不是“全部通过”。

[tests/CMakeLists.txt](../tests/CMakeLists.txt) 已给 climate、screenmanager、ui 设置 offscreen。
通过 CTest 运行这三个 suite 不需要手动补环境；直接运行二进制则要自己设置。

```bash
ctest --test-dir "$HOST_BUILD" --output-on-failure -R '^tst_climateservice$'
ctest --test-dir "$HOST_BUILD" --output-on-failure -R '^tst_(screenmanager|ui)$'
"$HOST_BUILD/tests/tst_canframeparser" -functions
"$HOST_BUILD/tests/tst_canframeparser" signalLayoutBoundaries
QT_QPA_PLATFORM=offscreen "$HOST_BUILD/tests/tst_climateservice" \
  cancelThenNewCommandIgnoresOldAck
QT_QPA_PLATFORM=offscreen "$HOST_BUILD/tests/tst_ui" climateCancelThenNewRequest
```

Qt 可按 `函数名:数据行名` 筛选，例如 `signalLayoutBoundaries:motorola-0-2`。
纯 C++ `tst_core_cxx` 没有 Qt 命令行解析，不能对它使用上述函数筛选语法。
全量回归可在交付前另行安排；定位单个问题时先跑相关目标，避免淹没线索。

## 4. 第一组边界：CAN 必须检查线上字节

先读 [CanTypes.h](../src/core/CanTypes.h)，再读 [CanFrameParserCore.cpp](../src/core/CanFrameParserCore.cpp)。
最后读 [CanFrameParser.cpp](../src/core/CanFrameParser.cpp)，确认 Qt 层只做类型转换。

新增边界用例已经检查这些独立结果：

| 布局 | 数值 | 最小原始字节 | 重点 |
| --- | --- | --- | --- |
| Intel `7\|2` | 3 | `80 01` | 跨越字节尾 |
| Motorola `7\|16` | 4660 | `12 34` | DBC 起始位是实际位索引 |
| Motorola `0\|2` | 3 | `01 80` | Motorola 锯齿跨字节 |
| Motorola `3\|12` | 2748 | `0A BC` | 非对齐布局 |

核心单信号编码输出按需要扩展；整帧编码会补足经典 CAN 的八字节。
所以 Qt 适配测试使用 `payload.leftJustified(8, '\0')` 比较，而非只比较前两字节。
还要验证 63/64 位宽、最后合法位、非法 startBit/bitLength、越过第八字节。
`double` 无法精确表示所有 64 位整数，不要把宽度测试等同于全范围精确性证明。

整帧 decode 失败时输出容器应保持原样；成功时不应残留上一帧的旧键。
单信号 decode 的 0 也可能是真值，判断整帧失败应使用 bool 返回值。
DBC 只支持当前解析器实现的子集，不能从少数示例推断完整标准兼容。

### 完整示例：从零写一个独立 Qt 测试

用编辑器保存为 `$WORK/testing/boundary.cpp`，不改仓库测试列表。
此例特意用写死的预期字节，避免依赖同一解码器证明编码正确。

```cpp
#include <QtTest>
#include "CanFrameParser.h"

class BoundaryTest : public QObject
{
    Q_OBJECT
private slots:
    void motorolaWireBytes()
    {
        sc::CanFrameParser parser;
        QVERIFY(parser.loadDbcText(QStringLiteral(
            "BO_ 1536 Layout: 8 ECU\n"
            " SG_ signal : 0|2@0+ (1,0) [0|3] \"\" ECU\n")));
        QHash<QString, double> values;
        values.insert(QStringLiteral("signal"), 3.0);
        QByteArray bytes;
        QVERIFY(parser.encode(1536, values, &bytes));
        QCOMPARE(bytes, QByteArray::fromHex("0180000000000000"));
    }
};

QTEST_GUILESS_MAIN(BoundaryTest)
#include "boundary.moc"
```

教学构建命令；此处 qmake、moc、pkg-config 都必须属于同一套**宿主 Qt 5**。

```bash
QT_HOST_BINS="$(qmake -query QT_INSTALL_BINS)"
"$QT_HOST_BINS/moc" "$WORK/testing/boundary.cpp" -o "$WORK/testing/boundary.moc"
c++ -std=c++17 -fPIC $(pkg-config --cflags Qt5Test Qt5Core) \
  -I"$PROJECT/src/core" "$WORK/testing/boundary.cpp" \
  "$PROJECT/src/core/CanFrameParser.cpp" "$PROJECT/src/core/CanFrameParserCore.cpp" \
  $(pkg-config --libs Qt5Test Qt5Core) -o "$WORK/testing/boundary"
"$WORK/testing/boundary" motorolaWireBytes
```

练习：仅在练习文件中将预期 `0180` 改为 `0300`，应稳定失败并给出实际/预期。
再改回正确预期应通过，这验证测试真能抓错，而不是验证过一次就删除断言。

## 5. 第二组边界：空调 ACK 必须匹配 target 与本地 ID

按 [CarService.cpp](../src/core/CarService.cpp) 的顺序追踪：
`sendClimateCommand → m_pendingClimateTarget → handleFrame200 → climateAckOk`。
再追踪 `cancelClimateCommand` 与 [ClimatePage.cpp](../src/ui/ClimatePage.cpp) 中的 pending ID。

- ACK 位为 1 不够：temperature、fanSpeed、blowMode、acOn、autoMode 必须都匹配。
- 截断 ACK、不同 target 的旧 ACK 不应确认当前请求。
- pending 期间再次发送返回 0，不应替换旧目标或锁死 UI。
- 取消只能匹配当前非零请求 ID；重复取消和错误 ID 应返回 false。
- 取消/stop 应停止超时计时，之后不能再出现该请求的 timeout。
- 无 pending 时忽略迟到 ACK；`ack=0` 的面板状态可以同步已确认状态。
- UI 必须忽略旧 ID/零 ID 的本地完成通知，不能误解锁一个新请求。

本地请求 ID **不在 CAN 帧里**，协议无法区分“相同 target 的旧 ACK”和新 ACK。
测试通过不代表这个协议边界已解决；需要协议事务号才能提供更强的关联保证。
`sendClimateCommand` 返回非零也不证明真实发送成功：未开 CAN 的测试模式仍可进入 pending。

**源码摘录：取消后给事件循环时间，检查没有额外超时。**

```cpp
QTest::qWait(CarService::kClimateAckTimeoutMs + 150);
QCOMPARE(cancelledSpy.count(), 1);
QCOMPARE(timeoutSpy.count(), 0);
QCOMPARE(ackSpy.count(), 0);
```

`QTest::qWait` 等待时会处理 Qt 事件；`std::this_thread::sleep_for` 只让线程睡眠。
在 GUI 线程用普通 sleep 会阻止定时器/队列信号执行，可能把错误测试成“没有超时”。
等待异步结果优先用 `QTRY_VERIFY(car.hasPendingClimate())` 这类条件等待。
已经同步发出的信号应检查 spy 计数，不要再调用 wait 等一个不会再来的新信号。
QSignalSpy 应在动作前创建，并为自定义信号参数注册元类型。

## 6. 第三组边界：摄像头停止后不能复活旧图

先读 [CameraService.cpp](../src/camera/CameraService.cpp) 的 `start/stop/pollStream/handleSample`。
理解 GStreamer streaming 线程产生图像，Qt 队列稍后才执行显示回调。
`stop()` 清空画面还不够：已排队的旧图可能随后把“NO SIGNAL”覆盖掉。
当前实现用 stream generation 标识轮次，旧轮次回调必须被拒绝。

[tst_cameraservice.cpp](../tests/tst_cameraservice.cpp) 的验证顺序：

1. 使用 `videotestsrc` 获取真实测试帧，确认大小为 64×48，而非仅检查 start 返回值。
2. 注入 EOS 和 ERROR，等待 `isRunning()` 为 false，检查错误信号只出现一次。
3. 确认清空后的 QImage 仍为空，即使再次处理队列也不能恢复旧图。
4. 暂停 pipeline 模拟卡流，等待 2500 ms 新鲜度阈值后的服务检查。
5. 人工排入 2×2 的可识别旧图，stop 或 stop/start 后都不得交付它。
6. 打开不存在的设备，检查 pipeline/appsink/bus 释放，再用测试源成功重试。

这里的 `#define private public` 是现有测试的白盒注入手段，不是生产接口设计建议。
测试调用 start 重试，不代表服务已经实现自动重连。
这些用例不需要真实摄像头，但仍需要 GStreamer 对应插件。

## 7. UI 金路径与边界验收矩阵

先以 [ScreenManager.cpp](../src/ui/ScreenManager.cpp) 验证调度，后以 UI 验证用户可见结果。
`pagesRenderSomething` 只检查部分页面存在足够亮像素，不是截图比对或完整端到端测试。
`offscreen` 也不等价于真实屏幕、KMS、触摸和 QOpenGLWidget 上下文。

| 场景 | 金路径预期 | 边界 / 当前覆盖性质 |
| --- | --- | --- |
| 仪表 | `7800b80b64010000` 显示 120 / 3000 / 50% | 截断/缺字段保持最后有效值；服务层有用例 |
| 空调 | 调整→250 ms 防抖→pending→匹配 ACK→确认 | 取消再发送、错误通知、超时回滚已有用例 |
| 倒车调度 | 普通页→Urgent 倒车→拒绝普通请求→返回 | 仅简单返回有用例，多普通页历史仍须补测 |
| 摄像头 | 首帧→持续更新→stop 清空 | EOS/ERROR/卡流/旧轮次已测，真实 GL 显示未证明 |
| 媒体 | 选择文件→播放/暂停/停止→进度和错误反馈 | 当前 UI 冒烟不证明音频实际播放或进度正确 |
| 健康 | 首帧后恢复、失联后 Emergency | 没有 HealthMonitor 专项测试，需另建覆盖 |
| 整机 | 中文、导航、竖屏比例和触摸可用 | 桌面/板端人工验收，不可用亮像素断言代替 |

### 仍未修复或未被测试证明的边界

- [MediaService::pollBus](../src/media/MediaService.cpp) 没有 bus 消息就提前返回，位置查询可能停止刷新。
- 媒体 ERROR 将 healthy 置 false，重新 openFile 没有明确将其恢复为 true。
- [MediaPage::openSelected](../src/ui/MediaPage.cpp) 的播放按钮重开当前文件，不能假定暂停后无缝续播。
- `onPlaybackStateChanged` 当前不更新按钮外观，绘制冒烟无法证明按钮状态正确。
- ScreenManager 留有多个普通页活动优先级，同优先级迭代不保证“返回最后访问页”。
- 未注册页面请求可能返回 true，虽然实际 `switchTo()` 只报错而不切页。
- [MainWindow.cpp](../src/ui/MainWindow.cpp) 点击导航先改变按钮选中态，请求被拒时未显式回滚。
- 第 10 章的日志轮转失败、并发生命周期和墙时钟回拨没有系统性覆盖。

这些是读源码得到的限制或待复现场景；本章没有改代码，也没有声称修复完成。

## 8. 调试顺序：先最小复现，再开工具

1. 记录失败测试名、函数/数据行、Qt/GStreamer 版本和第一条错误。
2. 单独重跑失败函数，区分确定性逻辑失败与环境缺插件。
3. 在 [CarService::handleFrame200](../src/core/CarService.cpp) 或对应边界处下断点。
4. 检查 pending target、request ID、队列轮次，不要只看最后一个 UI 标签。
5. 修复练习工程后保留回归用例，不用延长所有等待时间掩盖事件循环阻塞。

教学 GDB 命令：

```bash
gdb --args "$HOST_BUILD/tests/tst_canframeparser" signalLayoutBoundaries
# 在 gdb 内依次输入：
# break sc::CanFrameParserCore::encodeSignal
# run
# bt
# next
# print def.startBit
# continue
```

应用日志已经被 Qt handler 接管时，结合第 10 章指定的文件查看。
必要时只对单次进程启用 `GST_DEBUG=3` 或 `QT_DEBUG_PLUGINS=1`，不要全局污染环境。

## 9. ASan + UBSan：单独的外置构建

以下**完整教学命令组**使用独立目录，不复用普通或交叉构建缓存。
GCC/Clang 与相应运行库需已经可用；Qt/GStreamer 未必也是插桩构建。

```bash
export SAN_BUILD="$HOME/build/smart-cockpit-asan-ubsan"
cmake -S "$PROJECT" -B "$SAN_BUILD" \
  -DCMAKE_BUILD_TYPE=Debug -DSMART_COCKPIT_USE_QT6=OFF -DBUILD_TESTING=ON \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build "$SAN_BUILD" --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ctest --test-dir "$SAN_BUILD" --output-on-failure -R '^tst_(core_cxx|canframeparser)$'
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ctest --test-dir "$SAN_BUILD" --output-on-failure -R '^tst_cameraservice$'
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ctest --test-dir "$SAN_BUILD" --output-on-failure -R '^tst_ui$'
```

先读第一份报告的分配/释放堆栈，再区分自己对象、Qt 平台插件和第三方进程级缓存。
若确认特定第三方泄漏，可使用 LSan suppression 文件，但规则必须精确到已调查的栈。
报告里记录被抑制库版本、规则和原因，不能用匹配整个 Qt 的规则一概隐藏。
为单独调查越界可临时 `ASAN_OPTIONS=detect_leaks=0`，但那次运行**没有验证无泄漏**。
Sanitizer 没报错仅表示本次覆盖路径未触发相应检测，不能声称全项目无 bug/无泄漏。

## 10. 排错、练习收尾与验收

| 问题 | 检查方向 |
| --- | --- |
| 找不到 Qt5Test / GST | 确认开发包和宿主 pkg-config 路径，运行时库不等于开发依赖 |
| 找不到 xcb/display | 直接运行 GUI 测试时加 offscreen；不要给整个工程默认改平台 |
| offscreen OpenGL 失败 | 分类为图形环境限制，真实倒车显示仍需桌面/板端验证 |
| spy 一直等不到 | 是否动作前安装、已同步发出、元类型未注册、事件循环被阻塞 |
| 摄像头首帧超时 | 查 videotestsrc/appsink 插件、GStreamer 首条错误，不盲目加等待 |
| CTest 零用例 | BUILD_TESTING、配置目录、构建是否完成，不把零用例算通过 |

不要直接运行 [scripts/ci.sh](../scripts/ci.sh)：它要求 root、安装包并跑全套测试。
当前仓库没有 `.github/workflows/ci.yml`；脚本或 README 的文字不等于工作流真实存在。
[format.sh](../scripts/format.sh) 不带 `--check` 会修改源文件，也不属于只读调试。

- [ ] 六个 suite 与内部函数的区别清楚，能用 CTest 正则和 Qt 单函数定位。
- [ ] 完整示例故意写错时失败、恢复时通过，说明断言有实际约束。
- [ ] 能解释精确 CAN 字节、target/本地 ID、camera 旧帧的三个关键边界。
- [ ] 能区分 qWait、QTRY_VERIFY、同步 spy 计数与普通 sleep。
- [ ] 记录 UI 金路径、仍未修复的边界及真实屏幕待验事项。
- [ ] Sanitizer 报告保留检测范围及抑制原因，不夸大结果。

[返回学习路线](README.md) · [上一章：10 日志与健康](10-logging-and-health.md) · [下一章：12 交叉构建与部署](12-cross-build-and-deployment.md)
