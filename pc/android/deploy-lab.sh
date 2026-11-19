#!/bin/bash
# Deploy the MGR:R PC lab build to a device and start it.
#   deploy-lab.sh <serial> [seconds=90] [env lines...]
# Libraries go to the app's private files dir via run-as (never inside the APK). Log: build/lab/pc/runs/<serial>-<time>.log
set -e
S=$1; SECS=${2:-90}; shift 2 || true
P=com.dfdx047.zandatsu.pclab; ROOT=$(cd "$(dirname "$0")/../.." && pwd); B=${MGR_BUILD:-$ROOT/build/pc/android}
EXE=${MGR_EXE:-/storage/6A55-7C3B/MGRPC/METAL GEAR RISING REVENGEANCE.exe}
adb -s "$S" shell am force-stop $P || true
LIBDIR=${MGR_LIBDIR:-/storage/6A55-7C3B/MGRPC/lab}   # read into a memfd by the loader (internal storage is full)
adb -s "$S" shell mkdir -p "$LIBDIR"
if [ "${SKIP_LIBS:-0}" != 1 ]; then
  adb -s "$S" push "$B/dxvk/libdxvk_android.so" "$LIBDIR/libdxvk_android.so" >/dev/null
  adb -s "$S" push "$B/libmgrpc.stripped.so" "$LIBDIR/libmgrpc.so" >/dev/null
fi
printf '%s\n' "$LIBDIR" > "$B/mgr_libdir.txt"
# MGR_DRIVER=<dir with libvulkan_freedreno.so>: use that driver (Turnip) through adrenotools; empty = system driver.
if [ -n "$MGR_DRIVER" ]; then
  adb -s "$S" push "$MGR_DRIVER/libvulkan_freedreno.so" /data/local/tmp/ >/dev/null
  adb -s "$S" shell "run-as $P mkdir -p files/drivers && run-as $P cp /data/local/tmp/libvulkan_freedreno.so files/drivers/ && rm /data/local/tmp/libvulkan_freedreno.so"
  echo libvulkan_freedreno.so > "$B/mgr_driver.txt"
else : > "$B/mgr_driver.txt"; fi
printf '%s\n' "$EXE" > "$B/mgr_exe.txt"; printf '%s\n' "$@" > "$B/mgr_env.txt"
for f in mgr_exe.txt mgr_env.txt mgr_libdir.txt mgr_driver.txt; do
  adb -s "$S" push "$B/$f" /data/local/tmp/ >/dev/null; adb -s "$S" shell "run-as $P cp /data/local/tmp/$f files/$f && rm /data/local/tmp/$f"
done
mkdir -p "$ROOT/build/lab/pc/runs"; LOG=$ROOT/build/lab/pc/runs/$(echo "$S" | tr ':.' '__')-$(TZ=Europe/Lisbon date +%H%M%S).log
adb -s "$S" logcat -c
adb -s "$S" shell am start -n $P/android.app.NativeActivity >/dev/null
timeout "$SECS" adb -s "$S" logcat -v time -s MGRLAB NFSU2 MGR MGRGAME TRACE DEBUG DXVK libc AndroidRuntime > "$LOG" || true
echo "$LOG"
