#!/usr/bin/env bash
# 把本目录内容装到泰山派上（在板子上以 lckfb 运行，需要 sudo）。
#
# 这是本仓库唯一在泰山派 RK3566 上实测跑通过的自启方案。
# deployment/smart-cockpit.service 那套 systemd 方案没有在这块板上验证过，
# 因为 eglfs 不可用、且 root 单元拿不到用户的 PulseAudio/X 会话。
#
# 用法（在板子上）：
#   tar -xzf taishanpi.tar.gz && cd taishanpi && sudo ./install.sh
set -euo pipefail

USER_NAME="${SUDO_USER:-lckfb}"
HOME_DIR=$(getent passwd "$USER_NAME" | cut -d: -f6)

install -d -m 755 "$HOME_DIR/lab"
install -m 755 lab/cockpit-loop.sh "$HOME_DIR/lab/cockpit-loop.sh"

install -d -m 755 "$HOME_DIR/.config/autostart"
install -m 644 .config/autostart/*.desktop "$HOME_DIR/.config/autostart/"

install -D -m 644 etc/security/limits.d/99-camera-memlock.conf \
        /etc/security/limits.d/99-camera-memlock.conf
install -D -m 644 etc/lightdm/lightdm.conf.d/50-autologin.conf \
        /etc/lightdm/lightdm.conf.d/50-autologin.conf
install -D -m 644 etc/systemd/system/usb0-up.service \
        /etc/systemd/system/usb0-up.service

# 防息屏 + 防误触 PWR。缺了 logind 覆盖的话，按一下 PWR 会直接关机。
install -D -m 644 /dev/null /etc/systemd/logind.conf.d/99-lab.conf
cat > /etc/systemd/logind.conf.d/99-lab.conf <<'CONF'
[Login]
HandlePowerKey=ignore
HandleLidSwitch=ignore
IdleAction=ignore
IdleActionSec=0
CONF

chown -R "$USER_NAME":"$USER_NAME" "$HOME_DIR/lab" "$HOME_DIR/.config/autostart"
systemctl daemon-reload
systemctl enable --now usb0-up.service || true

cat <<'DONE'

装好了。要点：

  暂停自启（调试时用，别再 pkill 正在推流的进程）：
      touch ~lckfb/lab/disable
  恢复：
      rm -f ~lckfb/lab/disable

  二进制路径由 cockpit-loop.sh 里 BIN= 决定；它不存在时脚本直接退出，
  这是设计行为，不是故障。

  需要重启一次让 logind 覆盖生效（HandlePowerKey 只在 logind 启动时读）。
DONE
