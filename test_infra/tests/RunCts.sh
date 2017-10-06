#!/bin/bash

# A script to run camera CTS test by test.

set -o errexit

adb wait-for-device

pushd ${ANDROID_BUILD_TOP}

# Configures the phone for CTS
adb root
adb wait-for-device
adb shell svc power stayon true
adb shell setprop persist.camera.hdrplus.enable 1
adb shell pkill -f camera

DATE="$(date)"
LOG="/tmp/cameracts.${DATE// /_}.log"
RUNTEST=${ANDROID_BUILD_TOP}/development/testrunner/runtest.py

echo "#################" | tee -a $LOG
echo "Camera CTS Report" | tee -a $LOG
echo "${LOG}"
echo "#################" | tee -a $LOG

EASEL_TESTS="
	camera2/cts/FastBasicsTest.java \
	camera2/cts/NativeStillCaptureTest.java \
	camera2/cts/PerformanceTest.java \
	camera2/cts/StillCaptureTest.java \
	cts/CameraTest.java \
"

for test in ${EASEL_TESTS}; do
	echo "$(date): Running ${test}" | tee -a $LOG
	SECONDS=0
	${RUNTEST} -x cts/tests/camera/src/android/hardware/${test} | tee -a $LOG
	echo "$(date): ${test} finished in ${SECONDS} seconds" | tee -a $LOG
done

popd
