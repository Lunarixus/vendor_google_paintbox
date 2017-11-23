#! /bin/bash

usage() {
cat << EOF
  usage: $(basename $0) [-s|--serial {serial_no}] [--loop {loop}]
                        [--shot {shot}] [--interval {interval}] [--eng]
                        [--ignore-photo-count] [--verbose]
    -s|--serial             Set Android device serial number
    --loop      {loop}      How many loops to test
    --shot      {shot}      Shots per loop
    --interval  {interval}  intervals between shot in second
    --eng                   Use Eng build Google Camera App
    --ignore-photo-count    Does not check number of jpeg files after each shot
    --verbose               Print log to console on taking every picture
EOF
}

func_set_device_serial() {
  export ANDROID_SERIAL="$1"
  echo "Use serial number ${ANDROID_SERIAL}"
}

func_count_photo() {
  adb shell find /sdcard/DCIM/Camera /sdcard/Pictures -name "*.jpg" | wc -l
}

func_quit_test() {
  if [[ $1 -ne 0 ]]; then
    local screenshot_filename=screenshot-`date +%F-%H-%M-%S`.png
    echo "Saving ${screenshot_filename}"
    adb shell screencap -p > ${screenshot_filename}
    echo "Taking bugreport before exit..."
    adb bugreport
  fi

  echo "There are $( func_count_photo ) photos."

  echo "closing camera..."
  adb shell am force-stop ${package_name}
  sleep 1

  echo "Lock device orientation to portrait"
  adb shell settings put system user_rotation 0
  exit $1
}

loop="1"
shot="5"
interval="4"
package_name="com.google.android.GoogleCamera"
num_old_photos="-1"
num_new_photos="0"
ignore_photo_count=false
is_verbose=false

while [ $# -gt 0 ]; do
  arg="$1"
  shift
  case "$arg" in
    "-h")
      usage
      exit 0
      ;;
    "-s" | "--serial")
      arg="$1"
      shift
      func_set_device_serial $arg
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
    "--eng")
      package_name="com.google.android.GoogleCameraEng"
      ;;
    "--ignore-photo-count")
      ignore_photo_count=true
      ;;
    "--verbose")
      is_verbose=true
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

# Delete old photos
# adb shell "rm /sdcard/DCIM/Camera/*.jpg"

# Prepare the device for the test
adb root
echo "Make device stay on"
adb shell svc power stayon true
sleep 1
echo "Unlock lock screen"
adb shell input keyevent 82
sleep 1

# Check if package exists
list_package="$(adb shell pm list packages ${package_name})"
if [[ $list_package == "" ]]
  then
  echo "No such application: ${package_name}"
  exit
fi

echo "Disable accelerometer auto rotation"
adb shell settings put system accelerometer_rotation 0
sleep 2
echo "Lock device orientation to landscape"
adb shell settings put system user_rotation 1
sleep 2

adb logcat -c

echo "Loop camera open test for $loop times"
echo "There are $( func_count_photo ) photos."

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
  exit 1
fi

# launch GoogleCamera app:
echo "open camera..."
adb shell am start -a android.intent.action.MAIN \
                   -c android.intent.category.LAUNCHER \
                   -n ${package_name}/com.android.camera.CameraLauncher
sleep 2

#take a picture
echo "take picture..."

for i in `seq 1 $shot`;
do
  num_old_photos=$( func_count_photo )
  if "$is_verbose" ; then
    echo "  back camera shot $i"
  fi
  adb shell input keyevent 27
  sleep $interval
  num_new_photos=$( func_count_photo )

  if [[ $num_new_photos -le $num_old_photos ]] && [[ "$ignore_photo_count" = false ]]; then
    echo "Not seeing new photos: old=$num_old_photos, new=$num_new_photos."
    echo "Abort test."
    func_quit_test 1
  fi
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
  num_old_photos=$( func_count_photo )
  if "$is_verbose" ; then
    echo "  front camera shot $i"
  fi
  adb shell input keyevent 27
  sleep $interval
  num_new_photos=$( func_count_photo )

  if [[ $num_new_photos -le $num_old_photos ]] && [[ "$ignore_photo_count" = false ]]; then
    echo "Not seeing new photos: old=$num_old_photos, new=$num_new_photos."
    echo "Abort test."
    func_quit_test 1
  fi
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

((c++))
done

func_quit_test 0
