#!/usr/bin/env bash
# One-shot deployment to the TaisanPi.
#
# Usage:
#   ./deployment/deploy.sh root@192.168.1.100 [path-to-binary]
#
# The binary defaults to build-arm/smart-cockpit (cross build output).
set -euo pipefail

TARGET="${1:-root@taisanpi}"
BIN="${2:-build-arm/smart-cockpit}"

if [[ ! -f "${BIN}" ]]; then
  echo "Binary not found: ${BIN}"
  echo "Cross compile first: see README.md section '交叉编译'."
  exit 1
fi

echo ">>> Copying binary to ${TARGET}..."
rsync -avz --progress "${BIN}" "${TARGET}:/usr/local/bin/smart-cockpit"

echo ">>> Installing systemd unit..."
rsync -avz deployment/smart-cockpit.service \
      "${TARGET}:/etc/systemd/system/smart-cockpit.service"
rsync -avz deployment/kms-config.json \
      "${TARGET}:/etc/smart-cockpit/kms-config.json"

echo ">>> Preparing runtime directories..."
ssh "${TARGET}" 'mkdir -p /var/log/smart-cockpit && chmod 755 /var/log/smart-cockpit'

echo ">>> Enabling service..."
ssh "${TARGET}" 'systemctl daemon-reload && systemctl enable smart-cockpit && systemctl restart smart-cockpit'

echo ">>> Done. Logs: ssh ${TARGET} journalctl -u smart-cockpit -f"
