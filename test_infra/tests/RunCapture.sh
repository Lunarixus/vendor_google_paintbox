#! /bin/bash

if [ $# -ne 1 ]; then
   echo Usage: incorrect argument input, need specify a loop count
   exit 1
fi

echo
echo "Android camera open loop test"
echo "Press CTRL-C to exit"
echo

model="$(adb shell getprop ro.product.model)"
echo "This is a $model device"

# Prepare the device for the test
adb root
echo "Make device stay on"
adb shell svc power stayon true
sleep 1
echo "Unlock lock screen"
adb shell input keyevent 82
sleep 1

echo "Disable accelerometer auto rotation"
adb shell settings put system accelerometer_rotation 0
sleep 2
echo "Lock device orientation to landscape"
adb shell settings put system user_rotation 1
sleep 2

adb logcat -c

loop_count=$1
echo "Loop camera open test for $loop_count times"

c=1
while [ $c -le $loop_count ]
do

echo "Loop count: $c"

red=`tput setaf 1`
reset=`tput sgr0`
errorMsg="$(adb logcat -d |grep -i 'show fatal')"
if [[ $errorMsg == *"error"* ]]
	then
    echo "${red}Test failed!!!!!!!!!${reset}"
    adb bugreport
    exit
fi

# launch GoogleCamera app:
echo "open camera..."
adb shell am start -a android.intent.action.MAIN -c android.intent.category.LAUNCHER -n com.google.android.GoogleCamera/com.android.camera.CameraLauncher
sleep 2

#take a picture
echo "take picture..."
adb shell input keyevent 27
sleep 1


# Switch to front camera
echo "Switch to front camera..."
if [[ $model == *"XL"* ]]
	then
	#taimen
	adb shell input tap 2460 1250
	else
    #walleye
	adb shell input tap 1690 960
fi
sleep 2

#take a picture
echo "take picture..."
adb shell input keyevent 27
sleep 1

# Switch back camera recording
echo "Switch to back camera..."
if [[ $model == *"taimen"* ]]
	then
	#taimen
	adb shell input tap 2460 1250
	else
    #walleye
	adb shell input tap 1690 960
fi
sleep 2

# exit GoogleCamera app
echo "close camera..."
adb shell input keyevent 4
sleep 1

((c++))
done
