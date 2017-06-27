#!/bin/bash

set -e

if [ $# -ne 1 ]; then
	echo "Please specify the test you want to run."
	echo "Available tests under /data/nativetest:"
	adb shell ls /data/nativetest
	exit -1
fi

# Script to run easel comm & control related tests.
# Preparation:
# 1) Copy test server program to ramdisk source.
# 2) Build ramdisk to autostart test server but not pbserver.
# 3) Flash the new ramdisk to phone.
adb wait-for-device root
adb wait-for-device
adb shell "echo 1 > /sys/devices/virtual/misc/mnh_sm/stage_fw"
adb shell "cat /sys/devices/virtual/misc/mnh_sm/poweroff"
adb shell "setprop camera.hdrplus.donotpoweroneasel 1"
adb shell "pkill -f camera"
adb shell "/data/nativetest/$1/$1"
adb shell "setprop camera.hdrplus.donotpoweroneasel 0"
adb shell "pkill -f camera"
