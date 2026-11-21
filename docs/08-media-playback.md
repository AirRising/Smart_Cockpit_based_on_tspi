# 08｜把本地音频接成可观察的播放闭环

配套：[本章练习题](exercises/08-media-playback.md) · [全部题集](exercises/README.md)

[返回学习路线](README.md) · [上一章：空调命令闭环](07-climate-command-flow.md) · [下一章：摄像头与倒车](09-camera-and-reverse.md)

## 1. 先修、目标与实验范围

本章使用 **Qt5、C++17、GStreamer 1.0**，不是 Qt Multimedia 的 QMediaPlayer 教程。
先理解 QObject 信号槽、事件循环，以及“界面发请求，服务报告实际状态”的区别。
不依赖前面章节尚未定义的教学类；后文有可以独立构建的服务观察器。

完成后应能写出：本地文件列表、媒体服务接口、播放控制和进度回传四个部分。
更重要的是能区分“请求播放成功”“管线进入 PLAYING”“扬声器真的有声”。
这三件事不是同一个断言，标题变化也不能证明已经换成了新音频。

只使用你拥有或获得授权的本地音频，不下载未知媒体，不测试网络流或 DRM 绕过。
建议用自己录制的两段声音，A/B 内容明显不同；另备两段时长完全相同的 WAV。
命令不修改项目源码；修复思路只是仓库外练习，不能当作现有代码已经修好。

## 2. 目录、依赖与实现顺序

```bash
export WORK="$HOME/smart-cockpit-lab"
export PROJECT=/home/air/qt/Smart_Cockpit_based_on_tspi
export HOST_BUILD="$HOME/build/smart-cockpit-host"
mkdir -p "$WORK/media-probe" "$WORK/audio" "$WORK/logs" "$HOME/build"
```

把授权音频用文件管理器放到 `$WORK/audio`，下面示例约定文件名是 `A.wav`、`B.wav`。
不要把音频复制到项目仓库；声音测试先调低音量，避免突然大声播放。
需要 Qt5 Widgets 开发包、CMake、C++ 编译器、pkg-config 与 GStreamer 开发包；命令行实验还需 `python3` 和 `gst-launch-1.0`，Debian/Ubuntu 对应 `python3`、`gstreamer1.0-tools`。开发包不等于运行时工具与解码插件都已安装。
运行时还需 `playbin`、对应音频解码器和 `alsasink` 插件；能链接不代表插件齐全。

从零写自己的模块时按下面的顺序走，每一步通过后才增加下一层：

| 顺序 | 新建/实现文件 | 最小职责 |
| --- | --- | --- |
| 1 | `MediaService.h` | 定义控制方法和状态信号，不包含控件 |
| 2 | `MediaService.cpp` | init/shutdown，先证明能创建与释放 playbin |
| 3 | 同一实现文件 | open/play/pause/stop/seek，再接 bus 与定时查询 |
| 4 | `MediaPage.h/.cpp` | 扫目录、保存路径、按钮发请求、标签接结果 |
| 5 | 独立观察器 `main.cpp` | 脱离整个座舱，定位服务行为 |
| 6 | 自己的窗口入口 | 注入服务并保证服务寿命覆盖页面 |

下面观察器直接编译仓库服务作对照；不是把仓库代码复制到练习目录。
自己重写服务时保持同样接口，再在练习 CMake 中改用自己的实现文件进行比较。

## 3. 先画清楚数据流与状态机

主要入口：[MediaService.h](../src/media/MediaService.h)、[MediaService.cpp](../src/media/MediaService.cpp)。

```text
文件列表/按钮 → MediaService → playbin → 解码器 → alsasink
                       ↑          ↓
                Qt 200 ms timer ← bus 消息
                       ↓
            track / position / duration / state / error 信号
```

`playbin` 自动组织解封装、解码和输出；`alsasink` 把音频送往 ALSA 设备。
服务构造函数把 QTimer 间隔设为 200 ms，`init()` 创建 playbin、取得 bus 并启动 timer。
这里靠 Qt 事件循环轮询，不要求你另起 GLib 主循环。

| 方法 | 当前源码实际行为 |
| --- | --- |
| `init()` | 已初始化则返回 true，否则创建 playbin 并启动轮询 |
| `openFile(path)` | 检查存在、设置 URI/sink、发标题和未知时长，然后请求 PLAYING |
| `play()` | 请求 PLAYING；不是阻塞等到发声 |
| `pause()` | 请求 PAUSED，保留管线与当前位置 |
| `stop()` | 请求 READY；不是释放服务，也不是 PAUSED |
| `seekTo(ms)` | 用 TIME 格式执行 FLUSH/KEY_UNIT seek |
| `shutdown()` | 停 timer、切 NULL、释放 playbin/bus，标记不健康 |

`openFile()` **本身已经请求播放**；不要写成“只加载，必须 play 才开始”。
页面随后又调用 `play()`，当前实现属于重复请求，并非另一个必需阶段。
`gst_element_set_state()` 可能返回异步状态转换；非 FAILURE 只说明请求未立即失败。

## 4. URI、时间单位与错误回传

文件系统路径不是 URI：带中文、空格的路径应使用 `QUrl::fromLocalFile()` 转换。
不要手写 `"file://" + path`；列表里保存绝对路径，进入服务时再转换。

**源码摘录：`MediaService::seekTo()` 内部，非完整文件。**

```cpp
gst_element_seek_simple(GST_ELEMENT(m_playbin), GST_FORMAT_TIME,
                        GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                        positionMs * GST_MSECOND);
```

UI 单位为毫秒，GStreamer TIME 为纳秒；`GST_MSECOND` 是两者的换算因子。
查询得到的 position/duration 则除以 `GST_MSECOND` 再发给 UI。
不要在两端都乘 1000，也不要把 Qt 的毫秒数直接作为纳秒传入。
当前 seek 返回值未被上报，某些文件不支持 seek；按了按钮不等于跳转已经成功。

bus 处理应分开理解：

- ERROR：解析 GError/debug，释放临时对象，报告错误和非播放状态。
- EOS：媒体到尾，报告非播放状态；当前没有自动播放下一首。
- TAG：若有 title，覆盖以文件名生成的初始标题。
- STATE_CHANGED：只看 playbin 自身，避免把子元素状态误当整体状态。
- duration/position：属于周期查询信息，不应该依赖是否恰好收到消息。

`healthy()` 是服务内部健康标志，不是扬声器检测结果。
当前 `openFile()` 遇到未初始化或路径不存在会直接 false，不一定发 errorOccurred。
因此新写的页面应处理返回值，不能只等错误信号而保持静默。

## 5. 完整例子：最小服务观察器

以下是**完整例子**，提供全部新增文件；编译时只读仓库 MediaService，不依赖其他页面。
它保留真实服务的当前缺陷，用于观察而非修复，默认仍走 `alsasink`。

**完整文件：`$WORK/media-probe/CMakeLists.txt`**。

```cmake
cmake_minimum_required(VERSION 3.16)
project(media_probe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0)
if(NOT DEFINED COCKPIT_SOURCE)
    message(FATAL_ERROR "Pass -DCOCKPIT_SOURCE=/absolute/project/path")
endif()
add_executable(media-probe main.cpp
    "${COCKPIT_SOURCE}/src/media/MediaService.cpp"
    "${COCKPIT_SOURCE}/src/media/MediaService.h")
target_include_directories(media-probe PRIVATE "${COCKPIT_SOURCE}/src/media")
target_link_libraries(media-probe PRIVATE Qt5::Widgets PkgConfig::GST)
```

**完整文件：`$WORK/media-probe/main.cpp`**。
终端输出位置、时长、状态与错误；GUI 按钮只表达请求，不伪装成状态指示器。

```cpp
#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include "MediaService.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() != 3) {
        qCritical() << "Usage: media-probe /absolute/A.wav /absolute/B.wav";
        return 2;
    }
    const QString a = QFileInfo(args.at(1)).absoluteFilePath();
    const QString b = QFileInfo(args.at(2)).absoluteFilePath();
    sc::MediaService media;
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *title = new QLabel(QStringLiteral("尚未打开音频"), &window);
    layout->addWidget(title);
    auto addButton = [&](const QString &text, auto action) {
        auto *button = new QPushButton(text, &window);
        layout->addWidget(button);
        QObject::connect(button, &QPushButton::clicked, &window, action);
    };
    addButton("Open A", [&] { qInfo() << "open A:" << media.openFile(a); });
    addButton("Open B", [&] { qInfo() << "open B:" << media.openFile(b); });
    addButton("Play / Resume", [&] { media.play(); });
    addButton("Pause", [&] { media.pause(); });
    addButton("Stop", [&] { media.stop(); });
    addButton("Seek 10 seconds", [&] { media.seekTo(10000); });
    QObject::connect(&media, &sc::MediaService::trackChanged, title, &QLabel::setText);
    QObject::connect(&media, &sc::MediaService::positionChanged, &window,
                     [](qint64 ms) { qInfo() << "position(ms):" << ms; });
    QObject::connect(&media, &sc::MediaService::durationChanged, &window,
                     [](qint64 ms) { qInfo() << "duration(ms):" << ms; });
    QObject::connect(&media, &sc::MediaService::playbackStateChanged, &window,
                     [](bool value) { qInfo() << "playing:" << value; });
    QObject::connect(&media, &sc::MediaService::errorOccurred, &window,
                     [](const QString &error) { qWarning() << error; });
    if (!media.init())
        return 1;
    window.setWindowTitle(QStringLiteral("媒体服务观察器"));
    window.show();
    const int result = app.exec();
    media.shutdown();
    return result;
}
```

保存后构建运行；这不是整套项目测试：

```bash
cmake -S "$WORK/media-probe" -B "$WORK/media-probe/build" -DCOCKPIT_SOURCE="$PROJECT"
cmake --build "$WORK/media-probe/build" --parallel 2
"$WORK/media-probe/build/media-probe" "$WORK/audio/A.wav" "$WORK/audio/B.wav"
```

先 Open A，再 Pause，再 Play/Resume，最后 Stop；观察终端与声音是否一致。
与实际 MediaPage 不同，这里的 Resume 直接调用 `play()`，不再次 openFile。
预期真实音频设备可用时 A 能发声；若失败，先保留错误信息而不是连续点击重试。

## 6. 实现文件列表与进度条

阅读 [MediaPage.cpp](../src/ui/MediaPage.cpp)，先跳过封面、图标绘制，找到 `scanDirectory()`。

1. 清空 QListWidget；目录不存在就返回，当前不会显示专门的目录错误。
2. 用 `*.mp3`、`*.flac`、`*.wav`、`*.ogg` 过滤，只扫描当前目录，不递归。
3. 按 `QDir::Name` 排序，列表文字显示文件名。
4. 完整路径放到 `Qt::UserRole`；不要从显示标题反推路径。
5. 双击或播放按钮触发 `openSelected()`，取当前项路径并调用服务。

**源码摘录：创建一个列表项的局部。**

```cpp
auto *item = new QListWidgetItem(info.fileName());
item->setData(Qt::UserRole, info.absoluteFilePath());
m_fileList->addItem(item);
```

`info` 来自文件遍历，`m_fileList` 是页面成员；这段不是独立完整文件。
页面先查 `SMART_COCKPIT_MEDIA_DIR`，为空才用 QStandardPaths 的 MusicLocation。
[main.cpp](../src/main.cpp) 在创建页面前把非空 `--media-dir` 写入该环境变量。
因此命令行非空参数优先；当前页面只在构造时扫描，没有文件系统实时监视器。

进度条实现顺序：先 `setRange(0, 0)`，收到 duration 后设范围，收到 position 后设值。
用户 `sliderReleased` 才发送 seek；不要把每次程序设值都变成一次 seek 请求。
当前 `m_sliderSyncing` 包围程序设值，但没有在查询更新时跳过用户正在拖动的情形。
新写页面可在 `isSliderDown()` 时暂缓同步；这也是建议，不是已有保障。
`onPlaybackStateChanged()` 当前没有实际 UI 更新，不应宣称按钮已同步禁用/切换图标。

完整座舱的可选主机观察命令：

```bash
cmake -S "$PROJECT" -B "$HOST_BUILD" -DSMART_COCKPIT_USE_QT6=OFF -DBUILD_TESTING=ON
cmake --build "$HOST_BUILD" --target smart-cockpit --parallel 2
"$HOST_BUILD/smart-cockpit" --sim --media-dir "$WORK/audio" --log-dir "$WORK/logs"
```

这需要完整项目的 Qt5 DBus/Test、GStreamer app/video 等依赖；运行后点击“媒体”。
也可以不传 `--media-dir`，在同一终端 `export SMART_COCKPIT_MEDIA_DIR="$WORK/audio"`。

## 7. 三个已知缺陷：现状、建议与并排测试

下面是核对源码得到的现状，不是本教程已经完成的修复。
只在 WORK 中自己的实现或副本尝试建议，保留原服务作对照。

| 问题 | 当前为什么会发生 | 建议实现 | 原版/练习版并排测试 |
| --- | --- | --- | --- |
| 曲名换了，音频没换 | 播放中直接设置 URI，没有先退回可重建流的状态 | 换曲先退 READY/NULL，确认转换，再设 URI 并请求 PLAYING | A 播放中 Open B；原版可能仍播 A，练习版应确实切 B |
| 播放继续，进度停住 | `pollBus()` 没消息就提前 return，查询代码根本没运行 | 先处理消息，再独立查询；即使没有消息也查询位置/时长 | 连续播放 20 秒；对照声音与 position 日志，练习版应周期增长 |
| 同长度曲目时长归零后不恢复 | openFile 发 -1，却没重置 `m_durationMs`；相同时长被缓存判断跳过 | 每次成功准备新曲时重置时长缓存并重新查询 | 先 Stop，再打开同毫秒时长 B；原版可能只发 -1，练习版应再发有效时长 |

进一步定位时不要一次改三个地方：

1. 先用不同内容的 A/B 测直接换曲；标题只证明 `trackChanged` 已触发。
2. 用“Stop 后 Open B”作对照，绕开直接换 URI 的影响，继续检查时长缓存。
3. 相同时长测试最好用同采样率、同采样数的两段 WAV，避免编码延迟使长度不同。
4. 查询测试使用较长音频；暂停期间位置不增长是正确行为，不是进度缺陷。
5. 修改 bus 循环时可以设置每轮处理上限，避免消息洪水饿死 GUI；查询仍要执行。
6. 状态转换可能异步，不在 GUI 线程无限等待；用状态消息推进换曲流程。

**源码摘录：`pollBus()` 的关键提前返回。**

```cpp
if (!message)
    return;
```

源码虽然注释写着“每轮刷新”，实际查询在这段后面；注释不能替代控制流分析。
当前每次 timer 最多取一条匹配消息，也不是已经排空整个 bus。
不要通过额外制造 TAG/STATE 消息掩盖查询问题，那会让故障变得更难复现。

## 8. 无音频硬件：用 fakesink 验证到哪一层

先用 GStreamer 命令行验证授权本地文件可解码；不改服务的私有 `m_audioSink`。
`sync=true` 让假输出按时间节奏消费，避免文件瞬间跑完；它始终不会产生声音。

```bash
AUDIO="$WORK/audio/A.wav"
URI="$(python3 -c 'import pathlib,sys; print(pathlib.Path(sys.argv[1]).resolve().as_uri())' "$AUDIO")"
gst-launch-1.0 -m playbin uri="$URI" audio-sink="fakesink sync=true" video-sink=fakesink
```

预期：管线进入播放并最终 EOS，无扬声器输出；它不证明 ALSA、音量或实际 MediaService 正常。
当前服务没有公开 sink setter，也没有 `--audio-sink` 参数或对应环境变量开关。
若要测自己的服务，可在 WORK 版中设计可注入 sink，并只让测试使用 fakesink。
不能凭这条 CLI 成功，就在验收表中勾选“有声”和“三个媒体缺陷已修复”。

## 9. 动手练习、排错与验收

**练习 A：文件与标题分离。** 使用带空格/中文文件名的授权音频，双击播放。
预期 UserRole 路径仍正确；有 TAG title 时标题可以不同于文件名，但路径不能改变。
把另一首放进子目录，预期当前非递归列表不显示；重启页面后新增顶层文件才会被重新扫描。

**练习 B：状态与进度。** 用观察器逐个操作 Pause、Resume、Seek 10 seconds、Stop。
预期暂停保留进度、继续后推进、合法 seek 后接近目标；记录不支持 seek 或查询停滞的情况。
不要把原版已知缺陷当作练习失败，更不要把尚未验证的修复目标当成结果。

**练习 C：写自己的 MediaPage。** 先用普通按钮与 QLabel 接线，最后再做自绘封面和图标。
完成第 7 节的对照表后，再考虑空列表提示、缺失文件提示、拖动期间禁止抢滑块等改进。
当前没有专门的媒体服务回归测试；[UI 冒烟测试](../tests/tst_ui.cpp) 的页面绘制也不验证解码与声音。

| 现象 | 排查顺序 |
| --- | --- |
| 列表为空 | 路径是否正确、文件是否在顶层、扩展名是否符合过滤器 |
| open 返回 false | 是否 init、文件是否存在；随后再检查管线错误 |
| 有标题但没声音 | 是否实际换曲、插件是否齐全、ALSA 设备/音量是否可用 |
| 时间不走 | 先检查 bus early return，不要先给页面再加一个补丁 timer |
| 新曲时长一直为零 | 检查 -1 通知与 `m_durationMs` 缓存是否同步重置 |
| seek 无效 | 文件是否可 seek、单位是否为毫秒、底层调用是否成功 |
| fakesink 测试“无声” | 这是预期，不是音频硬件故障结论 |

验收清单：

- [ ] 能解释 openFile 已请求 PLAYING，以及 stop 和 shutdown 的差别。
- [ ] 完整观察器可构建；信号日志和听觉观察分开记录。
- [ ] 能找到非递归扫描、UserRole、环境变量和命令行参数的接线位置。
- [ ] 三个缺陷都有复现步骤、建议方案与未修复/已验证的明确标记。
- [ ] 明确 fakesink 只验证解码链，不把它当作有声测试。
- [ ] 所有练习和音频在 WORK，未自动改动项目源码，也未运行整套测试。

[返回学习路线](README.md) · [上一章：空调命令闭环](07-climate-command-flow.md) · [下一章：摄像头与倒车](09-camera-and-reverse.md)
