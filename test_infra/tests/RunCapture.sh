#!/bin/bash

# A camera test script to repeatedly start and stop camera app and taking pictures.
# Repeat start camera app times: 3
# Shots taken in one round: 10

set -e

adb wait-for-device root
adb wait-for-device

adb shell "input keyevent KEYCODE_MENU"
adb shell "setprop persist.camera.hdrplus.enable 1"
adb shell "killall cameraserver"

for i in $(seq 1 3)
do
	adb shell "am start -n com.google.android.GoogleCamera/com.android.camera.CameraLauncher"
	for i in $(seq 1 10);
	do
		adb shell "input keyevent KEYCODE_CAMERA"
		sleep 1
	done
	adb shell "am force-stop com.google.android.GoogleCamera"
done