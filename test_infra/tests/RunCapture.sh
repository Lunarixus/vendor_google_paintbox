#! /bin/bash

usage() {
cat << EOF
  Usage:
  $0 [--loop {loop}] [--shot {shot}] [--interval {interval}]
    --loop      {loop}      How many loops to test
    --shot      {shot}      Shots per loop
    --interval  {interval}  intervals between shot in second
EOF
}

loop="1"
shot="1"
interval="1"
while [ $# -gt 0 ]; do
  arg="$1"
  shift
  case "$arg" in
    "-h")
      usage
      exit 0
      ;;
    "--loop")
      arg="$1"
      shift
      loop="$arg"
      ;;
    "--shot")
      arg="$1"
      shift
      shot="$arg"
      ;;
    "--interval")
      arg="$1"
      shift
      interval="$arg"
      ;;
    *)
      usage
      exit 0
      ;;
  esac
done

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

echo "Loop camera open test for $loop times"

c=1
while [ $c -le $loop ]
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

for i in `seq 1 $shot`;
do
  adb shell input keyevent 27
  sleep $interval
done


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
for i in `seq 1 $shot`;
do
  adb shell input keyevent 27
  sleep $interval
done

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
