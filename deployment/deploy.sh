#!/usr/bin/env bash
# 部署到泰山派 RK3566。
#
# 本板实测走的是 X11/xcb + 桌面会话自启，不是 systemd/eglfs，所以默认部署
# deployment/taishanpi/ 那一套。systemd 单元保留给其他有可用 EGL 平台的板子。
#
# 用法：
#   ./deployment/deploy.sh lckfb@192.168.7.2 [path-to-binary]
#
# 二进制默认 build/smart-cockpit。注意：本项目在板子上原生编译更省事
# （2GB RAM 足够，实测一次增量编译几十秒），交叉编译路径未在实机验证过。
set -euo pipefail

TARGET="${1:-lckfb@192.168.7.2}"
BIN="${2:-build/smart-cockpit}"

if [[ ! -f "${BIN}" ]]; then
  echo "Binary not found: ${BIN}"
  echo "要么先在板子上原生编译，要么本地交叉编译（交叉路径未验证，见 docs/15）。"
  exit 1
fi

echo ">>> 传二进制到 ${TARGET} ..."
rsync -avz --progress "${BIN}" "${TARGET}:/home/lckfb/b/smart-cockpit"

echo ">>> 传自启工件并安装 ..."
tar -C deployment -czf /tmp/taishanpi.tgz taishanpi
rsync -az /tmp/taishanpi.tgz "${TARGET}:/tmp/"
ssh "${TARGET}" 'echo lckfb | sudo -S -p "" bash -c "
  rm -rf /tmp/taishanpi && mkdir -p /tmp/taishanpi && cd /tmp &&
  tar -xzf /tmp/taishanpi.tgz && cd taishanpi && ./install.sh"'

echo ">>> 完成。日志："
echo "    ssh ${TARGET} tail -f /home/lckfb/lab/logs/smart-cockpit.log"
echo "    暂停自启： ssh ${TARGET} 'touch /home/lckfb/lab/disable'"
