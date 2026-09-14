#!/usr/bin/env bash
# Send synthetic frames on vcan0 to exercise the instrument cluster.
# Requires can-utils (sudo apt install can-utils).
set -e

if ! ip link show vcan0 >/dev/null 2>&1; then
  echo "vcan0 not found - run scripts/vcan-setup.sh first"
  exit 1
fi

echo "Sending 0x100 (speed=120km/h, rpm=3000, fuel=50%, left indicator)..."
#
# 0x100 layout (little endian):
#   speed[15:0]  = 120    -> 78 00
#   rpm[31:16]   = 3000   -> b8 0b
#   fuel[39:32]  = 100*0.5=50 -> 64
#   left_ind[40] = 1      -> byte5 bit0
#
cansend vcan0 100#7800b80b64010000

echo "Sending 0x300 reverse engaged (gear_signal byte = 0x01)..."
cansend vcan0 300#0100000000000000

echo "Sending 0x200 climate echo (temp=24, fan=3, ac=1, ack=1)..."
# temp[7:0]=24, fan[11:8]=3, blow_mode[13:12]=0, ac[14]=1, auto[15]=0, ack[16]=1
# byte0=24, byte1=0x03|0x40(ac)=0x43, byte2=bit0 ack = 0x01
cansend vcan0 200#1843010000000000

echo "Sending 0x400 steering angle +300 deg (0.1 factor, signed)..."
# raw = 300/0.1 = 3000 = 0x0BB8
cansend vcan0 400#b80b000000000000

echo "Done."
