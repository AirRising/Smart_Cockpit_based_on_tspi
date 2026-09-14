#!/usr/bin/env bash
# Create the vcan0 virtual CAN interface for host-side (Ubuntu) debugging.
set -e

sudo modprobe vcan
sudo ip link add dev vcan0 type vcan 2>/dev/null || true
sudo ip link set up vcan0
ip -details link show vcan0
echo "vcan0 is up. Try: candump vcan0"
