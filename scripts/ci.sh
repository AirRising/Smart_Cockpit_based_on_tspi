#!/usr/bin/env bash
# Local CI runner: builds the project and runs every unit/smoke test headless.
#
# Replicates .github/workflows/ci.yml on any machine (your dev box or a cloud
# server), so you can run the same gate locally before pushing.
#
# Usage:
#   sudo ./scripts/ci.sh            # root or sudo
#   sudo ./scripts/ci.sh build/ci   # optional custom build dir
set -euo pipefail

BUILD_DIR="${1:-build/ci}"

if [[ ${EUID} -ne 0 ]]; then
  echo "Please run as root (or via sudo): sudo $0"
  exit 1
fi

echo ">>> Installing build dependencies..."
apt-get update
apt-get install -y --no-install-recommends \
  build-essential cmake pkg-config \
  qtbase5-dev qtbase5-dev-tools libqt5dbus5 libqt5test5 \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-plugins-base gstreamer1.0-plugins-good

echo ">>> Configuring (${BUILD_DIR})..."
cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON

echo ">>> Building..."
cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo ">>> Running tests..."
ctest --test-dir "${BUILD_DIR}" --output-on-failure

echo ">>> CI OK."
