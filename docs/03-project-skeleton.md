# 03 · 工程骨架：把状态和窗口分开

配套：[本章练习题](exercises/03-project-skeleton.md) · [全部题集](exercises/README.md)

[学习路线](README.md) · 上一章：[C++ 与 Qt](02-cpp-and-qt.md) · 下一章：[CAN 与 DBC](04-can-and-dbc.md)

## 本章目标

从新的空目录写一个小工程：服务保存车速，窗口只负责显示和操作，通过自定义信号连接二者。本章仍然不依赖参考项目，完整代码可以单独编译。

## 1. 为什么不要把代码全塞进 main

02 中速度、定时器、按钮都在一起。加入更多页面后，会出现“多个控件各存一份速度”“日志直接读取窗口内部成员”等问题。

先建立单向依赖：

```text
main：创建并连接对象
  ├── DemoVehicle：保存唯一的车速状态，发 speedChanged
  └── DashboardWindow：借用 DemoVehicle，显示变化、请求操作

服务不知道有哪个窗口；窗口知道服务的公共接口。
```

这与成品里 `CarService → InstrumentPage` 的思路一致，但这里只模拟一个整数，先不处理协议。

## 2. 创建文件

```bash
WORK="$HOME/smart-cockpit-lab"
mkdir -p "$WORK/03-skeleton"
```

在这个目录创建下面六个完整文件。已有同名作业时请另选目录。

### 完整文件：`03-skeleton/DemoVehicle.h`

```cpp
#pragma once
#include <QObject>
#include <QTimer>

class DemoVehicle : public QObject
{
    Q_OBJECT
public:
    explicit DemoVehicle(QObject *parent = nullptr);
    int speed() const;

public slots:
    void setRunning(bool running);
    void reset();

signals:
    void speedChanged(int speed);

private:
    void setSpeed(int speed);
    int m_speed = 0;
    QTimer m_timer;
};
```

### 完整文件：`03-skeleton/DemoVehicle.cpp`

```cpp
#include "DemoVehicle.h"

DemoVehicle::DemoVehicle(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(100);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        setSpeed((m_speed + 1) % 121);
    });
}

int DemoVehicle::speed() const
{
    return m_speed;
}

void DemoVehicle::setRunning(bool running)
{
    if (running)
        m_timer.start();
    else
        m_timer.stop();
}

void DemoVehicle::reset()
{
    setSpeed(0);
}

void DemoVehicle::setSpeed(int speed)
{
    if (m_speed == speed)
        return;
    m_speed = speed;
    emit speedChanged(m_speed);
}
```

### 完整文件：`03-skeleton/DashboardWindow.h`

```cpp
#pragma once
#include <QWidget>

class DemoVehicle;
class QLabel;

class DashboardWindow : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardWindow(DemoVehicle *vehicle, QWidget *parent = nullptr);

private slots:
    void showSpeed(int speed);

private:
    QLabel *m_speedLabel;
};
```

### 完整文件：`03-skeleton/DashboardWindow.cpp`

```cpp
#include "DashboardWindow.h"
#include "DemoVehicle.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

DashboardWindow::DashboardWindow(DemoVehicle *vehicle, QWidget *parent)
    : QWidget(parent), m_speedLabel(new QLabel(this))
{
    setWindowTitle(QStringLiteral("分层座舱实验"));
    resize(400, 260);
    auto *layout = new QVBoxLayout(this);
    auto *runButton = new QPushButton(QStringLiteral("运行 / 暂停"), this);
    auto *resetButton = new QPushButton(QStringLiteral("复位"), this);
    runButton->setCheckable(true);
    m_speedLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_speedLabel);
    layout->addWidget(runButton);
    layout->addWidget(resetButton);

    connect(vehicle, &DemoVehicle::speedChanged, this, &DashboardWindow::showSpeed);
    connect(runButton, &QPushButton::toggled, vehicle, &DemoVehicle::setRunning);
    connect(resetButton, &QPushButton::clicked, vehicle, &DemoVehicle::reset);
    showSpeed(vehicle->speed());
}

void DashboardWindow::showSpeed(int speed)
{
    m_speedLabel->setText(QStringLiteral("%1 km/h").arg(speed));
}
```

### 完整文件：`03-skeleton/main.cpp`

```cpp
#include <QApplication>
#include "DemoVehicle.h"
#include "DashboardWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    DemoVehicle vehicle;
    DashboardWindow window(&vehicle);
    window.show();
    return app.exec();
}
```

### 完整文件：`03-skeleton/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)
project(cockpit_skeleton LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets)
add_executable(cockpit-skeleton
    main.cpp
    DemoVehicle.h DemoVehicle.cpp
    DashboardWindow.h DashboardWindow.cpp
)
target_link_libraries(cockpit-skeleton PRIVATE Qt5::Widgets)
```

### 编译并运行

```bash
cmake -S "$WORK/03-skeleton" -B "$WORK/build-03"
cmake --build "$WORK/build-03" -j2
"$WORK/build-03/cockpit-skeleton"
```

预期：运行按钮选中后车速增加；取消选中后暂停；复位立即显示 0。运行中复位后，下一次计时继续增加。窗口关闭后正常退出。

## 3. 自定义信号是怎样工作的

`DemoVehicle.h` 声明 `speedChanged`；`setSpeed` 中 `emit`；窗口建立连接后收到数值。

`Q_OBJECT` 让类参与 Qt 元对象系统，CMake 的 AUTOMOC 调用 moc 生成元对象代码。**不要自己实现 `speedChanged()` 函数体**。若出现 vtable 或信号符号的链接错误，检查 AUTOMOC、头文件是否进入目标和构建目录是否属于当前工程。

`setSpeed()` 只在值真的变化时通知，类似成品 `CarService` 的处理：避免重复刷新不是靠“少发信号”的口号，而是由这个具体比较保证。

窗口构造函数最后读取一次 `vehicle->speed()`，解决“订阅前已经有状态，但之后没有变化信号”的情况。状态查询与变化通知各有用途。

## 4. 所有权和生命周期

`main` 先创建服务，再创建窗口；退出时窗口先销毁、服务后销毁。窗口借用的指针在窗口生命周期内有效。

`m_timer` 是服务的值成员，随服务销毁；按钮和标签是窗口的 QObject 子对象，由窗口销毁。头文件里的 `class DemoVehicle;` 是前置声明：只存指针时暂时不需要完整类定义，调用方法的 `.cpp` 再 include 真正头文件。

本章所有对象都留在主线程。**不要把这个有值成员 QTimer 的教学类随意 moveToThread 后就声称线程安全**；跨线程的对象归属、定时器归属与访问规则需要重新设计。

## 5. 走向参考工程的三层目标

现在一个可执行目标足够。完整工程把共同代码放进库，程序和测试复用同一套实现：

| 目标 | 源码 | 为什么拆开 |
| --- | --- | --- |
| `smart_cockpit_cxx` | [CanFrameParserCore](../src/core/CanFrameParserCore.h)、[AsyncLoggerCore](../src/logging/AsyncLoggerCore.h) | 算法与日志线程不必依赖 Qt |
| `smart_cockpit_core` | Qt 适配、车辆/外设服务、UI | 应用与 Qt 测试复用 |
| `smart-cockpit` | [main.cpp](../src/main.cpp) | 只负责组装与进退场 |

`target_include_directories` 告诉编译器找头文件；`target_link_libraries` 告诉链接器目标依赖哪些库。`PUBLIC` 依赖会传给使用者，`PRIVATE` 则不作为公共使用要求向外传播。

CMake 不是“目录下所有 cpp 自动编译”。新写 `.cpp` 后要把它加入目标；不要只创建文件却忘记构建清单。

## 6. 练习：先改结构，不急着美化

1. 在窗口上加第二个标签，显示同一车速换算出的 m/s。只读服务，不在标签里独立模拟速度。
2. 给服务加 `reset()` 的测试：连续复位两次，第二次不应再发变化信号。11 会介绍 `QSignalSpy`。
3. 将 `DemoVehicle` 放进 `core/`、窗口放进 `ui/`，更新 CMake 源文件路径与 include 路径，再编译一次。
4. 用自己的话画出“用户按复位 → 服务改变 → 信号 → 两个标签更新”的箭头，不允许从服务直接调用窗口方法。

## 验收

- [ ] 能从空目录构建这六个文件，而不是依赖参考仓库的隐式头文件路径。
- [ ] 服务中没有 `QLabel` 或 QWidget 指针。
- [ ] 知道源文件漏编译与库漏链接是两个问题。
- [ ] 知道当前分层没有自动增加线程，也没有自动保证所有共享数据安全。

下一章 [CAN 与 DBC](04-can-and-dbc.md) 将把这里的定时整数替换为真正有位布局的车辆数据。完整重建时使用 [13 的文件施工顺序](13-build-from-zero.md)。
