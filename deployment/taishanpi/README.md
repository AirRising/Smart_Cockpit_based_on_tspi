# 泰山派 RK3566 实测自启方案

本目录是**唯一在这块板上跑通过**的部署方式，和 `deployment/smart-cockpit.service`
那套 systemd/eglfs 方案是两条路，不要混用。

为什么不用 systemd + eglfs：

| 假设 | 实测结果 |
| --- | --- |
| eglfs 直显 | 纯色 `QOpenGLWidget` 探针也整屏全黑 → 不可用 |
| linuxfb 兜底 | `cannot mmap framebuffer` → 不可用 |
| 以 root 运行 | 拿不到 `:0` 的 X 授权，也连不上用户会话的 PulseAudio socket |
| KMS 输出 HDMI-A-1 1280x800 | 本板是 `card0-DSI-1: 800x1280`，HDMI 没接屏 |

实际可用组合：lightdm 自动登录 `lckfb` → XFCE 会话 → `~/.config/autostart` 拉起
守护脚本 → `QT_QPA_PLATFORM=xcb` + `LIBGL_ALWAYS_SOFTWARE=1`。

## 文件对应关系

| 本目录 | 装到板上 | 作用 |
| --- | --- | --- |
| `lab/cockpit-loop.sh` | `/home/lckfb/lab/cockpit-loop.sh` | 守护循环：崩溃自动重启；`disable` 哨兵存在时暂停 |
| `.config/autostart/smart-cockpit.desktop` | 同名 | 会话内自启 |
| `.config/autostart/no-blank.desktop` | 同名 | `xset s off / s noblank / -dpms`，防息屏 |
| `etc/security/limits.d/99-camera-memlock.conf` | 同名 | 抬升 memlock（注意：音频故障与它无关，见下） |
| `etc/lightdm/lightdm.conf.d/50-autologin.conf` | 同名 | 自动登录 `lckfb` |
| `etc/systemd/system/usb0-up.service` | 同名 | 开机配 `usb0` 静态 IP `192.168.7.2/24`，否则 RNDIS ssh 不通 |
| `install.sh` 生成 | `/etc/systemd/logind.conf.d/99-lab.conf` | `HandlePowerKey=ignore`，防按 PWR 直接关机 |

## 哨兵机制

```bash
touch /home/lckfb/lab/disable   # 暂停自启（调试前必做）
rm  -f /home/lckfb/lab/disable  # 恢复
```

调试摄像头时**先设哨兵再手动起实例**。两个实例并存会互相抢 `/dev/video0`，
产生 `Device or resource busy` 这种假根因，浪费过大量排查时间。

停 app 也别用 `pkill` 打正在推流的进程——rkisp 在进程被强杀时不回收 vb2 缓冲，
会把采集缓冲池耗干，之后 `REQBUFS` 返回 0 个缓冲，只能重启恢复。
