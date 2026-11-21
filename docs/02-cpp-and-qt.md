# 02 · C++ 与 Qt：从空文件到第一扇窗口

配套：[本章练习题](exercises/02-cpp-and-qt.md) · [全部题集](exercises/README.md)

[学习路线](README.md) · 上一章：[环境准备](01-environment.md) · 下一章：[工程骨架](03-project-skeleton.md)

## 本章目标

自己创建一个只有两个源配置文件的程序：按钮切换运行状态，定时器推动模拟车速，标签显示数值。此处**不依赖参考仓库源码**，也不用 CAN 或 GStreamer。

## 1. 先掌握这几种 C++ 写法

| 写法 | 理解方式 | 项目里的例子 |
| --- | --- | --- |
| `int speed = 0;` | 给有类型的数据一个名字 | 风量用整数档位 |
| `bool running = false;` | 真/假状态 | 是否有空调请求等待确认 |
| `struct` / `class` | 将数据或数据与行为组织起来 | `ClimateState` / `CarService` |
| `const T &value` | 借用对象，只读，不复制整个对象 | 传递图像或状态 |
| `T *pointer` | 保存对象地址，不自动说明所有权 | 页面持有服务指针 |
| `enum class` | 有名字的有限状态，避免数字混用 | `PageId::Climate` |
| `std::vector` | 长度可变的连续序列 | CAN 原始字节 |
| `std::unordered_map` | 通过键查值 | 信号名到物理值 |
| `namespace sc` | 给项目名字加作用域 | `sc::CarService` |

`.h` 通常声明“有什么”；`.cpp` 通常实现“怎么做”。编译器分别处理 `.cpp`，链接器再把实现和引用拼起来。`#include` 是把声明引入当前编译单元，不是自动编译对应的 `.cpp`。

### 值、引用、指针不要混淆

```cpp
int a = 10;
int b = a;
int &alias = a;
int *address = &a;
b = 20;
alias = 30;
*address = 40;
```

这是语法片段：最后 `a` 为 40，`b` 为 20。引用和指针都可能访问同一个对象；把地址交给其他对象，不会让被指向对象自动活得更久。

Qt 页面借用 `CarService*` 时，创建者必须保证服务仍然存在。下一章会通过对象创建顺序和 QObject 父子关系解释如何做到。

### Lambda 是可以传给别人的小函数

```cpp
int value = 0;
auto increment = [&value]() { ++value; };
increment();
```

`[&value]` 表示按引用捕获外部变量；函数执行时修改原变量。若回调比变量活得更久，就会访问失效对象。因此后面的 `connect` 不只给函数，还给一个 `QObject` 上下文，让连接随上下文销毁而断开。

## 2. 事件循环：GUI 不是一条执行到底的脚本

传统小程序可能“输入 → 计算 → 输出 → 退出”。GUI 则要持续等鼠标、键盘、计时器、窗口重绘和其他对象的通知。

```text
创建 QApplication
创建窗口和控件
建立连接
显示窗口
app.exec() 处理事件
窗口关闭后退出循环
对象按生命周期清理
```

`app.exec()` 等待事件不是程序卡死。真正的卡死往往是你在主线程槽函数里放了长时间 `while`、文件处理或 `sleep`，使事件循环没有机会继续工作。

## 3. 创建两个完整文件

确认下列练习目录没有你要保留的同名文件后创建它；使用编辑器录入代码，不要覆盖参考仓库。

```bash
WORK="$HOME/smart-cockpit-lab"
mkdir -p "$WORK/02-first-window"
```

### 完整文件：`02-first-window/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)
project(first_window LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets)
add_executable(first-window main.cpp)
target_link_libraries(first-window PRIVATE Qt5::Widgets)
```

这个实验只用 Qt 已有的类，没有自己声明 `Q_OBJECT` 类，因此暂时不需要 AUTOMOC。下一章再加入。

### 完整文件：`02-first-window/main.cpp`

```cpp
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    int speed = 0;
    bool running = false;
    QWidget window;
    window.setWindowTitle(QStringLiteral("我的第一个座舱实验"));
    window.resize(400, 240);

    auto *layout = new QVBoxLayout(&window);
    auto *label = new QLabel(QStringLiteral("0 km/h"), &window);
    auto *button = new QPushButton(QStringLiteral("开始"), &window);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    layout->addWidget(button);

    QTimer timer;
    timer.setInterval(100);
    QObject::connect(button, &QPushButton::clicked, &window,
                     [&running, button]() {
        running = !running;
        button->setText(running ? QStringLiteral("暂停") : QStringLiteral("开始"));
    });
    QObject::connect(&timer, &QTimer::timeout, &window,
                     [&speed, &running, label]() {
        if (!running)
            return;
        speed = (speed + 1) % 121;
        label->setText(QStringLiteral("%1 km/h").arg(speed));
    });
    timer.start();
    window.show();
    return app.exec();
}
```

### 编译运行

```bash
cmake -S "$WORK/02-first-window" -B "$WORK/build-02"
cmake --build "$WORK/build-02" -j2
"$WORK/build-02/first-window"
```

预期：初始是 `0 km/h`；点“开始”，每次计时事件增加 1，120 后回到 0；点“暂停”停止变化；关闭窗口退出。

计时器设为 100ms，不代表操作系统保证精确每 100ms 执行。实际触发受事件循环和系统调度影响，它不是硬实时车速测量。

## 4. 逐步理解刚写的程序

### QApplication 和 QWidget

`QApplication` 管理图形应用环境，必须先于窗口创建。`QWidget` 是可显示部件的基础类；这里它没有父控件，所以是顶层窗口。

`show()` 让窗口变为可见，真正的绘制由后续事件驱动，不应理解成直接立即画完每个像素。

### 布局不是固定坐标

`new QVBoxLayout(&window)` 把纵向布局安装到窗口。`addWidget()` 让布局根据可用尺寸分配子控件空间。拖动窗口边缘观察布局变化；后面的横竖屏适配也是在这个基础上做，不是简单给所有控件写固定 x/y。

### QObject 父子关系

这里 `label`、`button` 以 `&window` 为父对象，窗口销毁会销毁它们。不要再无条件手动 `delete` 一遍。

`timer` 是栈对象，作用域结束自动销毁。Qt 程序不是“所有东西都必须 new”，也不是“只要设置了 parent 就不用考虑生命周期”：尤其不能把一个较早创建的栈对象挂到较晚创建、会先销毁的父对象上。

### 信号与槽

```text
按钮发 clicked      → lambda 切换 running
计时器发 timeout    → lambda 读取 running 并更新 label
```

这里两者都在主线程，默认连接会直接调用接收函数。信号不是另开一个线程，也不是一定异步。

`connect(sender, signal, context, lambda)` 中的 `context` 是 `&window`：窗口销毁后不再调用这些连接。仍要保证 lambda 捕获的其他数据也活得足够久；本例状态变量在窗口前创建，销毁顺序相反。

## 5. 元对象系统先有一个印象

下一章你会自己声明：

```cpp
signals:
    void speedChanged(int speed);
```

这不是让你手写函数体，而是由 Qt 的元对象工具 `moc` 处理。自定义 QObject 类写 `Q_OBJECT`，CMake 开 `AUTOMOC`，再把头文件加入目标，才能可靠生成和链接所需代码。

普通 C++ 类、lambda，以及本章对 Qt 已有类的使用不要求你自己声明 `Q_OBJECT`。不要把 moc、编译器和链接器当成同一个工具。

## 6. 三个小练习

1. 增加一个“复位”按钮：点下后速度立即回到 0，但不改变运行/暂停状态。两个按钮都放进布局。
2. 速度大于 100 时显示“请减速”，否则显示正常数字。先保证逻辑正确，再考虑样式。
3. 故意在计时回调里做耗时计算，观察按钮和重绘变慢，然后删掉它。解释为什么把所有任务都塞进主线程不行；不要把死循环留在程序中。

完成复位后，思考：如果仪表、日志和另一个页面都要知道速度，难道每个地方都复制一份 `speed` 吗？03 会把数据移到独立服务中，让一个信号通知多个观察者。

## 常见问题与验收

| 现象 | 检查 |
| --- | --- |
| 窗口一闪而过 | 是否调用并返回 `app.exec()` |
| 按钮点了没变化 | `connect` 是否建立，lambda 是否修改同一个状态 |
| 改了代码却还是旧效果 | 是否重新构建、是否运行正确构建目录里的程序 |
| `QObject::connect` 编译错误 | 参数类型、`&类名::信号` 写法、捕获变量是否正确 |
| 没有中文 | 字体与源文件 UTF-8 编码 |

- [ ] 不看代码也能描述点击和计时两条事件链。
- [ ] 能区分栈对象、QObject 所有权和借用指针。
- [ ] 能独立完成复位按钮，而非直接改参考项目。
- [ ] 能解释 `update()`/事件循环与一个死循环的区别。

继续 [03 工程骨架](03-project-skeleton.md)，把一个文件拆成可扩展的工程。
