# 06｜从一个自绘控件到仪表盘

配套：[本章练习题](exercises/06-widgets-and-dashboard.md) · [全部题集](exercises/README.md)

[返回学习路线](README.md) · [上一章：车辆服务与模拟器](05-vehicle-service-and-simulator.md) · [下一章：空调命令闭环](07-climate-command-flow.md)

## 1. 先修、目标与边界

本章沿用系列环境 **Qt 5.15、C++17、CMake ≥ 3.21**，不需要开发板或 CAN 设备；小例子的较低最低版本声明不代表完整项目支持所有旧版本。
先会创建目录、运行终端命令，并了解类、成员函数和 Qt 信号槽。
建议先读上一章的数据流；没有上一章的练习代码，也能独立运行本章完整例子。
这里不引用 01—03 中任何假定已经存在的教学类。

完成后，你应该能够：

- 从空目录写出一个可以编译、缩放、响应滑块的自绘仪表。
- 解释 QWidget、布局、父子所有权和服务借用指针的区别。
- 将车速、转速、电量显示接到现有 CarService，而不是在绘图中读 CAN。
- 分清“界面显示名称”和“车辆协议实际提供的信号”。

代码块标记规则：**完整例子**可按标注文件名保存；**源码摘录**只解释局部逻辑。
实现清单是给你在仓库外重新编写模块的顺序，不是覆盖项目文件的指令。

## 2. 准备目录，先确定写什么

在终端设置以下变量；后续章节沿用同样约定：

```bash
export WORK="$HOME/smart-cockpit-lab"
export PROJECT=/home/air/qt/Smart_Cockpit_based_on_tspi
export HOST_BUILD="$HOME/build/smart-cockpit-host"
mkdir -p "$WORK/widgets" "$HOME/build"
```

`PROJECT` 只用于读源码；`WORK` 放练习；`HOST_BUILD` 放完整项目的主机构建产物。
本节小例子的构建目录单独放在 `$WORK/widgets/build`，不要混入项目 `build/`。
主机需准备 C++ 编译器、CMake 和 Qt5 Widgets 开发包，例如 Debian 系的 `qtbase5-dev`。

建议按这个顺序实现，不要第一步就照抄整个 MainWindow：

| 顺序 | 在练习中创建/实现什么 | 这一轮只验证什么 |
| --- | --- | --- |
| 1 | `widgets/CMakeLists.txt`、`widgets/main.cpp` | 一个滑块带动一个表盘 |
| 2 | 将控件拆成自己的 `GaugeWidget.h/.cpp` | 接口与绘图分离 |
| 3 | `Theme.h/.cpp`、`BatteryWidget.h/.cpp` | 统一颜色、电量阈值 |
| 4 | `InstrumentPage.h/.cpp` | 卡片布局与信号连接 |
| 5 | `MainWindow.h/.cpp` | 顶栏、页面栈、导航、状态栏 |

第 1 步有完整文件；后续步骤对照源码接口自行实现，不要求复制现有整文件。

## 3. 完整例子：不用任何项目类的最小仪表

**完整例子文件：`$WORK/widgets/CMakeLists.txt`**。

```cmake
cmake_minimum_required(VERSION 3.16)
project(widget_lab LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets)
add_executable(widget-lab main.cpp)
target_link_libraries(widget-lab PRIVATE Qt5::Widgets)
```

**完整例子文件：`$WORK/widgets/main.cpp`**。
这里不用自定义信号或元对象槽，因此类不需要 `Q_OBJECT`，也不需要 AUTOMOC。
此例刻意不做缓存，先掌握绘制闭环；它不是仓库 GaugeWidget 的复制品。

```cpp
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>
#include <QtMath>

class MiniGauge final : public QWidget
{
public:
    explicit MiniGauge(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(240, 240);
    }

    void setValue(double value)
    {
        m_value = qBound(0.0, value, 240.0);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#0A0E14"));
        const qreal side = qMin(width(), height());
        p.translate(width() / 2.0, height() / 2.0);
        p.scale(side / 200.0, side / 200.0);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#161B22"));
        p.drawEllipse(QRectF(-88, -88, 176, 176));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#484F58"), 5));
        p.drawArc(QRectF(-76, -76, 152, 152), 225 * 16, -270 * 16);
        const double angle = 225.0 - m_value / 240.0 * 270.0;
        const double radians = qDegreesToRadians(angle);
        p.setPen(QPen(QColor("#2F81F7"), 3));
        p.drawLine(QPointF(0, 0),
                   QPointF(66 * qCos(radians), -66 * qSin(radians)));
        p.setBrush(QColor("#E6EDF3"));
        p.drawEllipse(QPointF(0, 0), 4, 4);
        QFont f = font();
        f.setPixelSize(16);
        p.setFont(f);
        p.setPen(QColor("#E6EDF3"));
        p.drawText(QRectF(-80, 32, 160, 30), Qt::AlignCenter,
                   QString::number(m_value, 'f', 0) + " km/h");
    }

private:
    double m_value = 0;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *gauge = new MiniGauge(&window);
    auto *slider = new QSlider(Qt::Horizontal, &window);
    slider->setRange(0, 240);
    layout->addWidget(gauge, 1);
    layout->addWidget(slider);
    QObject::connect(slider, &QSlider::valueChanged, gauge,
                     [gauge](int value) { gauge->setValue(value); });
    slider->setValue(120);
    window.setWindowTitle(QStringLiteral("仪表练习"));
    window.resize(400, 440);
    window.show();
    return app.exec();
}
```

保存这两个完整文件，再执行：

```bash
cmake -S "$WORK/widgets" -B "$WORK/widgets/build" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$WORK/widgets/build" --parallel 2
"$WORK/widgets/build/widget-lab"
```

预期：深色圆盘、蓝色指针和 `120 km/h`；拖动滑块，读数和指针同步变化。
0 在左下、120 在正上、240 在右下；放大窗口后表盘仍是圆形。
窗口依赖桌面显示环境；无桌面的 offscreen 运行不等于已目视验收。

## 4. 拆解自绘闭环：数据不直接画到屏幕上

对照 [GaugeWidget.h](../src/ui/GaugeWidget.h) 和 [GaugeWidget.cpp](../src/ui/GaugeWidget.cpp)。

1. `configure()` 保存最小值、最大值、单位、红区、主刻度数与小数位。
2. `setValue()` 只限幅并保存最新数值，然后请求 `update()`。
3. Qt 事件循环合并重绘请求，适当时机调用 `paintEvent()`。
4. `paintEvent()` 读取状态，用 QPainter 绘制；不在这里请求车辆数据。

**源码摘录：`GaugeWidget::setValue()`，不是完整文件。**

```cpp
void GaugeWidget::setValue(double value)
{
    m_value = qBound(m_min, value, m_max);
    update();
}
```

不要把 `update()` 当成马上执行一次绘图，也不要在绘图函数里再次无条件 `update()`。
仓库 GaugeWidget 没有独立动画 timer：显示节奏来自外部数值更新。
模拟器每 100 ms 推送一轮数据；这不表示控件内部插值出了 60 fps 动画。

几何换算的核心是：

- 比例 `fraction = (value - min) / (max - min)`；先保证合法量程。
- 角度 `angle = 225 - fraction * 270`，终点为 -45°。
- Qt `drawArc()` 用 1/16 度，所以传入角度要乘 16。
- 数学坐标向上为正，屏幕 y 向下，因此示例用 `-sin(angle)`。
- 仓库的指针是预先构建的朝上路径，绘制时平移并旋转 `90 - angle`。

练习拆类时先支持 `0..240`，验证三个端点后，再扩展可配置量程。
不要让 `majorTicks` 为零；也不要用等值或反向的 min/max 配置测试正常绘图。

## 5. 静态缓存：为什么每次只动指针

仓库将表盘、背景轨道、红区、刻度和标题画入 `QPixmap m_staticLayer`。
每次 `paintEvent()` 先贴静态层，再画数值弧、指针、中心点、数字和单位。
缓存同时保存几何位置、字体和指针路径，避免每帧重新构造这些对象。

实现顺序建议：先无缓存画对，再拆出 `rebuildCache()`，最后测量性能。
`configure()`、`setLabel()` 和 `resizeEvent()` 都会重建缓存；空缓存会在绘制时补建。
`setValue()` 不重建缓存，否则就失去了分层的意义。

高 DPI 要区分逻辑尺寸和像素尺寸：

- 仓库按 `size() * devicePixelRatioF()` 创建像素图。
- 再调用 `setDevicePixelRatio(dpr)`，让 QPainter 按逻辑尺寸使用它。
- resize 会触发重建；DPR 改变也意味着旧缓存应失效。
- **当前源码没有专门监听 DPR 变化**，不能保证无 resize 的跨屏切换必然重建。
- 若练习需要跨不同缩放屏幕，比较缓存 DPR 并补充失效处理；这是建议，不是现状。

QPixmap 和 QWidget 的绘制留在 GUI 线程；不要为了“优化”在 CAN 回调线程操作控件。

## 6. 主题、对象名与电量不是三件互不相关的事

阅读 [Theme.h](../src/ui/Theme.h)、[Theme.cpp](../src/ui/Theme.cpp)。
`Bg=#0A0E14`、`Card=#161B22`、`Accent=#2F81F7`，基础间距 `SpacingUnit=8`。
先统一这些 token，再安排 8、16、24 像素的间距，不要逐个控件凭感觉配色。
QSS 管普通控件，QPainter 自绘部分从 Theme 常量取色；QSS 不会替你改自绘画笔。

`setObjectName("card")` 对应 `QFrame#card`；类型、对象名任一不匹配，规则都可能失效。
动态属性选择器例如 `QLabel#canStatus[connected="true"]` 用于表达状态。
改变属性后，仓库显式让样式重新计算，而不是只改文字。

**源码摘录：MainWindow 的连接状态处理局部。**

```cpp
m_canStatus->setProperty("connected", connected);
m_canStatus->style()->unpolish(m_canStatus);
m_canStatus->style()->polish(m_canStatus);
```

这段依赖已有 `m_canStatus`、`connected` 和 `<QStyle>`，不能作为独立程序粘贴运行。

阅读 [BatteryWidget.cpp](../src/ui/BatteryWidget.cpp)：输入限幅到 0—100。
颜色按顺序判断：`<=15` 红色，`<=30` 橙色，其余使用 Accent 蓝色。
外壳、端子和标题可缓存；填充宽度、颜色、百分比每次根据当前值绘制。

**协议边界：**当前界面的“电量”来自 `fuel_level → fuelChanged → BatteryWidget`。
它不是独立 SOC 协议，也不能由这个标签推断项目已经接入真实 BMS。
“电机转速”“电机”警告标签仍映射 `engine_rpm`、`mil_engine`。
不要把显示文案里的 motor/电机写成一个已经存在的独立真实电机服务。

## 7. 组装页面，再组装窗口

阅读 [InstrumentPage.cpp](../src/ui/InstrumentPage.cpp) 时，按三层拆解：

1. 创建车速表、转速表、电量、挡位、指示灯，并设置稳定的 objectName。
2. 放进卡片与布局；车速、转速、状态卡使用 47:30:20 的伸展权重。
3. 连接 CarService 信号，再读取初始 speed/rpm/fuel/gear 填充显示。

槽函数只负责转发，如 `onSpeedChanged(kmh)` 调用 `m_speedGauge->setValue(kmh)`。
危险警示灯打开时左右灯也亮；挡位 0、1、2、3 对应 P、R、N、D。
构造时指示灯先设为 false，随后由服务信号更新，不要误说所有灯都读取了初始缓存。

[MainWindow.cpp](../src/ui/MainWindow.cpp) 的实际构造顺序是：
顶栏 → stack/ScreenManager → 页面实例/registerPage → 导航 → 状态条。
教学中可以先画顶栏、导航、状态条的空壳，但装配顺序要保证依赖对象已经创建。
顶栏提供时钟与状态；中间 QStackedWidget 只显示一个页面；底部负责导航和消息。
导航应向 ScreenManager 请求页面，倒车优先级约束将在第 09 章展开。

所有权要在创建对象时想清楚：

- `new QWidget(parent)` 由 Qt 父对象管理；布局和页面栈会组织/接管其子控件。
- `registerPage()` 中 `addWidget()` 把页面交给栈管理，不必再手动 delete 页面。
- 页面保存的 CarService、MediaService、CameraService 指针是借用，不是转移所有权。
- [main.cpp](../src/main.cpp) 先创建服务，再创建窗口，局部对象逆序析构使窗口先销毁。
- 事件循环结束后先停止服务，再离开 main；不能让页面继续访问已经销毁的服务。

## 8. 动手练习与预期表现

**练习 A：只改仓库外例子。** 输入 0、120、240，再从代码调用 -10 和 300。
预期越界值夹到端点；拖动时不闪烁，缩小到最小尺寸仍可读。
把颜色抽到自己的 Theme，再做第二个 `0..8000` 转速表，注意单位与比例分离。

**练习 B：完成自己的 BatteryWidget。** 依次输入 0、15、16、30、31、100。
预期分别为红、红、橙、橙、蓝、蓝；0 时无填充，但数字仍能读到。

**练习 C：追踪一帧数据。** 对照 [tst_ui.cpp](../tests/tst_ui.cpp) 的 `instrumentGaugesFollowCanFrames()`。
测试把 `7800b80b64010000` 注入 ICU_Dynamic，期望 120 km/h、3000 rpm、50%。
这是本进程调用 `CarService::onCanFrame()`，不是向真实车辆发送报文的操作。
先检查控件值，再检查绘制结果；“有亮像素”不等于颜色、布局、业务全部正确。

完整项目的可选观察命令如下；只写到外部构建目录，不自动更改源码：

```bash
cmake -S "$PROJECT" -B "$HOST_BUILD" -DSMART_COCKPIT_USE_QT6=OFF -DBUILD_TESTING=ON
cmake --build "$HOST_BUILD" --target smart-cockpit --parallel 2
mkdir -p "$WORK/logs"
"$HOST_BUILD/smart-cockpit" --sim --log-dir "$WORK/logs"
```

完整项目还需要 Qt5 DBus、Test 和 GStreamer 开发库，不能用“最小例子能编译”替代依赖检查。
桌面上应看到模拟车速与转速变化；模拟时间到 45 秒会进入倒车，52 秒退出。模拟器每次 timeout 累加 100ms，不按真实经过时间追赶；只有事件循环畅通时才约等于这些墙钟时间。
本章不要求运行整套测试；重点是先得到可解释的显示闭环。

## 9. 排错与验收

| 现象 | 优先检查 |
| --- | --- |
| 找不到 Qt5Config.cmake | Qt5 开发包或 Qt 安装前缀；不要误切到 Qt6 |
| 数字更新但指针不动 | 比例是否用了整数除法、角度和弧度是否混用 |
| 表盘被压成椭圆 | 是否用较短边统一缩放，而非分别缩放 x/y |
| 属性变了颜色没变 | objectName、QSS 类型选择器以及 unpolish/polish |
| 高 DPI 模糊或跨屏异常 | 缓存像素尺寸、DPR 标记和缓存失效时机 |
| 界面崩溃或跨线程警告 | 服务寿命、QObject 上下文、是否在 GUI 线程更新 |
| 没有数据显示 | 先确认信号到达，再检查槽和控件值，最后检查绘图 |

验收清单：

- [ ] 两个完整文件可以独立构建，不依赖任何前章教学类。
- [ ] 能解释 `setValue → update → paintEvent`，不把刷新说成独立动画。
- [ ] 三个表盘端点、六个电量边界输入都有明确结果。
- [ ] 能指出缓存包含什么、何时失效，以及当前 DPR 处理的边界。
- [ ] 能沿 CarService 信号找到页面槽，明确 fuel_level 和电机标签的真实含义。
- [ ] 所有练习留在 WORK，项目代码、配置和其他文档未因练习被覆盖。

[返回学习路线](README.md) · [上一章：车辆服务与模拟器](05-vehicle-service-and-simulator.md) · [下一章：空调命令闭环](07-climate-command-flow.md)
