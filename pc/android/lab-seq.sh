#!/bin/bash
# Scripted input + screenshots against the running lab app (Odin 2 Mini).
#   lab-seq.sh <serial> <outdir> step...    steps: w<sec>  k<KEYCODE suffix>  t<x>,<y> (tap, screen px)  s<name> (screenshot)
#          p<file> (send a pilot file: stick/btn steps, see mgr_android_main.cpp)
#   example: lab-seq.sh $O runs/m2 w50 kBUTTON_START w4 sTitle kBUTTON_A
S=$1; OUT=$2; shift 2; mkdir -p "$OUT"
for step in "$@"; do
  arg=${step:1}
  case ${step:0:1} in
    w) sleep "$arg" ;;
    k) adb -s "$S" shell input keyevent "KEYCODE_$arg" ;;
    t) adb -s "$S" shell input tap "${arg%,*}" "${arg#*,}" ;;
    p) adb -s "$S" push "$arg" /data/local/tmp/mgr_pilot.txt >/dev/null && adb -s "$S" shell "run-as com.dfdx047.zandatsu.pclab cp /data/local/tmp/mgr_pilot.txt files/mgr_pilot.txt" ;;
    s) adb -s "$S" exec-out screencap -p > "$OUT/$arg.png"; echo "$(TZ=Europe/Lisbon date +%T) $arg" ;;
  esac
done
