# 15 · 泰山派 RK3566 实测记录

本文只收**在实机上验证过的结论**，以及为得到它们付出的代价（错误假设、失效的测法）。
凡是没在板子上跑通的，一律标注"未验证"，不写成事实。

配套：[09 摄像头](09-camera-and-reverse.md) · [12 交叉构建与部署](12-cross-build-and-deployment.md) ·
[14 已知边界](14-known-issues-and-exercises.md) · [板端自启方案](../deployment/taishanpi/README.md)

---

## 1. 环境矩阵（全部实测）

| 项 | 值 | 怎么确认的 |
| --- | --- | --- |
| 板子 | 立创·泰山派 RK3566 原版 V10，**2GB RAM + 16GB eMMC** | `free -m`、`df -h /` |
| 系统 | Ubuntu 20.04，内核 4.19.232 | `lsb_release -a`、`uname -r` |
| Qt | **5.12.8**（不是 5.15） | `dpkg -l \| grep libqt5core5a` |
| 显示输出 | `card0-DSI-1: 800x1280`，HDMI 未接屏 | `ls /sys/class/drm/*/modes` |
| 触摸 | GT9271，dts 里 `tp-size` 必须为 `101` | 改前触摸上下颠倒 |
| 网络 | RNDIS `usb0` 静态 `192.168.7.2/24`，**无互联网** | `apt install` 不可用 |
| 调试口 | 板上 **CN2** 4Pin GH1.25 座，1500000 8N1，**3.3V** | 抓到完整启动日志 + 免密 root shell |
| 音频 | card0 = `rk809-codec`（I2S），card1 = HDMI | `aplay -l` |
| 包保护 | 1487 个包 hold | `apt-mark showhold \| wc -l` |

---

## 2. 显示栈：eglfs 和 linuxfb 在这块板上都不可用

这是本文最重要的一条，因为它推翻了仓库原本的既定方案。

| 尝试 | 结果 |
| --- | --- |
| `QT_QPA_PLATFORM=eglfs` | **整屏全黑**。用一个只画纯色的最小 `QOpenGLWidget` 探针也是黑的，所以不是应用的问题 |
| `QT_QPA_PLATFORM=linuxfb` | `cannot mmap framebuffer` |
| `QT_QPA_PLATFORM=xcb` + lightdm 自动登录 | **可用**，唯一跑通的路线 |

`xcb` 路线还需要 `LIBGL_ALWAYS_SOFTWARE=1`：板子上虽然装了
`libmali-bifrost-g52-g2p0-x11`，但 `rockchip_dri.so` 实际是改名后的 swrast，
驱动名校验过不了。

> **不要照抄 `deployment/smart-cockpit.service` 的 eglfs 配置。** 本板的可用部署在
> [`deployment/taishanpi/`](../deployment/taishanpi/README.md)，那条路是桌面会话自启，
> 不是 root systemd 单元——root 单元既拿不到 `:0` 的 X 授权，也连不上用户的
> PulseAudio socket。

---

## 3. 摄像头为什么必须软件校色：rkaiq 3A 在这块板上起不来

官方 FAQ 说画面"发绿"，根因不是 IQ 文件没配好，而是 **3A 守护进程根本没跑起来**。
排查过程逐条排除了所有表面解释：

1. `/etc/iqfiles` 里确实缺 `ov5695_TongJu_CHT842-MD.json`（SDK 有：
   `external/camera_engine_rkaiq/iqfiles/isp21/`）。补上后仍崩。
2. `CamHwIsp20.cpp::get_sensor_caps` 把"枚举 mbus code / frame size"那段包在
   `#if 0` 里，于是 `fie.code` 恒为 0；而驱动 `ov5695_enum_frame_interval` 要求
   `code == MEDIA_BUS_FMT_SBGGR10_1X10` 才返回 0 → `Enum sensor frame interval failed`。
   放宽驱动后这个报错消失了，但仍然崩。
3. 换成 SDK 里那份 rk356x 的 `external/isp2-ipc/libs/librkaiq.so`，报错变成
   `Get sensor module info failed`——它的 `RKMODULE_GET_MODULE_INFO` ioctl 编码
   和本 kernel 的 `rk-camera-module.h` 对不上。
4. **决定性一步**：把镜像自带的 2022 版 `imx415_*.json` 改名顶替 ov5695 的 IQ，
   **同样段错误**。

结论：崩因与 IQ 内容无关。rootfs 的 `librkaiq.so` 属于
`camera-engine-rkaiq 3.0x8.5`，包描述明写 "match Rockchip rkisp **v30(rk3588)**"，
在 RK3566 的 **ISP 2.1** 上初始化即段错误。要修好得从 SDK 源码重编整个 rkaiq。

于是 `rkaiq_3A.service` 已 `systemctl disable`，校色改在应用侧做。

### 应用侧做了什么

在既有的 NV12→RGBA 逐像素循环里用查表补上，不新增依赖：

- **色彩空间**：mainpath 实测是 **full-range**（luma 真的铺到 0..255），原代码用
  BT.601 **limited** 公式，暗部被压死。改成 Rec.709 full-range。
- **灰世界白平衡**：统计 1/16 采样的 R/G/B 均值，把三通道拉平，EMA 平滑、限幅 0.5~2.5。
- **gamma 2.2**：3A 不跑时 ISP 不写 gamma LUT，输出是线性的，不提亮会又暗又闷。
- **亮度闭环**：目标线性均值 40，增益限幅 1~6。
- 合成源（`--camera ""`）不参与校正，免得主机调试时被误导。

### 还有一条容易漏的字节序坑

`QImage::Format_RGBA8888` 在内存里是 **R,G,B,A**。小端下写 quint32 时：

```cpp
*dst = 0xff000000U | (quint32(b) << 16) | (quint32(g) << 8) | quint32(r);  // 对
*dst = 0xff000000U | (quint32(r) << 16) | (quint32(g) << 8) | quint32(b);  // 错，红蓝互换
```

原仓库是后者。这条在 [14](14-known-issues-and-exercises.md) 里原本列为"尚需修正"，现已改对，
并用真实场景照片确认颜色正常。

---

## 4. 摄像头电源使能：`DVP_PWREN` = `GPIO0_B0`

原理图（`第02章/.../立创·泰山派开发板原理图.pdf`）第 21 页：

```
DVP_PWREN_H ──[R90 10kΩ]──┬── U7 pin3 (CE)     U7 = TP5108E23E-28 → VCC2V8
                          └──[C238 100nF]── GND
```

`U8`（→VCC1V8）、`U9`（→VCC1V2）拓扑完全相同。**三路 LDO 的使能端都由这同一根网络控制，
且各挂 100nF 对地**——上电瞬间电容放电完毕，CE=0V，即**摄像头默认断电**。
唯一能给电容充电的是 `GPIO0_B0`（原理图 P5 脚位 `1T20`）。

而软件侧：

- 设备树写了 `power-gpios = <&gpio0 RK_PB0 GPIO_ACTIVE_HIGH>`，但 **`ov5695.c` 里没有
  `power_gpio` 字段，从不读这个属性**；
- `avdd/dovdd/dvdd` 三个 supply 在板上全是 **dummy regulator**（dmesg 明写
  `using dummy regulator`）；
- U-Boot 里也搜不到 `DVP_PWREN`；
- 实测 PMU GRF `0xFDC20024` 里 GPIO0_B0 的 2bit 编码是 **0（无上拉）**，而同组标注
  `_d` 的 B7 读数是 2（下拉正常）——即这颗脚复位后是**输入 + 无驱动**。

> 顺带纠正一个我自己犯过的错：手册 P18/19 说 **PMUIO0/PMUIO1 是固定电平电源域、不可配置**，
> 所以往 `&pmu_io_domains` 里补 `pmuio0/pmuio1-supply` 是**错的方向**，不要做。
> 别的 rk3566 板子 dts 里写 `pmuio1-supply` 是另一回事。

### 修法

```dts
/ {
    cam_pwr: cam-pwr-regulator {
        compatible = "regulator-fixed";
        regulator-name = "camPwr";
        regulator-min-microvolt = <3300000>;
        regulator-max-microvolt = <3300000>;
        gpio = <&gpio0 RK_PB0 GPIO_ACTIVE_HIGH>;
        enable-active-high;
    };
};

&ov5695 {
    avdd-supply = <&cam_pwr>;
};
```

这样驱动里本来就在调、但一直是空操作的 `regulator_bulk_enable()` 才真正给模组上电。

**注意 dtc 1.4.6 不接受在 dtsi 里写"根级具名节点"**，必须包在 `/ { ... };` 里，否则报
`syntax error`，而且**报出来的行列会指向节点名，误导你以为是命名非法**（连字符节点名在
Rockchip dts 里到处都是）。已实测：包进 `/ {}` 通过。

配套时序：`R90 10kΩ × C238 100nF → τ≈1ms`，加 LDO 自身启动，**从拉高使能到真正带电至少
2~3ms**；而驱动 `__ov5695_power_on()` 原本只等 `8192 cycles / 24MHz ≈ 341µs`，差一个数量级。

验证结果：`gpio-8` 归 `camPwr` 所有、`use_count=1`，传感器第一次就 `Detected OV005695`。

---

## 5. `io-mode`：dmabuf 只对 root 可用

同一条 pipeline，只换运行身份：

| 身份 | `io-mode=4`(dmabuf) | `io-mode=0`(auto) | `io-mode=2`(MMAP) |
| --- | --- | --- | --- |
| root | 通过 | 通过 | 通过 |
| `lckfb` | **失败** | **失败** | 通过 |

失败信息：`Failed to allocate required memory` / `we received 0 buffer from device`。
但 `REQBUFS` 两种身份都是 `asked=4 granted=4`，所以卡点在 dmabuf 导出那一步。

**已排除**：`RLIMIT_MEMLOCK`（抬到 unlimited 后照样失败）、CMA（`v4l2-ctl --stream-mmap`
正常出帧）、Qt 的 EGL/GL 上下文（`offscreen` 也失败）、开机时序、双实例竞争、
GStreamer 注册表缓存。

**未查明**：`VIDIOC_EXPBUF` 为什么对普通用户失败。板上没有 `/dev/ion`、没有
`/dev/dma_heap`，也没有 strace 可抓 errno。

实践结论：app 用 `io-mode=2`。这里每帧本来就要逐像素拷进 `QImage`，dmabuf 的零拷贝
从来没有收益。

> **方法论教训**：我一度反复用 root 跑 gst-launch 做对照，得出"独立跑正常、是 app 的问题"，
> 白白排除了好几个正确方向。**对照实验的运行身份必须和被测对象一致**，否则对照组无效。

---

## 6. 音频：pulseaudio 独占 card 0

`MediaService.h` 原本写死 `alsasink`。板子上：

```
ERROR: GstAlsaSink: Could not open audio device for playback.
Device is being used by another application. Device 'default' is busy
```

因为 pulseaudio 持有声卡，而系统**没装 `alsa-plugins-pulseaudio`**，alsasink 直连 hw 就撞车。
改成 `autoaudiosink` 后通过（有 pulse 走 pulsesink，没有退回 alsasink）。

验证不能只看状态栏——`MEDIA:OK` 只表示 playbin 建起来了。要看：

```bash
pactl list sink-inputs | grep -E "application.name|media.title|State"
ls -l /proc/$(pgrep -x smart-cockpit)/fd | grep -c snd    # 0 就是根本没打开设备
```

codec 侧只有两个控件，**没有 `Master`**（`amixer -c 0 sget Master` 会失败）：

```
'Playback Path'      # 路由枚举：OFF/RCV/SPK/HP/SPK_HP/...  实测值 6 = SPK_HP
'Capture MIC Path'
```

媒体目录：`SMART_COCKPIT_MEDIA_DIR`，未设置则 `QStandardPaths::MusicLocation`（即 `~/Music`）。
`*.mp3 *.flac *.wav *.ogg`。

---

## 7. 调试控制台：接错排针就"零输出"

- 控制台在板上 **`CN2`，一个 GH1.25-S04DCA-00 的 4Pin 小座**（1.25mm 间距），
   TX/RX 各串 1kΩ（R88/R89）并挂 TVS。出处是硬件手册 P36 那张「UART2 接口防护电路」原理图。
- **40Pin 扩展口上没有调试控制台**，只有 `UART3_TX_M1`(pin10) / `UART3_RX_M1`(pin12)。
  接 40Pin 必然零输出，与转接板好坏无关。
- 电平是 **3.3V**（PMUIO2 由 `VCC3V3_PMU` 供电），所以 CH340 直连即可，不需要电平转换。
  手册 P35 那句"tablet 参考设计可能 1.8V"在本板不适用。
- 波特率 **1500000**；CH340 实测跑得动（TX↔RX 自环在 1500000 和 115200 两档都完整回显）。
- 内核在调试口上开了 root getty（`loginctl` 里能看到 `ttyFIQ0` 会话），所以**不需要键盘、
  密码、网络就能执行命令**。
- ⚠️ **COM 号每次都可能变**（实测 COM5 → COM6 → COM8）。用之前先重新枚举端口，
  否则会像我们第一次那样在错误端口上测出"零输出"并误判成硬件故障。

串口在本项目的价值：**U-Boot 阶段的输出 ssh 永远看不到**，而 boot 早期问题
（panic、传感器 probe、DRM 分配）恰好都在那一段。

---

## 8. 构建与工具链陷阱（每一条都真实浪费过时间）

| 陷阱 | 现象 | 对策 |
| --- | --- | --- |
| **板子 RTC 每次冷启掉回 2023-03-27** | `make` 认为源文件比目标旧 → **静默跳过重编**，你以为改了其实没改 | 构建前 `date -u -s`，`touch -d 2030-01-01 00:00` 源文件，然后**反查产物** |
| `QStringLiteral` 存 UTF-16 | `strings 二进制 \| grep 关键字` 永远搜不到，误判"代码没编进去" | 用 `strings -el`；或更硬的办法 `nm -C` 查符号 |
| `/tmp` 是 **tmpfs** | 重启后放到 `/tmp` 的辅助脚本全消失，采集拿到空数据 | 放 `/home/...`，或每轮重新上传 |
| `sudo -S` 与 `bash -s` 抢 stdin | `ssh 'echo pw \| sudo -S bash -s' <<HEREDOC` 把脚本内容当命令执行 | 脚本先落地再 `sudo bash /path/to.sh` |
| dtc 报错位置 ≠ 原因 | "根级具名节点"被拒，却报在节点名那一列，看起来像命名非法 | 包进 `/ { }`；别信报错位置 |
| `grep -c $'\r'` 测行尾 | 对任何文件都返回总行数，得出"CRLF"假结论 | `tr -dc '\r' \| wc -c` |
| 临时 `git init` 验 patch | Windows `core.autocrlf` 在 checkout 时改行尾，伪造出"行尾不一致" | 用 `patch -p1` + `cmp` |
| `sha256sum * > S; sha256sum -c S` | 先重算再自校 = 循环验证，把 scp 失败掩盖成全部 OK | 校验和要在传输**之前**生成，或比对远端与本地的实际内容 |
| `set -e` 遇到 `diff` | `diff` 有差异就返回 1，脚本静默退出，看起来像"成功但没输出" | 显式检查，别在 diff 的地方用 `set -e` |
| 嵌套引号（Git Bash → ssh → sudo → bash -c） | 反斜杠和 `$_` 被吃掉，报 `unexpected EOF` | 写成脚本文件传过去执行 |

---

## 9. 操作禁忌

- ⚠️ **不要对 `ov5695` 做 `unbind`**。`echo 4-0036 > .../unbind` 会触发内核 NULL 解引用
  （`ov5695_remove → v4l2_async_unregister_subdev`），只杀掉发起进程，但把设备留在
  **半绑定**坏状态：`/sys/bus/i2c/drivers/ov5695/` 里仍列着 4-0036，而 `media-ctl` 里
  没有 ov5695，此时 `bind` 是空操作（再 echo 报 `I/O error`），**只能重启**。
- ⚠️ **不要 `pkill` 正在推流的 app**。rkisp 在进程被强杀时不回收 vb2 缓冲，会把采集
  缓冲池耗干，之后 `REQBUFS` 返回 0 个缓冲，只能重启恢复。要停就用哨兵
  `touch /home/lckfb/lab/disable` 让守护循环不再拉起，然后等 app 正常退出。
- ⚠️ **不要同时跑两个 app 实例**。它们会抢 `/dev/video0`，产生
  `libv4l2: error setting pixformat: Device or resource busy` 这类**假根因**。
- **不要用 `reboot` 的结论代替冷启动**。GPIO/GRF 寄存器只有 POR（真断电）才复位，
  warm reboot 可能把上一次的状态泄漏进下一次。
- **不要用 PC USB 给板子供电**，走板载 DC 口。USB 供电会导致掉电复位循环。

---

## 10. 未解决 / 待验证

| 项 | 状态 |
| --- | --- |
| 冷启动识别率 | **未定量**。电源现在被 regulator 主动驱动，理论上不再"抛硬币"，但只观察到一次冷启动成功。用 `deployment/taishanpi/` 之外的 `coldboot_verify.sh` 跑 3 轮 |
| `EXPBUF` 为何对普通用户失败 | 未查明，见第 5 节 |
| 摄像头画质 | 只能软件增益。`analogue_gain` 已在最大 248，只剩 `exposure`（4..2020，当前 1104）可拉大 |
| `can0` 实车 | 未验证：需外接 CANFD 收发器 + dts 使能 |
| `CanManager` 扩展帧掩码 | 仓库列为已知问题，**本次未复核也未修改** |
| 旋转方向的可测试性 | `tst_cameraservice` 只能钉住帧尺寸（转置后 48x64），钉不住方向——videotestsrc 的 ball 图案对称。方向目前靠实机照片确认 |
| `AA_ShareOpenGLContexts` | **不需要**。板上和仓库的 `main.cpp` 都没有它，app 在 xcb 下正常出图。此前"已插入"的说法不成立 |

---

## 11. 怎么自己复现这些结论

```bash
# 显示输出与模式
ls /sys/class/drm/*/modes

# 摄像头是否被 probe、链路是否建立
dmesg | grep -iE "Detected OV|Unexpected sensor|No link between dphy"
media-ctl -d /dev/media0 -p | grep -c ov5695

# 电源使能脚归属与状态
cat /sys/kernel/debug/gpio | grep "gpio-8 "
grep -i campwr /sys/kernel/debug/regulator/regulator_summary

# PMU GRF 里 GPIO0_B0 的上下拉编码（0=无拉 1=上拉 2=下拉）
# 用 EVIOCGABS 同款办法读 /dev/mem 的 0xFDC20024

# 采集缓冲能不能分到
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1280,height=720,pixelformat=NV12 \
         --stream-mmap --stream-count=3 --stream-to=/tmp/f.raw

# 音频到底有没有出去（别看状态栏）
pactl list sink-inputs | grep -E "application.name|media.title"
ls -l /proc/$(pgrep -x smart-cockpit)/fd | grep -c snd

# 传感器地址在不在总线上（I2C_RDWR 不需要占用地址，I2C_SLAVE 会 EBUSY）
# 见本文第 5 节，用 ctypes 构造 i2c_rdwr_ioctl_data
```
