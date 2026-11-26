# 12 · 交叉构建与部署：先验证产物，再接管设备

配套：[本章练习题](exercises/12-cross-build-and-deployment.md) · [全部题集](exercises/README.md)

[返回学习路线](README.md) · [上一章：11 测试与调试](11-testing-and-debugging.md) · [下一章：13 从零构建](13-build-from-zero.md)

本章主线为 Qt 5 / C++17，目标板为 aarch64 Linux。
**本章没有板端实测；所有板端命令、显示、摄像头和 CAN 结论均需硬件验证。**
以下仅为教学步骤，没有执行安装、部署、开机启动、权限调整或仓库配置修改。
遇到环境差异应停在当前阶段，不把后面的管理员命令当成一键修复。

## 1. 新手概念：三种环境必须分清

- 宿主机 host：运行编辑器、CMake、交叉编译器的电脑，通常是 x86_64。
- 目标机 target：最终运行 smart-cockpit 的板子，本章是 aarch64。
- sysroot：目标系统头文件和库的配套副本，不是随便找来的 ARM 文件夹。
- 工具链：编译器、链接器及查找规则；交叉编译器在宿主运行，输出目标机器码。
- ABI：二进制接口；架构相同但 glibc、libstdc++ 或 Qt 版本不匹配仍会失败。
- Qt 的 moc/uic/rcc 是构建时程序，要在宿主运行；链接进去的 Qt 库必须属于目标。
- staging：普通用户的临时交付目录；复制进去不等于已经安装或启用服务。
- systemd：管理进程生命周期；服务 running 不等于图像、声音或 CAN 业务正常。

不要把宿主 `/usr/lib/x86_64-linux-gnu` 的库放进目标链接目录来“消除报错”。
也不要在宿主上用 ldd 检查 ARM 程序；先用 file/readelf 静态检查，板端再查依赖。

## 2. 按文件顺序理解已有构建和部署

| 顺序 | 真实文件 | 阅读重点与当前限制 |
| --- | --- | --- |
| 1 | [CMakeLists.txt](../CMakeLists.txt) | C++17、Qt5 默认、GST、安装目标；最低 CMake 3.16 |
| 2 | [CMakePresets.json](../CMakePresets.json) | version 3，需要 CMake 3.21；Cross 输出 build/arm |
| 3 | [工具链](../cmake/toolchain-aarch64-linux-gnu.cmake) | sysroot、目标库查找；编译器前缀有错误 |
| 4 | [deploy.sh](../deployment/deploy.sh) | 默认 build-arm，远端直接覆盖，随后 enable/restart |
| 5 | [smart-cockpit.service](../deployment/smart-cockpit.service) | 原为 User=root、eglfs、硬编码 can0。**泰山派上 eglfs 不可用、root 拿不到用户 X 会话与 PulseAudio，本板默认改走 [deployment/taishanpi/](../deployment/taishanpi/)** |
| 6 | [kms-config.json](../deployment/kms-config.json) | 原为 HDMI-A-1、1280x800@60。本板实测是 `DSI-1` 800x1280，且该文件只有 eglfs 会读，本板用不到 |
| 7 | [main.cpp](../src/main.cpp) | CLI 才决定 can/camera/log-dir；没有 camera 就用测试源 |

已有脚本也须逐个辨认，不能见到“ci/deploy”就直接运行：

- [ci.sh](../scripts/ci.sh) 要求 root，执行 apt-get 安装并跑全部测试；本章不用。
- [format.sh](../scripts/format.sh) 默认原地改源码；只有 `--check` 是检查模式。
- [vcan-setup.sh](../scripts/vcan-setup.sh) 会加载模块、创建并启动接口，不是只读诊断。
- [demo-can.sh](../scripts/demo-can.sh) 向 vcan0 发有限几帧，不是持续心跳或真实协议验证。
- deploy 没有预建 `/etc/smart-cockpit`，首次复制 KMS 配置可能失败，且此前文件可能已覆盖。
- deploy 不复制 Qt/GStreamer/驱动依赖，不配置 CAN 波特率，也没有备份和回滚事务。
- 原服务未传 `--camera`，所以默认仍是 videotestsrc，不是真实摄像头。
- 原服务的 `SMART_COCKPIT_CAN` 环境变量没有被 main 读取；实际生效的是 `--can can0`。

**源码摘录：现有工具链错误，不能照抄后承诺 Cross 成功。**

```cmake
set(TOOLCHAIN_PREFIX aarch64-linux-gnu)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
```

它拼成 `aarch64-linux-gnugcc` / `aarch64-linux-gnug++`，前缀末尾缺 `-`。
本章不改仓库工具链，而在练习目录提供修正示例；也不使用仓库内构建 preset。

## 3. 路线选择：初学者优先考虑板端 native

统一宿主变量，下面命令只在你准备好依赖后自行执行：

```bash
export PROJECT=/home/air/qt/Smart_Cockpit_based_on_tspi
export HOST_BUILD="$HOME/build/smart-cockpit-host"
export WORK="$HOME/smart-cockpit-lab"
mkdir -p "$WORK/cmake" "$WORK/staging" "$HOME/build"
cmake --version
```

板子资源足够时，在板子上原生编译最容易避开 SDK/Qt 工具的宿主目标混用。
**板端 native 教学命令，待硬件验证**：源码需先放入普通用户拥有的目录。

```bash
BOARD_SOURCE="$HOME/src/Smart_Cockpit_based_on_tspi"
BOARD_BUILD="$HOME/build/smart-cockpit-native"
cmake -S "$BOARD_SOURCE" -B "$BOARD_BUILD" \
  -DCMAKE_BUILD_TYPE=Release -DSMART_COCKPIT_USE_QT6=OFF -DBUILD_TESTING=OFF
cmake --build "$BOARD_BUILD" --parallel 2
file "$BOARD_BUILD/smart-cockpit"
```

目标仍需 Qt 5 Widgets/DBus、GStreamer 开发依赖及编译工具；安装由管理员另行评审。
主机 Debug 产物留在 `$HOST_BUILD`，不能因为文件名相同就把它当成板端程序。

## 4. 交叉路线：仓库外的完整工具链示例

先由 SDK 提供者准备与目标 rootfs 匹配的 sysroot、交叉编译器和 Qt 5 SDK。
`SDK_QT_ROOT` 若使用，必须指向目标 aarch64 Qt 前缀，不是宿主 Qt。
建议 Qt 也放进 sysroot；若外置 Qt，其包配置、依赖和可重定位路径需 SDK 明确支持。
用编辑器保存下列**完整示例**为 `$WORK/cmake/aarch64-lab.cmake`：

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(TOOLCHAIN_PREFIX aarch64-linux-gnu-)
set(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}g++")
if(NOT DEFINED ENV{SDK_SYSROOT} OR "$ENV{SDK_SYSROOT}" STREQUAL "")
  message(FATAL_ERROR "Export SDK_SYSROOT to the target sysroot")
endif()
set(CMAKE_SYSROOT "$ENV{SDK_SYSROOT}")
if(NOT IS_DIRECTORY "${CMAKE_SYSROOT}/usr/include")
  message(FATAL_ERROR "Target sysroot must contain usr/include")
endif()
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
if(DEFINED ENV{SDK_QT_ROOT} AND NOT "$ENV{SDK_QT_ROOT}" STREQUAL "")
  if(NOT IS_DIRECTORY "$ENV{SDK_QT_ROOT}")
    message(FATAL_ERROR "SDK_QT_ROOT must be a target Qt prefix")
  endif()
  list(APPEND CMAKE_FIND_ROOT_PATH "$ENV{SDK_QT_ROOT}")
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{SDK_QT_ROOT}")
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(ENV{PKG_CONFIG_PATH} "")
set(ENV{PKG_CONFIG_LIBDIR}
  "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
set(CMAKE_EXE_LINKER_FLAGS_INIT
  "-Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu")
```

PROGRAM NEVER 表示构建工具从宿主找；LIBRARY/INCLUDE/PACKAGE ONLY 隔离目标依赖。
清空 `PKG_CONFIG_PATH`，并限定 `PKG_CONFIG_LIBDIR`，避免宿主 `.pc` 混入。
`PKG_CONFIG_SYSROOT_DIR` 重定位目标 `.pc` 中的系统路径，不会自动修好坏的 SDK。
如需 SDK 特有 pc 目录，只加入确认属于目标的目录，不追加宿主搜索路径。

教学配置与构建命令；路径是占位示例，必须替换成实际 SDK：

```bash
export SDK_SYSROOT=/opt/taisanpi/sysroot
# 仅在有单独目标 Qt 前缀时设置；否则保持未设置：
# export SDK_QT_ROOT=/opt/taisanpi/qt5
export CROSS_BUILD="$HOME/build/smart-cockpit-aarch64"
command -v aarch64-linux-gnu-g++
aarch64-linux-gnu-g++ -dumpmachine
cmake -S "$PROJECT" -B "$CROSS_BUILD" \
  -DCMAKE_TOOLCHAIN_FILE="$WORK/cmake/aarch64-lab.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DSMART_COCKPIT_USE_QT6=OFF -DBUILD_TESTING=OFF
cmake --build "$CROSS_BUILD" --parallel 2 --verbose
file "$CROSS_BUILD/smart-cockpit"
aarch64-linux-gnu-readelf -h -l -d "$CROSS_BUILD/smart-cockpit"
```

期望看到 AArch64，而非 x86-64；确认 interpreter、NEEDED 和 RPATH/RUNPATH。
`-rpath-link` 只帮助链接器找间接依赖，不负责板端运行时部署。
SDK/工具链改变后换新的外置 build，不把宿主缓存继续当交叉缓存使用。

## 5. Qt host tools 与目标库必须配套

Qt5 包可能把 `Qt5::moc` 指向目标 ELF，PROGRAM NEVER 并不能自动修复导入目标。
在 verbose 构建中检查实际执行的 moc/uic/rcc，用 `file` 确认它们能在宿主运行。
宿主工具应来自与目标 Qt 相匹配的 SDK，通常要求同版本；不可随手改用系统 Qt6 moc。
Qt5 SDK 的 host-tools 配置因发行方式而异，应按 SDK 文档配置包导出或工具路径。
不要假定 Qt6 的 `QT_HOST_PATH` 能通用于这里的 Qt5 工程。
若 SDK 未提供可用宿主工具，先补齐匹配 SDK 或改用板端 native，不伪造构建成功。
同样检查 GST 查找结果：pkg-config 程序可以来自宿主，但头文件和库必须来自目标。

## 6. 阶段 A：先只读检查板端设备与格式（待硬件验证）

以普通用户登录板子；工具不存在时记录缺项，由管理员另行准备，勿自动安装。

```bash
uname -m
id
ls -l /dev/dri /dev/video* /dev/snd /dev/input
ip -details link show can0
v4l2-ctl --list-devices
v4l2-ctl --device=/dev/video0 --list-formats-ext
modetest -c
qmake -query QT_VERSION
gst-inspect-1.0 videotestsrc
gst-inspect-1.0 v4l2src
gst-inspect-1.0 appsink
gst-inspect-1.0 playbin
gst-inspect-1.0 alsasink
fc-list :lang=zh
```

`/dev/video0` 只是候选，须用枚举结果找到真实采集节点；不是每个 video 节点都输出图像。
确认摄像头支持当前默认 1280×720 / 30 fps，及 `io-mode=4` 与 NV12 转换链路。
[CameraService.h](../src/camera/CameraService.h) 默认参数不能通过现有 CLI 改分辨率。
`modetest -c` 只查询连接器，不使用设模式的参数；访问受限时记录并交管理员处理。
逐项记下 DRM card、连接器、模式、屏幕方向，不能把 1280×800 横屏当成 800×1280 竖屏。
专用用户及 video/render/input/audio 等设备组应由管理员按实际节点权限配置。
不要 chmod 777、放宽所有设备权限或用 root 启动应用来绕过问题。

## 7. 阶段 B：普通用户 staging，不安装（待硬件验证）

宿主教学命令：目标使用普通登录用户，先替换地址；不调用原 deploy.sh。

```bash
TARGET=developer@board-address
STAGE="$WORK/staging"
install -m 0755 "$CROSS_BUILD/smart-cockpit" "$STAGE/smart-cockpit"
cp "$PROJECT/deployment/kms-config.json" "$STAGE/kms-config.json"
sha256sum "$STAGE/smart-cockpit"
ssh "$TARGET" 'mkdir -p "$HOME/smart-cockpit-staging"'
rsync -av "$STAGE/" "$TARGET:smart-cockpit-staging/"
```

native 路线则在板端将 `$BOARD_BUILD/smart-cockpit` 复制到同名 staging，不经过宿主产物。
在 staging 的 KMS 副本中按阶段 A 结果修改 device/name/mode，不改仓库文件。
只有确认硬件恰好匹配时才保留原来的输出名与模式。**泰山派 RK3566 实测是 `card0-DSI-1: 800x1280`，HDMI 口未接屏；照抄 HDMI-A-1 / 1280x800@60 会黑屏。**
这里仅放二进制和配置，不宣称已带齐依赖；Qt 插件、GST 插件、字体、GPU 驱动仍需匹配。

## 8. 阶段 C：检查匹配库并前台验证（待硬件验证）

板端普通用户命令；ldd 只用于你自己可信构建的程序，不检查来源未知的 ELF：

```bash
STAGE="$HOME/smart-cockpit-staging"
file "$STAGE/smart-cockpit"
sha256sum "$STAGE/smart-cockpit"
ldd "$STAGE/smart-cockpit"
python3 -m json.tool "$STAGE/kms-config.json"
mkdir -p "$STAGE/logs"
QT_QPA_PLATFORM=eglfs QT_QPA_EGLFS_KMS_CONFIG="$STAGE/kms-config.json" \
  QT_DEBUG_PLUGINS=1 GST_DEBUG=3 \
  "$STAGE/smart-cockpit" --sim --camera "" --log-dir "$STAGE/logs"
```

先确认无 `not found`，再比较校验和；依赖缺失应准备匹配的目标运行库而非复制宿主库。
eglfs 会接管显示：须在获准的空闲显示会话里操作，不能直接与现有桌面/座舱争用 DRM。
若 DRM master 被占用，由管理员协调会话，不在教程中自动关闭显示服务。
先用 `--sim` 和测试源隔离 CAN/摄像头，再将 `--camera ""` 替换成已核实的采集节点。
检查中文、方向、触摸、倒车画面与日志；这些必须实际看见，不由 systemctl 状态替代。
真实 CAN 只在**隔离台架**接入前核对 DBC、波特率、终端电阻和接口配置，由获授权人员完成。
不能向在用车辆发送示例帧；应用空调操作也可能发帧，台架批准前保留 `--sim`。

## 9. 阶段 D：准备最小服务并备份（待硬件验证）

用编辑器把下面**完整教学示例**保存到板端 `$STAGE/smart-cockpit.service`。
它是“模拟演示服务”，没有真实 CAN/摄像头承诺；最终参数须与已通过的前台验证一致。
`smartcockpit` 用户和同名组必须已由管理员建立并获必要的设备访问权。

```ini
[Unit]
Description=Smart Cockpit lab (simulation)
After=local-fs.target

[Service]
Type=simple
User=smartcockpit
Group=smartcockpit
WorkingDirectory=/var/lib/smart-cockpit
Environment=QT_QPA_PLATFORM=eglfs
Environment=QT_QPA_EGLFS_KMS_CONFIG=/etc/smart-cockpit/kms-config.json
ExecStart=/usr/local/bin/smart-cockpit --sim --camera "" --log-dir /var/log/smart-cockpit
Restart=on-failure
RestartSec=3
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

先检查 `systemctl cat smart-cockpit`、`is-active`、`is-enabled`，记录旧状态与所有 drop-in。
执行 `systemd-analyze verify "$STAGE/smart-cockpit.service"`；安装前可出现目标路径不存在提示。
有未知 drop-in 或旧实例占用显示时先暂停，不能通过覆盖主文件假定旧配置已经消失。
**以下开始涉及系统修改，仅在管理员明确授权的维护窗口执行；不是让应用以 root 运行。**
先备份旧版本，不先停服务、更不先 enable：

```bash
BACKUP="/var/backups/smart-cockpit-$(date +%Y%m%d-%H%M%S)"
sudo install -d -m 0700 "$BACKUP"
for path in /usr/local/bin/smart-cockpit /etc/smart-cockpit /etc/systemd/system/smart-cockpit.service /etc/systemd/system/smart-cockpit.service.d; do
  if sudo test -e "$path"; then sudo cp -a --parents "$path" "$BACKUP/"; fi
done
```

记录哪些路径原先不存在，首次安装回退需要撤销新建服务，而非寻找不存在的备份。

## 10. 阶段 E：授权安装、试启动，最后才允许自启（待硬件验证）

以下命令**修改系统文件和服务状态**；先完成阶段 D 并确认备用登录/串口通道。

```bash
id smartcockpit
getent group smartcockpit
sudo systemctl stop smart-cockpit   # 仅当旧服务存在；全新安装跳过此行
sudo install -d -m 0755 /etc/smart-cockpit
sudo install -d -m 0750 -o smartcockpit -g smartcockpit /var/lib/smart-cockpit /var/log/smart-cockpit
sudo install -m 0755 "$STAGE/smart-cockpit" /usr/local/bin/smart-cockpit
sudo install -m 0644 "$STAGE/kms-config.json" /etc/smart-cockpit/kms-config.json
sudo install -m 0644 "$STAGE/smart-cockpit.service" /etc/systemd/system/smart-cockpit.service
sudo systemd-analyze verify /etc/systemd/system/smart-cockpit.service
sudo systemctl daemon-reload
sudo systemctl start smart-cockpit
systemctl status smart-cockpit --no-pager
systemctl show smart-cockpit -p User -p ExecStart -p NRestarts
journalctl -u smart-cockpit -b -n 100 --no-pager
```

任一步失败就停下，不连续粘贴后续步骤；旧日志文件所有权也要管理员逐项审核。
注意默认 SIGTERM 未必进入 Qt 正常退出清理，服务停止不证明异步日志完整排空。
Qt 消息会进入文件日志，不一定出现在 journal；按第 10 章同时检查实际文件。
若失败：经授权 stop，按备份原路径恢复二进制/配置/服务与 drop-in，daemon-reload 后恢复旧运行状态。
首次安装则经授权撤销新服务并 reload；保留日志供调查，不删除设备数据或整个备份目录。
**只有前台与专用用户服务验收均通过、回退方案可用，才另行获准启用自启：**

```bash
sudo systemctl enable smart-cockpit
systemctl is-enabled smart-cockpit
```

受控重启验证需再获维护授权；不得把“enable 成功”写成“重启后已验证正常”。

## 11. 排错、练习与验收

| 现象 | 排查顺序 |
| --- | --- |
| 找不到交叉编译器 | 检查前缀末尾 `-`（**已修**：原 `aarch64-linux-gnu` 缺结尾连字符，会拼出 `aarch64-linux-gnugcc`）、PATH、完整交叉 build 缓存。注意：交叉路线至今未在实机跑通，泰山派上推荐板端原生编译 |
| moc 无法执行 | 是否误用 ARM moc；宿主工具与目标 Qt 是否匹配 |
| ELF 存在却无法执行 | file/readelf、解释器路径、glibc/GLIBCXX 与目标 ABI |
| Qt 平台插件加载失败 | 插件自身依赖、目标 Qt 版本、GPU/EGL 驱动，不只查 libQt5Widgets |
| 黑屏/方向不对 | DRM card、connector、mode、占用者、屏幕方向，不能照搬示例值 |
| CAN LOST / 摄像头失败 | 先区分模拟与真实参数，再查接口/协议/格式，禁止盲发帧试错 |
| 服务反复重启 | journal、文件日志、NRestarts、专用用户权限与工作目录 |

练习：先在纸面列出 Cross preset 与 deploy 默认路径差异，再写自己的 staging 清单。
预期能指出 `build/arm` 与 `build-arm` 不一致，并找到缺失配置目录和立即启用的副作用。
练习：对比宿主与目标 ELF 的 file/readelf 输出，解释为什么成功编译不等于能上板运行。

- [ ] 没改仓库工具链/部署脚本；练习产物、构建和配置副本均在仓库外。
- [ ] SDK、Qt host tools、目标库/插件来源明确，没有混入宿主库。
- [ ] 按只读检查→staging→前台→备份→授权安装→服务验证→自启推进。
- [ ] 专用用户按实际设备授权，不依赖 root 应用或 chmod 777。
- [ ] 显示、摄像头、媒体、真实 CAN 和受控重启分别记录为已验或待验，不虚构板端成功。

[返回学习路线](README.md) · [上一章：11 测试与调试](11-testing-and-debugging.md) · [下一章：13 从零构建](13-build-from-zero.md)
