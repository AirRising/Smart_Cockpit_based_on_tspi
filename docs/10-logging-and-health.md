# 10 · 异步日志与健康监控：让故障可见

配套：[本章练习题](exercises/10-logging-and-health.md) · [全部题集](exercises/README.md)

[返回学习路线](README.md) · [上一章：09 摄像头与倒车](09-camera-and-reverse.md) · [下一章：11 测试与调试](11-testing-and-debugging.md)

本章主线是 Qt 5 / C++17，目标不是“加几句打印”，而是解释线程、退出和状态判断。
先读概念，再按文件顺序复写到自己的练习工程，最后用故障场景验证。
本文只提供教学步骤；不要求修改原仓库源码、系统配置或设备权限。
所有代码块都注明“源码摘录”或“完整示例”，摘录不能当成独立程序编译。

## 1. 从零理解：日志和健康不是同一件事

- 日志保存“发生过什么”，例如空调请求号、启动失败原因和倒车切换。
- 健康监控回答“现在还能信任哪些服务”，例如 CAN 最近是否有帧。
- 同步日志让调用者直接写文件，磁盘慢时可能拖住 UI。
- 异步日志让调用者先放入队列，另一个线程稍后写文件。
- 生产者是调用 `log()` 的线程；消费者是唯一的文件写线程。
- 互斥锁保护共享队列；条件变量让消费者没事时睡眠，不空转占 CPU。
- `flush()` 把 C++ 流缓冲交给操作系统，不等于 `fsync()` 的持久化承诺。
- 健康状态是项目自己的应用状态，不是汽车功能安全认证或硬件看门狗。

本项目的日志队列使用 `std::thread/mutex/condition_variable/deque` 与 `ofstream`。
不是 QThread + QMutex 队列；Qt 只通过适配层转换字符串、接管消息。
摄像头显示类里的 QMutex 属于另一个模块，不要混在一起理解。

## 2. 建立练习目录与文件地图

以下为教学命令，仅在依赖已经准备好的普通用户终端中使用：

```bash
export PROJECT=/home/air/qt/Smart_Cockpit_based_on_tspi
export HOST_BUILD="$HOME/build/smart-cockpit-host"
export WORK="$HOME/smart-cockpit-lab"
mkdir -p "$WORK/logging" "$WORK/logs"
```

仓库作为只读参考；练习程序、日志和外置构建全部留在仓库外。
如果前几章还没完成主机构建，先阅读下一章的配置命令。

| 复写顺序 | 真实文件 | 先完成的职责 |
| --- | --- | --- |
| 1 | [AsyncLoggerCore.h](../src/logging/AsyncLoggerCore.h) | Level、Entry、生命周期 API、队列成员 |
| 2 | [AsyncLoggerCore.cpp](../src/logging/AsyncLoggerCore.cpp) | 开文件、入队、消费、停止、轮转 |
| 3 | [AsyncLogger.h](../src/logging/AsyncLogger.h) / [cpp](../src/logging/AsyncLogger.cpp) | QString 转换和 Qt 消息路由 |
| 4 | [SystemState.h](../src/core/SystemState.h) | Normal、Degraded、Emergency |
| 5 | [HealthMonitor.h](../src/core/HealthMonitor.h) / [cpp](../src/core/HealthMonitor.cpp) | 定时读取服务状态并发信号 |
| 6 | [main.cpp](../src/main.cpp) | 创建对象、连接 UI、按顺序退出 |
| 7 | [MainWindow.cpp](../src/ui/MainWindow.cpp) | 状态胶囊、诊断文字 |

不要一次抄完所有文件：先让纯 C++ 日志示例工作，再接 Qt，最后加入健康监控。
链接里的实现是参照；下文讨论的能力边界同样属于实现的一部分。

## 3. 第一步：设计日志接口与启动失败路径

在 `AsyncLoggerCore.h` 先声明 `instance/start/stop/log/currentPath`。
`Level` 从低到高为 Info、Warn、Error，低于 `minLevel` 的消息不入队。
`Entry` 保存等级、字符串、毫秒时间戳；时间在生产者入队时取得。
随后增加线程、互斥锁、条件变量、双端队列、文件流和两个状态标志。
禁止复制日志单例，避免复制线程、文件和锁的所有权。

`start(directory, minLevel)` 的实现顺序如下：

1. 持锁检查 `m_running`，已经启动就直接返回，不重新应用参数。
2. 尝试创建指定目录，默认是 `/var/log/smart-cockpit`。
3. 以追加模式打开其中的 `smart-cockpit.log`。
4. 打不开时回退到**进程当前工作目录**下的 `logs`。
5. 两处都打不开则向 `std::cerr` 写错误，返回且不启动线程。
6. 成功后保存路径、设置标志，再创建消费者线程。

`start()` 返回 `void`，不能写 `if (logger.start(...))`。
`currentPath()` 可以辅助定位文件，但不是可靠的“本次启动成功”状态码。
尤其失败重启可能留下旧路径；还需看 stderr、文件是否存在以及是否实际追加。
未启动时调用 `log()` 会被忽略，不会帮你缓存到以后再写。

## 4. 第二步：先入队，再在锁外写文件

**源码摘录：`AsyncLoggerCore::log()` 的核心步骤。**

```cpp
std::lock_guard<std::mutex> lock(m_mutex);
if (!m_running || level < m_minLevel)
    return;
m_queue.push_back({level, message, nowMs()});
m_cond.notify_one();
```

锁把检查和入队保护为一个临界区；不同生产者的顺序由拿到锁的先后决定。
异步不等于完全无阻塞：复制字符串、申请队列内存和等待互斥锁仍有成本。
这里是**无界队列**，没有最大条数、丢弃策略或背压机制。
若产生日志比磁盘写入更快，内存可能一直增加，不能宣传为“高负载不丢不堵”。

**源码摘录：`writerLoop()` 的等待与取批次部分。**

```cpp
std::deque<Entry> batch;
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cond.wait(lock, [this] {
        return !m_queue.empty() || m_stopRequested;
    });
    if (m_queue.empty() && m_stopRequested)
        break;
    batch.swap(m_queue);
}
```

它位于一个循环内，因此这里的 `break` 不是独立片段里可直接编译的语句。
谓词同时检查“有数据”与“要求停止”，也能应对条件变量的虚假唤醒。
`swap` 将当前队列搬到局部批次，释放锁后才格式化并写文件。
新日志可以进入下一批，慢磁盘不会在整个写入期间占住生产者的锁。
每批写完执行 `m_file.flush()`，再调用 `rotateIfNeeded()`。

## 5. 第三步：轮转与退出不能靠猜

轮转阈值是 `10 * 1024 * 1024` 字节，即 10 MiB，不是十进制 10 MB。
检查发生在整批写完后，所以单批或单条很大时文件可能超过阈值。
流程是关闭当前文件、删除旧 `.1`、重命名当前文件、重新打开当前路径。
只保留一份 `smart-cockpit.log.1`；这不是无限期审计归档。
当前代码对轮转中的删除、重命名、重新打开失败没有完整恢复路径。
磁盘满、目录不可写、异常断电都不能靠“调用过 flush”保证恢复。

停止时先停止**所有会继续产生日志的服务和线程**，再调用 logger 的 `stop()`。
其内部设置停止标志并唤醒消费者，随后 `join()` 等待消费者退出。
消费者只有在“队列空且要求停止”时退出，因此正常生命周期下会排空已有队列。
最后关闭文件；不能在持队列锁时等待消费者 join，否则会形成互相等待。

不要把这描述为支持任意并发 `start/stop/log` 的生命周期协议。
特别是多个线程同时 stop、stop 期间仍不断 log，均不在本章可依赖的使用方式内。
局部变量析构也可能打印；规划退出时必须覆盖生产者的析构与后台回调。
异常终止、`kill -9`、掉电或 `qFatal()` 不会可靠走完这条正常退出路径。

## 6. 完整示例：只用 C++17 验证排空与等级过滤

用编辑器将以下**完整示例**保存为 `$WORK/logging/logger_demo.cpp`。
它不依赖 Qt；两条 ERROR、两条 WARN 应保留，INFO 应被过滤。
重复运行会追加，因此验证时区分本轮记录，不要误把旧记录算作重复写入。

```cpp
#include "AsyncLoggerCore.h"
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "usage: logger_demo LOG_DIRECTORY\n";
        return 2;
    }
    auto &logger = sc::AsyncLoggerCore::instance();
    logger.start(argv[1], sc::AsyncLoggerCore::Warn);
    auto produce = [&logger](const std::string &name) {
        logger.info(name + " filtered info");
        logger.warn(name + " warning");
        logger.error(name + " error");
    };
    std::thread first(produce, "first");
    std::thread second(produce, "second");
    first.join();
    second.join();
    logger.stop();
    std::cout << "requested log path: " << logger.currentPath() << '\n';
    return 0;
}
```

教学编译、运行和查看命令：

```bash
c++ -std=c++17 -pthread -I"$PROJECT/src/logging" \
  "$WORK/logging/logger_demo.cpp" \
  "$PROJECT/src/logging/AsyncLoggerCore.cpp" \
  -o "$WORK/logging/logger_demo"
"$WORK/logging/logger_demo" "$WORK/logs"
less "$WORK/logs/smart-cockpit.log"
```

预期：每轮增加四行，包含时间、等级和生产者名称；两个生产者间顺序不固定。
程序返回 0 只代表示例控制流结束，仍须检查日志内容，不能当成启动成功证明。
本例没有测试轮转、磁盘满、竞争停止或掉电持久化。

## 7. 第四步：接上 Qt 消息，但不要递归记录错误

`AsyncLogger` 将 QString 转成 `std::string`，其 `stop()` 委托给核心层。
`installQtMessageHandler()` 用 `qInstallMessageHandler` 安装全局处理器。
Debug/Info 映射 INFO，Warning 映射 WARN，Critical/Fatal 映射 ERROR。
它没有保留源码上下文，也没有将全部消息同步镜像到 stderr。
因此安装后不要仅盯终端；去日志路径看 `qInfo/qWarning` 的内容。

处理器内部不能再用 `qWarning()` 报日志错误，否则会递归调用自己。
核心层打不开文件用 `std::cerr`，正是为了避开这条路由。
Fatal 只是被异步入队，随后进程可能终止，**不保证 fatal 记录写出**。
也不能指望启动失败后的 Qt 消息仍被完整保存。

在主入口按 `start → installQtMessageHandler → 启动服务` 顺序接入。
正常退出参照 `main.cpp`：停 health、模拟器、car、camera、media，最后停 logger。
这个顺序说明了设计意图，不代表操作系统信号退出也已经接上 Qt 清理路径。

## 8. 第五步：写健康监控的真实判断规则

先在头文件继承 QObject，构造器参数顺序是 `car, camera, media, parent`。
保存的是非拥有型服务指针；这些对象必须活得比监控使用它们的时间更久。
构造器连接 QTimer 的 timeout 到 `poll()`，默认间隔设为 2000 ms。
`start(int intervalMs = 2000)` 启动定时器，并且**立即执行一次 poll**。
`stop()` 只停定时器，不重连服务、不重置状态，也不是急停控制器。

**源码摘录：`HealthMonitor::poll()` 的状态选择。**

```cpp
const qint64 now = QDateTime::currentMSecsSinceEpoch();
const bool canAlive = m_car && m_car->canConnected() &&
                      now - m_car->lastFrameTimeMs() < 3000;
const bool cameraAlive = m_camera && m_camera->isRunning() &&
                         m_camera->lastFrameAgeMs() < 2500;
const bool mediaAlive = m_media && m_media->healthy();
sc::SystemState next = sc::SystemState::Normal;
if (!canAlive)
    next = sc::SystemState::Emergency;
else if (!cameraAlive || !mediaAlive)
    next = sc::SystemState::Degraded;
```

| CAN 条件 | camera / media 条件 | 结果 |
| --- | --- | --- |
| 不满足 | 任意 | Emergency，优先级最高 |
| 满足 | 任一不满足 | Degraded |
| 满足 | 都满足 | Normal |

年龄恰好 3000/2500 ms 时也不满足，因为比较符是 `<`，不是 `<=`。
媒体健康不是“必须正在播放”：`MediaService::init()` 成功后空闲也可健康。
相应实现见 [MediaService.cpp](../src/media/MediaService.cpp)。
摄像头启动会先写入当前时间，初期健康不等于已收到首帧，见 [CameraService.cpp](../src/camera/CameraService.cpp)。

状态改变才发 `stateChanged`；每次 poll 都发 `diagnosticsChanged`。
诊断格式为 `CAN:OK CAM:OK MEDIA:OK frames:...`，不自动写入日志。
`MainWindow::onSystemStateChanged()` 更新文字、QSS 属性并重新 polish。
`onDiagnostics()` 写状态条，可能覆盖刚出现的媒体错误文字。

## 9. 时间与模拟模式：容易误读的现象

- 2000 ms 轮询不是精确截止时间；阈值跨过后要等下一次轮询才被监控发现。
- GUI 忙时 QTimer 还会延迟，不能许诺“3 秒整一定告警”。
- 时间来自墙上时钟，不是单调时钟；系统校时前跳/后跳会影响年龄判断。
- `--sim` 只让 `canConnected()` 返回 true，仍需要模拟器持续更新帧时间。
- `SignalSimulator::start()` 先启动定时器，首帧尚未到时立即 poll 可短暂 Emergency。
- [SignalSimulator.cpp](../src/core/SignalSimulator.cpp) 直接调用 `onCanFrame()`，不经过 SocketCAN 接收计数。
- 因此模拟画面正常、诊断 `frames:0` 可以同时出现，不能据此认定模拟器停了。
- [CarService.cpp](../src/core/CarService.cpp) 在解析业务字段前更新时间，流活着不等于每类信号都新鲜。
- 当前没有 HealthMonitor 自动重连、恢复滞回或分信号新鲜度统计。

## 10. 练习与预期：一次只制造一种故障

在桌面会话中使用已构建程序；显式日志目录避免依赖 `/var/log` 权限。

```bash
"$HOST_BUILD/smart-cockpit" --sim --camera "" --log-dir "$WORK/logs"
```

1. 等首帧和一次监控轮询：依赖齐备时应趋向 Normal；无需先播放音乐。
2. 在独立运行中把 `--camera ""` 改成不存在的 `$WORK/missing-camera`。
3. 预期 CAN 模拟继续更新，摄像头失败，系统应为 Degraded，而不是 Emergency。
4. 在另一独立运行中去掉 `--sim`，使用 `--can sc_missing0`，先确认该接口确实不存在。
5. 预期 CAN 打开失败，Emergency 优先于同时存在的摄像头或媒体失败。
6. 在自己的练习工程暂停模拟器而不停止 health，预期跨过阈值后的轮询报告 CAN LOST。
7. 对照第 11 章的摄像头测试，区分“服务清空旧帧”与“监控显示降级”两条路径。

不必为练习创建 vcan、不需 sudo，也不要在实际车辆总线上注入故障。
关闭桌面窗口可观察正常退出；Ctrl+C/SIGTERM 不能作为已验证排空的证据。

## 11. 排错与本章验收

| 现象 | 优先检查 | 不要这样修 |
| --- | --- | --- |
| 终端没有 qInfo | handler 已路由到哪个文件、日志是否启动 | 反复用 qWarning 报日志错误 |
| 找不到日志 | `--log-dir`、工作目录、stderr、是否真正追加 | 给整个目录 chmod 777 |
| stop 很慢 | 生产者是否停完、待写量、存储是否阻塞 | 强制退出后声称“已排空” |
| CAN 点亮但系统红 | 接收时间是否过期；连接点不等于健康状态 | 把健康检查改成只看 connected |
| 画面停住但稍后才降级 | 摄像头自身 200 ms 检查与 health 2000 ms 检查 | 把定时轮询说成硬实时 |
| 媒体出错后不恢复绿色 | healthy 的恢复路径是否实际存在 | 假定重开文件一定自动恢复 |

- [ ] 能画出生产者入队、消费者 swap、锁外写入、flush、轮转的流程。
- [ ] 能指出无界队列、10 MiB 超阈值和一份备份的限制。
- [ ] 能用完整示例证明等级过滤和正常 stop 后可读，不夸大为掉电保证。
- [ ] 能解释两处目录失败、void start 与 fatal 丢记录的可能性。
- [ ] 能按真值表区分 Normal / Degraded / Emergency。
- [ ] 能解释模拟 frames 为零、首轮短暂红色、轮询延迟和墙时钟影响。
- [ ] 知道仓库没有 HealthMonitor 专项测试；人工练习不是自动化覆盖。

[返回学习路线](README.md) · [上一章：09 摄像头与倒车](09-camera-and-reverse.md) · [下一章：11 测试与调试](11-testing-and-debugging.md)
