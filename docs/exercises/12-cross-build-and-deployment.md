# 第 12 章配套练习：交叉构建与安全交付审查

[对应教程](../12-cross-build-and-deployment.md) · [题集导航](README.md)

## 前置、用时与学习目录

- 前置：完整阅读对应教程；了解 CMake、Linux ELF、目录权限与进程/服务的区别。
- 建议时间：基础 30 分钟，静态审查 45 分钟，方案实践 100 分钟，排障与验收 45 分钟。
- `$WORK` 为仓库外普通用户学习根目录，例如 `$HOME/smart-cockpit-lab`；本章使用 `$WORK/exercises/12`。
- 用编辑器或文件管理器准备 `source-copy/`、`toolchain-draft/`、`staging-plan/`、`evidence/`；原项目只读。
- **本章全部题目可仅凭静态审查和纸面方案完成并取得同等分数；不要求 SDK 或板子。**
- 只允许在学习副本编写草案、清单和模拟输出；无 SDK/板子不扣知识题，不能把预期填成实测通过。
- 本题集不要求执行任何部署、联网传输、sudo、服务控制、系统修改、设备权限调整或真实 CAN 操作。
- 原 deploy/ci/vcan 脚本只读不运行；即使教程有管理员命令，本练习也只审查其前提与后果。
- 共 8 题，分值依次为 5、5、5、15、20、20、15、15，总分 100 分。

入口：[预设](../../CMakePresets.json)、[原工具链](../../cmake/toolchain-aarch64-linux-gnu.cmake)、[部署脚本](../../deployment/deploy.sh)。
配置入口：[服务](../../deployment/smart-cockpit.service)、[KMS](../../deployment/kms-config.json)、[CLI 主入口](../../src/main.cpp)。

### 12-01 宿主、目标与 ABI 不是一个维度（5分）

**输入/约束：** 宿主 x86_64 Linux，目标 aarch64 Linux；同名应用分别在宿主 Debug 和交叉 Release 目录出现。
目标 Qt、glibc、libstdc++ 版本尚未确认，不能假设“都是 ARM 就兼容”。

**任务：** 标明 CMake、交叉编译器、生成的应用分别在哪运行；解释架构正确但 ABI 不匹配的一种失败。
说明 SDK/sysroot/宿主构建产物各自是什么，不能拿哪个替代哪个。

**提交物：** 环境归属表及一个带库/解释器版本的兼容性反例。

**验收/评分点：** 构建程序在 host、应用在 target 2 分；架构与 ABI 区别 2 分；宿主产物不混充目标产物 1 分。

### 12-02 sysroot 与 pkg-config 的隔离（5分）

**输入/约束：** 目标 sysroot 的占位路径为 `/opt/sdk/sysroot`，GST 的 `.pc` 文件属于目标；宿主也安装了 GST。
候选配置仅设置 `CMAKE_SYSROOT`，却继承了指向宿主目录的 `PKG_CONFIG_PATH`。

**任务：** 指出污染通路，解释 `PKG_CONFIG_PATH`、`PKG_CONFIG_LIBDIR`、`PKG_CONFIG_SYSROOT_DIR` 的职责。
给出程序/库/头文件/包各自的 CMake 查找策略，说明 sysroot 不会修复错误架构或损坏的 SDK。

**提交物：** 一份变量审查表，写清每个路径指向 host 还是 target；无需实际设置环境变量。

**验收/评分点：** 清除宿主 `.pc` 污染 2 分；目标 `.pc` 搜索及路径重定位 2 分；PROGRAM 与目标依赖的查找策略 1 分。

### 12-03 Qt 主机工具与目标库配对（5分）

**输入/约束：** Qt5 SDK 的导入目标 `Qt5::moc` 指向 AArch64 ELF，构建却运行在 x86_64；工具链已设 PROGRAM NEVER。
有人建议换系统 Qt6 moc，或仅设置 Qt6 的 `QT_HOST_PATH` 就宣称解决。

**任务：** 判断两项建议，列出检查 moc/uic/rcc 的证据，并说明目标 Qt 库应来自哪里。
不能以“找到了同名 moc”代替版本与运行架构检查。

**提交物：** 三步定位顺序、兼容性判断及 SDK 不齐时的停止条件。

**验收/评分点：** 工具能在 host 运行 2 分；导入目标不受简单查找策略自动修正 1 分；匹配 Qt5 SDK/目标库且不套用 Qt6 假设 2 分。

### 12-04 静态审查原交付链的断点（15分）

**输入/约束：** 只读原工具链、Cross preset、deploy、service、KMS 和 main；以下为源码事实摘要，仅供阅读，不要执行。

| 项目 | 待审查片段或默认值 |
| --- | --- |
| 编译器 | prefix 为 `aarch64-linux-gnu`，后接 `gcc` / `g++`。 |
| 构建与取件 | Cross 输出 `build/arm`，deploy 默认取 `build-arm/smart-cockpit`。 |
| 复制顺序 | 先覆盖二进制、service，再复制 KMS；只准备日志目录。 |
| 复制成功之后 | 立即执行 daemon-reload、enable、restart，无前台验证门槛。 |
| 服务与硬件 | User=root，固定 KMS，ExecStart 只有 `--can can0`，未传 camera。 |

**任务：** 写实际拼出的两个编译器名；解释目录不一致可能选不到产物或误用旧产物。
假设首次部署缺 `/etc/smart-cockpit`，按 `set -e` 推演哪些文件可能已改变、哪些后续步骤不会执行。
若复制均成功，说明立即 restart 的副作用，以及 root、固定 KMS、无 camera 参数的实际含义。
补查 `SMART_COCKPIT_CAN` 环境变量是否被 main 读取，不把环境变量文本当成生效参数。

**提交物：** “缺陷→证据文件→后果→应增加的检查门槛”表，不提交仓库修复。

**验收/评分点：** 前缀缺 `-` 3 分；两种构建路径 2 分；缺目录及部分覆盖 4 分；root/KMS/camera 3 分；立即启用与重启、CLI 生效来源 3 分。

### 12-05 具体实践：离线编写交叉构建审查包（20分）

**输入/约束：** 在学习目录写工具链草案与证据模板，不执行交叉构建；下表为虚构审查材料，仅供阅读，不要执行。
目标是 AArch64、Qt 5；SDK 中应同时有目标开发库与匹配的宿主工具。

| 样本 | 模拟静态结果 |
| --- | --- |
| H：同名宿主应用 | ELF x86-64。 |
| T：候选目标应用 | ELF AArch64，解释器 `/lib/ld-linux-aarch64.so.1`，NEEDED 含 Qt5Widgets、libstdc++。 |
| M：SDK 的 moc | ELF AArch64，但准备在 x86_64 上运行。 |
| P：GST 查找结果 | 含 `-L/usr/lib/x86_64-linux-gnu`。 |

**任务：** 草案需包含正确 compiler prefix、非空 sysroot 检查、目标 Qt 来源、四种查找模式、pkg-config 隔离。
分别判定 H/T/M/P 能否放行；T 还缺哪些 interpreter、ABI、插件和 RPATH/RUNPATH 证据？
设计宿主静态检查清单：使用 file/readelf 检查目标产物，用 verbose 构建记录确认实际 moc/uic/rcc；本题只写检查方案。
规划三个互不复用的学习构建目录：host、cross、sanitizer；SDK 变化时另建缓存，不使用仓库 preset 的构建目录。

**提交物：** 学习副本中的工具链关键片段、四样本审查结果、证据字段模板和目录图。

**验收/评分点：** 编译器/sysroot 5 分；目标库与 pkg-config 隔离 5 分；Qt 主机工具 4 分；ELF/ABI/运行依赖证据 4 分；外置缓存及未运行声明 2 分。

### 12-06 具体实践：staging、备份与回滚清单（20分）

**输入/约束：** 仅在 `$WORK/exercises/12/staging-plan/` 写纸面交付包，不连接板子、不复制到系统路径。
假设旧版 v1 active=true、enabled=false，存在 service drop-in；候选 v2 尚未前台验证。
候选包只有二进制和 KMS JSON，不能假设 Qt/GST 插件、字体、驱动已打包齐全。

**任务：** 制作 manifest：相对路径、版本、来源、目标架构、大小/校验和待填字段、候选安装路径和验证负责人。
列出 `/usr/local/bin/smart-cockpit`、`/etc/smart-cockpit`、主 service 与 `.service.d` 的备份映射，记录原先不存在的路径。
设计专用非 root 用户、工作/日志目录、实际设备授权及 KMS 副本审查，不修改系统或放宽设备权限。
给出升级失败和首次安装失败两个回滚分支，明确恢复二进制/配置/服务/drop-in，以及旧 active/enabled 状态。
列出前台未通过时停止推进的条件，任何覆盖前都必须已有可用备份；保留调查日志和备用登录通道。

**提交物：** manifest、备份表、回滚流程、两项失败演练的纸面状态变化；哈希未知写“待计算”而非伪造。

**验收/评分点：** staging 清单及依赖缺口 5 分；备份完整性与原状态 5 分；两类回滚分支 6 分；最小权限、前台门槛和证据标识 4 分。

### 12-07 排障：架构正确、服务活着仍可能失败（15分）

**输入/约束：** 以下均为虚构故障样本，仅供阅读，不要执行；只能给调查计划，不登录设备或控制显示会话。

| 样本 | 模拟观察 |
| --- | --- |
| A | 应用 ELF 是 AArch64；目标提示找不到解释器，补齐后又报 GLIBCXX 版本不满足。 |
| B | cross 构建运行 moc 时出现 Exec format error；团队准备把宿主 Qt 库加入目标链接目录。 |
| C | service 显示 active，但黑屏；配置仍是 card0/HDMI-A-1/1280x800@60，未传 camera；journal 没有 qInfo。 |

**任务：** 每例按“先看什么→如何区分环境与程序问题→禁止的捷径”给三步诊断。
C 要同时考虑 DRM 占用、方向、专用用户权限、测试源与真实摄像头，以及 Qt 消息所在文件。
所有板端观察均写成待授权取证项，不把计划写成已经看到的现象。

**提交物：** 三张排障卡，给出需要的静态输出/配置字段，而非一串管理员修复命令。

**验收/评分点：** 每例 5 分：有序证据 2 分、机制解释 2 分、不越权且不夸大 1 分。

### 12-08 综合验收：先前台验证，再讨论自启（15分）

**输入/约束：** 假设有人提出“产物能编译，直接沿用原脚本启用服务”；只在纸面召开交付审查，不执行部署。
你手头没有 SDK/板子也可完成本题：全部实机结果留待验，给出通过条件与失败分支。

**任务：** 画出只读核对→staging→获准空闲显示会话的前台验证→备份→授权安装→专用用户服务验证→另行批准自启的门槛图。
前台先用模拟 CAN/测试源隔离依赖，再规划真实相机；真实 CAN 仅在隔离台架获授权后核对协议、波特率与终端电阻。
分别列中文/方向/触摸、倒车图像、媒体音频、日志、真实 CAN、重启后的验证项；不以 active/enable 成功替代业务结果。
写一个“拒绝放行”示例：没有前台证据或回滚不可用时，即使服务 active 也不能越过门槛。

**提交物：** 门槛图、验收矩阵、待验列表和最终“可继续审查/不可部署”的纸面结论。

**验收/评分点：** 阶段顺序与失败即停 5 分；硬件/业务分别验收 4 分；备份回滚、专用用户与自启授权 4 分；不冒充实机及证据索引 2 分。

## 提交清单

- [ ] 八题答案、工具链草案、静态审查表均保存在学习目录，原仓库不变。
- [ ] manifest、备份映射、旧运行/启用状态、drop-in 和首次安装回滚分支齐全。
- [ ] 每项标“源码事实 / 虚构样本 / 纸面方案 / 待硬件验证”，没有伪造校验和或实机通过。
- [ ] 无 sudo、联网、部署、服务控制或系统修改；无 SDK/板子不影响知识题和纸面方案评分。

<details><summary>参考答案与评分提示（先独立完成）</summary>

**12-01**
- CMake、交叉编译器在 host 运行；编译器输出 target 机器码，应用在 target 运行，host Debug 产物不能混用。
- sysroot 是目标头文件和库的配套副本，SDK 还提供工具链/构建工具等；普通 ARM 文件夹不能代替匹配 sysroot。
- 即使 AArch64 正确，目标缺少 ELF 指定的解释器，或 libstdc++ 缺所需 GLIBCXX 版本，仍无法运行。

**12-02**
- 清空继承的 PKG_CONFIG_PATH，限定 PKG_CONFIG_LIBDIR 为目标 `.pc` 目录；不要追加宿主库目录。
- PKG_CONFIG_SYSROOT_DIR 重定位目标 `.pc` 中的系统路径，但不会把宿主库转换为目标库或修复坏 SDK。
- PROGRAM NEVER 找宿主构建程序；LIBRARY/INCLUDE/PACKAGE ONLY 查目标根。pkg-config 程序本身可在 host 运行。
- sysroot 常含目标 `usr/lib/aarch64-linux-gnu/pkgconfig`、`usr/lib/pkgconfig`、`usr/share/pkgconfig`，以 SDK 实际布局为准。

**12-03**
- 从 verbose 命令确认实际执行路径，再检查 moc/uic/rcc 的架构及版本，并与目标 Qt5 SDK 匹配。
- Qt5 包可把导入目标直接指向 ARM 工具；PROGRAM NEVER 不自动纠正该导入路径。
- 不随意用 Qt6 moc 或假定 QT_HOST_PATH 通用于 Qt5；缺匹配 host tools 就暂停，补齐 SDK 或另行评估板端 native。

**12-04**
- 拼出 `aarch64-linux-gnugcc` / `aarch64-linux-gnug++`；前缀应为 `aarch64-linux-gnu-`，但这里只改学习草案。
- `build/arm` 与 `build-arm` 不同；默认取件可失败，也可能误取另一目录旧文件。文件存在检查不验证架构/版本。
- 脚本没有先创建 `/etc/smart-cockpit`；KMS 复制失败前二进制/service 可能已覆盖，set -e 不负责回滚。
- 此失败会阻止后续准备日志目录和 enable/restart；若前面均成功，则立即 reload/enable/restart，仍无验证门槛。
- 服务 User=root；KMS 固定 `/dev/dri/card0`、HDMI-A-1、1280x800@60，不能据此认定设备或屏幕方向匹配。
- 未传 `--camera` 默认使用 videotestsrc；main 不读取 SMART_COCKPIT_CAN，实际接口由 `--can can0` 决定。

**12-05**
- 草案关键是正确 prefix、非空且含目标头文件的 sysroot、目标 Qt 来源、ONLY/NEVER 分工和 `.pc` 隔离，不必提交整套构建脚本。
- H 架构错误拒绝；M 不能在本题 host 直接运行，须匹配宿主工具；P 混入宿主库，应拒绝当前查找结果。
- T 仅过架构初筛，仍需匹配解释器、glibc/GLIBCXX、Qt/GST 库及插件、GPU 驱动、RPATH/RUNPATH。
- 宿主用 file/readelf 做静态检查，不在宿主对 ARM ELF 用 ldd；目标上的 ldd 也仅规划用于可信自建程序。
- `-rpath-link` 帮链接器找间接依赖，不负责板端运行部署；换 SDK 必须重新审查缓存与路径来源。

**12-06**
- staging 不等于安装；manifest 区分应用、KMS 副本、服务草案和外部运行依赖，哈希/硬件值未知留待填。
- 备份覆盖旧二进制、整个配置目录、主 unit、全部 drop-in，并保存权限/所有权、active/enabled 及原不存在路径。
- 升级回滚经另行授权恢复各原路径、重新加载服务配置、恢复旧运行状态；题设应恢复 active=true、enabled=false。
- 首装回滚撤销本次新增服务/启用关系并重新加载配置；只处理明确属于本次安装的项，不误删数据或调查日志。
- 前台验证未通过不安装；专用用户和设备授权由管理员审核，不用 root 应用或 chmod 777 绕过；保留备用登录通道。

**12-07**
- A：读 ELF interpreter/NEEDED、目标库版本和 ABI；文件存在与架构正确都不等于运行环境匹配，不复制宿主库补洞。
- B：核对实际 moc 的运行架构与 Qt 版本，修正学习方案的 host tools 来源；宿主 Qt 库加入目标链接是另一种污染。
- C：核对 DRM card/connector/mode/方向、占用者与权限，先计划获准空闲会话的前台观察，不擅停桌面或抢 DRM。
- 未传 camera 只是测试源；真实采集节点、格式及插件待核实。Qt handler 把消息送往文件日志，journal 为空不能判无日志。
- service active 只代表进程状态；不能证明显示、音频或 CAN 可用，更不能靠 root 重启掩盖环境错误。

**12-08**
- 合格顺序含只读检查、staging、前台、备份、授权安装、专用用户服务验证，全部通过且回退可用后才另行批准自启。
- 实际 KMS、摄像头默认 1280×720/30 fps、NV12/io-mode 等适配需硬件证据；现有 CLI 不能改相机分辨率。
- 先模拟隔离依赖，真实 CAN 仅限获授权隔离台架；不能向在用车辆注入示例帧，空调操作也可能发 CAN。
- 服务停止/SIGTERM 不保证进入 Qt 正常退出并排空日志；enable 成功也不证明受控重启后正常，重启验证另需授权。
- 没有板端证据时结论应为“纸面审查完成，部署/实机验收待授权验证”，不能填写所有项目实机通过。

</details>
