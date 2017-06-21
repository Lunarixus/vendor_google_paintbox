#!/bin/bash

# A script to run camera CTS test by test.

set -o errexit

adb wait-for-device

pushd ${ANDROID_BUILD_TOP}

LOG=~/cameracts.log
RUNTEST=${ANDROID_BUILD_TOP}/development/testrunner/runtest.py

echo "#################" | tee -a $LOG
echo "Camera CTS Report" | tee -a $LOG
date | tee -a $LOG
echo "#################" | tee -a $LOG

TESTS="
	camera2/cts/FastBasicsTest.java \
	camera2/cts/CameraManagerTest.java \
	camera2/cts/NativeCameraManagerTest.java \
	camera2/cts/NativeImageReaderTest.java \
	camera2/cts/DngCreatorTest.java \
	camera2/cts/ExtendedCameraCharacteristicsTest.java \
	camera2/cts/CaptureResultTest.java \
	camera2/cts/MultiViewTest.java \
	camera2/cts/NativeStillCaptureTest.java \
	camera2/cts/SurfaceViewPreviewTest.java \
	camera2/cts/BurstCaptureRawTest.java \
	camera2/cts/AllocationTest.java \
	camera2/cts/StaticMetadataTest.java \
	camera2/cts/FlashlightTest.java \
	camera2/cts/BurstCaptureTest.java \
	camera2/cts/ImageReaderTest.java \
	camera2/cts/ImageWriterTest.java \
	camera2/cts/CaptureRequestTest.java \
	camera2/cts/NativeCameraDeviceTest.java \
	camera2/cts/PerformanceTest.java \
	camera2/cts/CameraDeviceTest.java \
	cts/Camera_ParametersTest.java \
	cts/CameraGLTest.java \
	cts/CameraTest.java \
	cts/Camera_SizeTest.java \
	multiprocess/camera/cts/CameraEvictionTest.java \
"

FLAKY_TESTS="
	camera2/cts/RecordingTest.java \
	camera2/cts/ReprocessCaptureTest.java \
	camera2/cts/RobustnessTest.java \
	camera2/cts/StillCaptureTest.java \
"

echo "############" | tee -a $LOG
echo "NORMAL TESTS" | tee -a $LOG
echo "############" | tee -a $LOG
for test in ${TESTS}; do
	echo "Running ${test}" | tee -a $LOG
	${RUNTEST} -x cts/tests/camera/src/android/hardware/${test} | tee -a $LOG
done

echo "###########" | tee -a $LOG
echo "FLAKY TESTS" | tee -a $LOG
echo "###########" | tee -a $LOG
for test in ${FLAKY_TESTS}; do
	echo "Running ${test}" | tee -a $LOG
	${RUNTEST} -x cts/tests/camera/src/android/hardware/${test} | tee -a $LOG
done

popd
