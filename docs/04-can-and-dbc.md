[返回学习路线](README.md) · 前置：[03 项目骨架](03-project-skeleton.md) · 下一篇：[05 车辆服务与模拟器](05-vehicle-service-and-simulator.md)

# 04｜从字节到车速：亲手理解 CAN 与 DBC

配套：[本章练习题](exercises/04-can-and-dbc.md) · [全部题集](exercises/README.md)

本篇以 **Qt5、C++17、classic CAN（经典 CAN）** 为基准。
先不用 Qt、界面和 CAN 硬件：我们从几个字节算出车速，再编译一个纯 C++ 实验。
读完应能回答：为何 `78 00` 是 120，为什么短帧不能把仪表清零？
建议先读前一篇的构建基础；本篇实验不依赖前面练习的源文件。

## 1. 先准备一个不会弄坏项目的练习区

项目只作为只读参考；练习文件、二进制、构建缓存全部放在仓库外。
以下变量在系列教程中保持一致，`PROJECT` 可替换为你的实际项目路径。
每开一个新终端，重新执行变量定义。

```bash
PROJECT=/home/air/qt/Smart_Cockpit_based_on_tspi
HOST_BUILD="$HOME/build/smart-cockpit-host"
WORK="$HOME/smart-cockpit-lab"
mkdir -p "$WORK/04"
command -v g++
```

只需要支持 C++17 的编译器；本篇不会安装软件、创建 CAN 接口或发送总线帧。
下面说“新建文件”时，均指用编辑器保存到 `$WORK/04`，不是项目 `src`。
代码块分三类：**源码摘录**用于阅读，**伪代码**用于设计，**完整示例**才能整块保存编译。

## 2. 数制、bit 和 byte：先会读原始数据

- 十进制逢十进一：`120` 就是一百二十。
- 十六进制逢十六进一：数字为 `0..9、A..F`，C++ 通常写前缀 `0x`。
- `0x78 = 7 × 16 + 8 = 120`；`0x100 = 256`，不是十进制 100。
- 二进制只有 0 和 1；一个二进制位叫 **bit**。
- 8 bit 组成一个 **byte（字节）**，能表示无符号数 `0..255`。
- 一个十六进制数字表示 4 bit，因此 `78` 是一个字节，`7800` 是两个字节。

把 `0x78` 展开为 `01111000`，从右到左的位权为 `1、2、4、8、16、32、64、128`。
一个字节的最低位叫 bit0，最高位叫 bit7；不要把图上从左数第一个位置叫 bit0。
本文打印字节序列时，最左边是 byte0，其后是 byte1。

```text
字节内编号：  7 6 5 4 3 2 1 0
0x78 的位：  0 1 1 1 1 0 0 0
置位的位权：64 + 32 + 16 + 8 = 120
```

CAN 报文可先理解为“消息编号 ID + 数据长度 DLC + payload 字节”。
本项目使用标准 ID `0x100、0x200、0x300、0x400`，分别承载仪表、空调、挡位、底盘数据。
标准 CAN ID 有 11 bit；扩展 ID 有 29 bit。**ID 不属于 payload 的 8 字节**。
classic CAN 的数据最多 8 字节；本篇不讨论 CAN FD 的更长数据区。

## 3. 小端、大端不是把整帧随便翻转

### 3.1 Intel：起始位指向信号最低有效位

`startBit=0、bitLength=16、bigEndian=false` 表示从 byte0 的 bit0 开始。
依次走 bit0、bit1，走完 bit7 后转到下个字节 bit0。
因此 `78 00` 的原始值为 `0x00 × 256 + 0x78 = 120`。
`B8 0B` 的原始值为 `0x0B × 256 + 0xB8 = 11 × 256 + 184 = 3000`。

跨字节也必须逐位思考：Intel `7|2` 依次取 byte0.bit7、byte1.bit0。
数值 3 的二进制是 `11`，所以编码后为 **`80 01`**，不是 `03 00`。

### 3.2 Motorola：起始位指向信号最高有效位

DBC 的 Motorola 位编号仍以每字节 bit0..bit7 标记，但信号遍历方向不同。
先从起始位向 bit0 走，越过后转到**下个字节的 bit7**。
Motorola `7|16` 读取 byte0 的 7..0，再读取 byte1 的 7..0。
因此 **`12 34` → `0x1234` → 4660**。
Motorola `0|2` 先读 byte0.bit0，再读 byte1.bit7，数值 3 编码为 **`01 80`**。
这说明“大小端只是交换两个字节”仅适用于部分对齐信号，不是通用算法。

### 3.3 signed、factor、offset 各管一件事

无符号 16 bit 的范围是 `0..65535`；有符号补码 16 bit 的范围是 `-32768..32767`。
补码最高位为 1 时，按 16 bit 理解可用“无符号值减 65536”得到负数。
方向盘原始值 `9C FF` 按 Intel 读为 `0xFF9C = 65436`，再解释为 `-100`。
它的 factor 为 0.1，所以物理角度为 `-100 × 0.1 = -10°`。

```text
解码：physical = signed_or_unsigned_raw × factor + offset
编码：raw = round((physical - offset) / factor)
油量：raw=100，factor=0.5，offset=0 → 50%
```

factor 是比例，不是范围校验器。非零、有限的 factor 和可表示的数值应由调用者保证。
当前接口用 `double` 表达物理量；它通常只有 53 位整数精度，不能承诺任意 64 bit 整数精确往返。
当前解码末尾还会转换为 `int64_t`，不要把它当作完整的无符号 64 bit 数值接口。

## 4. 把手算规则写成 DBC

打开只读参考 [vehicle.dbc](../config/vehicle.dbc)，找到以下**源码摘录**，不是完整 DBC 文件：

```dbc
BO_ 256 ICU: 8 ICU
 SG_ speed : 0|16@1+ (1,0) [0|240] "km/h" ICU
 SG_ engine_rpm : 16|16@1+ (1,0) [0|8000] "rpm" ICU
```

逐项读第一条信号：
1. `BO_ 256` 声明十进制 ID 256，也就是 `0x100`。
2. `SG_ speed` 声明名字；车辆服务靠这个名字找值，不会猜测别名。
3. `0|16` 是 startBit 和 bitLength，不是两个字节偏移。
4. `@1+` 表示 Intel、无符号；`@0-` 表示 Motorola、有符号。
5. `(1,0)` 是 factor 与 offset；`[0|240]` 是描述性的最小最大值。
6. `"km/h"` 是单位；后面的节点名称不是本教程解析器的控制逻辑。

本项目不是完整 DBC 工具链，只实现 `BO_` / `SG_` 的有限子集。
未知行通常跳过，`CM_`、`VAL_` 等不会成为运行时规则；不要期待多路复用和属性语义。
`BO_` 的名字和 DLC 没有成为严格的帧长度约束；实际长度按信号布局判断。
加载同 ID 的 `BO_` 会立即清空该 ID 的旧表，随后逐条加入 `SG_`。
**加载非事务性**：后面一行失败不会撤销前面修改，不能写成“失败自动回到内置表”。
范围虽然被读入，但解析器不会自动拒绝超出 `[min|max]` 的物理量。

## 5. 从零实现时，按这个顺序拆问题

只在练习区写自己的版本；每完成一步，用下一节的完整实验作为对照。
不要一开始就把 Qt、线程和位运算放进同一个类。

### 第一步：先定义协议数据，不依赖 Qt

参考 [CanTypes.h](../src/core/CanTypes.h) 的 `CanSignalDef` 与 `FrameId`。
自己先写出 name、startBit、bitLength、bigEndian、isSigned、factor、offset。
再补 minVal、maxVal、unit；区别“协议描述”与“业务校验”。
验收点：只包含标准库头文件的程序也能创建一个信号定义。

### 第二步：先验布局，再读取任何一位

参考 [CanFrameParserCore.cpp](../src/core/CanFrameParserCore.cpp) 的 `requiredSignalBytes()`（本次核对位于第 84 行）。
以下为该函数的**源码摘录**，不能单独作为完整 `.cpp` 编译：

```cpp
if (def.startBit < 0 || def.startBit >= 64 ||
    def.bitLength < 1 || def.bitLength > 64)
    return 0;
const int needBytes = def.bigEndian
    ? def.startBit / 8 + (7 - def.startBit % 8 + def.bitLength + 7) / 8
    : (def.startBit + def.bitLength + 7) / 8;
return needBytes <= 8 ? needBytes : 0;
```

这里非负整数 `(n+7)/8` 就是向上取整 `ceil(n/8)`。
Intel 需要 `ceil((start+len)/8)` 字节。
Motorola 需要 `start/8 + ceil((7-start%8+len)/8)` 字节。
Motorola `0|2`：`0 + ceil((7+2)/8)=2`；不能误算为一个字节。
负 start、start≥64、len≤0、len>64，以及跨出第 8 字节的布局都拒绝。
即使调用者给了 16 字节缓冲区，也不能让非法 classic CAN 布局通过。

### 第三步：实现单信号，再实现整帧

参考同文件的 `decodeSignal()`、`encodeSignal()`、`decode()`、`encode()`。
以下是**设计伪代码**，不是可编译 C++：

```text
decode(id, data, out):
    查找 id；未知则失败
    检查该帧所有字段的布局和所需长度；任何失败都直接返回
    清空 out
    逐字段取位 → 有符号解释 → 比例变换 → 写入 out
```

先检查**所有字段**再清 `out`，因此失败时旧输出保持不变。
单独的 `decodeSignal()` 错误返回 `0.0`，与合法零无法区分；业务应优先用整帧 bool 返回值。
`encodeSignal()` 先验布局、后扩容，非法布局不会先破坏输出。
整帧 `encode()` 则建立固定 8 字节输出，未提供的已知字段维持零位，未知名称不会被编码。
实际服务不能仅凭“encode 返回 true”认定所有业务字段都存在。

有符号扩展和掩码要避免用有符号 `1` 左移到高位。
本实现使用 `std::uint64_t(1)` 构造掩码，特别是 63 bit 信号；64 bit 情况另行跳过该移位。
这是避免危险移位的细节，不是“任意 double 都能安全编码成 64 bit”的承诺。

### 第四步：最后加 Qt 包装

参考 [Core 接口](../src/core/CanFrameParserCore.h) 和 [Qt 包装接口](../src/core/CanFrameParser.h)。
[CanFrameParser.cpp](../src/core/CanFrameParser.cpp) 只转换 `QByteArray` / `QHash` 与标准容器。
不要在包装层重新写一套端序算法，否则两层容易出现相反的结果。
验收点：下节不链接 Qt 仍能完成位运算；Qt 层只是把结果交给界面侧。

## 6. 动手：编译一个有断言的纯 C++ 实验

文件名：`$WORK/04/can_lab.cpp`。以下是**完整、可独立运行的实验入口**。
它复用仓库里的 Core 实现，因此还需编译命令中列出的那一个 `.cpp`，无需 Qt。

```cpp
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include "CanFrameParserCore.h"

int main()
{
    using Core = sc::CanFrameParserCore;
    using Bytes = std::vector<std::uint8_t>;
    Core parser;
    std::unordered_map<std::string, double> values;
    const Bytes frame{0x78, 0x00, 0xb8, 0x0b, 0x64, 0x01, 0x00, 0x00};
    assert(parser.decode(0x100, frame, &values));
    assert(values.at("speed") == 120 && values.at("engine_rpm") == 3000);
    assert(values.at("fuel_level") == 50);
    std::cout << "speed=120 rpm=3000 fuel=50\n";
    const auto previous = values;
    assert(!parser.decode(0x100, Bytes{0x78, 0x00}, &values));
    assert(values == previous);
    std::cout << "short frame: rejected, previous output preserved\n";

    struct Case { int start; int length; bool motorola; double value; Bytes bytes; };
    const std::vector<Case> cases{
        {7, 2, false, 3, {0x80, 0x01}},
        {7, 16, true, 4660, {0x12, 0x34}},
        {0, 2, true, 3, {0x01, 0x80}}
    };
    for (const auto &c : cases) {
        sc::CanSignalDef def;
        def.startBit = c.start;
        def.bitLength = c.length;
        def.bigEndian = c.motorola;
        Bytes encoded;
        assert(Core::encodeSignal(def, c.value, &encoded));
        assert(encoded == c.bytes);
        assert(Core::decodeSignal(def, encoded) == c.value);
    }
    std::cout << "Intel/Motorola: 3 boundary cases passed\n";

    sc::CanSignalDef signedDef;
    signedDef.bitLength = 16;
    signedDef.isSigned = true;
    signedDef.factor = 0.1;
    assert(std::abs(Core::decodeSignal(signedDef, Bytes{0x9c, 0xff}) + 10) < 1e-9);
    signedDef.bitLength = 63;
    signedDef.factor = 1;
    Bytes negative;
    assert(Core::encodeSignal(signedDef, -1, &negative));
    assert(negative == Bytes({0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f}));
    assert(Core::decodeSignal(signedDef, negative) == -1);
    std::cout << "signed: -10 degrees and signed-63 -1 passed\n";

    Bytes output{0xa5};
    signedDef.startBit = -1;
    assert(!Core::encodeSignal(signedDef, 1, &output));
    assert(output == Bytes{0xa5});
    assert(parser.encode(0x200, {{"temp_set", 24}, {"fan_speed", 3}, {"ac_on", 1}}, &output));
    assert(output == Bytes({0x18, 0x43, 0, 0, 0, 0, 0, 0}));
    std::cout << "invalid layout preserved output; HVAC encoded to 8 bytes\n";

    assert(parser.loadDbcText(
        "BO_ 256 Lab: 8 ECU\n"
        " SG_ speed : 0|16@1+ (1,0) [0|240] \"km/h\" ECU\n"));
    assert(parser.decode(0x100, Bytes{0x78, 0x00}, &values));
    assert(values.size() == 1 && values.at("speed") == 120);
    std::cout << "DBC override: only speed remains\n";
}
```

在定义过变量的终端编译；不要加 `-DNDEBUG`，否则断言及其中的调用会被删掉。

```bash
g++ -std=c++17 -Wall -Wextra -pedantic \
  -I"$PROJECT/src/core" "$WORK/04/can_lab.cpp" \
  "$PROJECT/src/core/CanFrameParserCore.cpp" -o "$WORK/04/can_lab"
"$WORK/04/can_lab"
```

### 逐步解释与预期结果

1. 第一组断言验证手算的 120、3000、50，输出第一行数值。
2. 第二组给短帧，确认失败没有清空上一帧结果。
3. 三个端序例子同时断言字节和解码值，避免“编码解码一起错却互相抵消”。
4. 有符号例子验证 `-10°` 和 63 bit 的 `-1`，不尝试危险的大浮点数。
5. 非法布局不改变 `A5`；空调命令是 `18 43 00 00 00 00 00 00`。
6. 最后覆盖 ID 256，只剩 speed，所以两字节也能解码；这不是原内置整帧的规则。

成功会输出六行说明并以状态码 0 结束；断言失败会指出源文件行号。
练习 A：只把 frame 的 `0x78` 改为 `0x64`，先预测失败，再把预期速度改为 100。
练习 B：恢复例子后，把 Motorola 的预期 `01 80` 改成 `03 00`，观察断言抓住端序错误。
练习 C：在实验副本中把 DBC 范围改为 `[0|10]`；仍可得到 120，证明范围没有自动执行。
每次只改一个变量；修改的是 `$WORK/04/can_lab.cpp`，不是项目协议文件。

## 7. 排错与验收

| 现象 | 按顺序检查 |
| --- | --- |
| 找不到头文件 | `PROJECT` 是否正确；编译命令是否保留 `-I"$PROJECT/src/core"` |
| 链接时报 undefined reference | 是否把 `CanFrameParserCore.cpp` 一起编译，而不只是包含头文件 |
| 120 被算成 30720 | 是否错误地把 Intel `78 00` 当成大端 `0x7800` |
| 单信号输出 0 | 先检查布局和输入长度；零可能合法，也可能是失败返回值 |
| 覆盖 DBC 后少字段 | 同 ID 会整表替换，不是给原表增补一个字段 |
| 短帧有时能解码 | 看当前所有信号所需的最大长度，不是只看 `BO_` 写了 8 |

内置 `0x100` 的最后字段位于 bit50，需要至少 7 字节；`0x200` 的 ACK 需要第 3 字节。
业务层还要检查必需字段名，下一篇会用 `hasAllSignals()` 防止部分 DBC 覆盖损坏状态。
对照测试可读 [纯 C++ 测试](../tests/tst_core_cxx.cpp) 与 [Qt 包装测试](../tests/tst_canframeparser.cpp)。
本篇无需运行项目全量测试，也不需要修改这些测试文件。

- [ ] 能把 `0x100` 转为十进制 256，把 `B8 0B` 算成 3000。
- [ ] 能在纸上画出 Intel `7|2` 与 Motorola `0|2` 的跨字节路径。
- [ ] 完整实验六组检查通过，并能说明短帧为何保留旧输出。
- [ ] 知道 DBC 子集、非事务覆盖、范围不校验和 double 精度的边界。
- [ ] 所有练习产物都在仓库外，没有修改源代码或原始 DBC。

[返回学习路线](README.md) · 上一步：[03 项目骨架](03-project-skeleton.md) · 继续：[05 车辆服务与模拟器](05-vehicle-service-and-simulator.md)
