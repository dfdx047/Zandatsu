#!/usr/bin/env bash
# device-profile.sh (ProjectSwordMan copy 2026-11-19 of the Road to Horizons copy, 2026-11-14, of Slipstream's scripts/android/device-profile.sh, itself from the
# owner's HiddenLeaf project, tools/device-profile.sh): limits a handheld (Retroid Pocket 5 / Snapdragon 865, AYN Odin 2
# Mini and AYN Thor / Snapdragon 8 Gen 2)
# to behave like a weaker Snapdragon,
# to measure and optimize the ports (PS2 recomp, Xbox 360) against it.
# AYN Thor (KernelSU Next, su in the adb shell): HL_ROOT="su -c" HL_KEEP=1 tools/lab/device-profile.sh -s <thor> limit sd480p
#
#   tools/device-profile.sh [-s SERIAL] list
#   tools/device-profile.sh [-s SERIAL] status
#   tools/device-profile.sh [-s SERIAL] limit sd665|sd680|sd480p|sd695
#   tools/device-profile.sh [-s SERIAL] restore
#
# Needs root on the device. AYN and Retroid handhelds (no su): install the hlprofile helper app once
# (android/hlprofile, adb install hlprofile-debug.apk); it runs the commands through the firmware's PServerBinder.
# Devices with su: HL_ROOT="su -c" tools/device-profile.sh ...
# HL_STOP_PERF=1 with limit: also stops the vendor performance services that pin minimum frequencies
# (vendor.perfservice, perf-hal); restore starts them again, and a reboot does too.
# HL_KEEP=1 with limit: also starts a small root loop on the device (hl-keep.sh) that puts the CPU caps back whenever
# something resets them; restore stops it. Needed on the Odin 2 Mini after a reboot: the AYN pservice reapplies its
# standard mode (scaling_min/max_freq) every 10 s ("cpu_init boot_completed cpugpumode=0" in logcat, 2026-11-13).
# The loop checks five times a second, so a reset lasts at most about 0.2 s; it logs each fix to hl-keep.log.
# HL_PIN=1 with HL_KEEP=1: the loop also holds scaling_min_freq at the cap (fixed clocks). Without it the governor and
# the AYN pservice (performance_mode 0/1 rewrites the minimum every 10 s) move the clocks, and the same MGR PC build
# measured 17.2 fps (performance_mode 0) or 23.9 fps (right after performance_mode 1) in the same fight, 2026-11-19.
#
# The frequencies below are STARTING POINTS, not a calibration: tune them until Geekbench 6 single/multi on the
# limited device matches the target chip's public average (±5%), then write the final numbers here, with the date.
set -euo pipefail

serial=""
if [ "${1:-}" = "-s" ]; then serial="$2"; shift 2; fi
cmd="${1:-status}"; target="${2:-}"
adb=(adb); [ -n "$serial" ] && adb=(adb -s "$serial")
root="${HL_ROOT:-}"   # empty: use the hlprofile app (PServerBinder)
remote=/data/local/tmp/hl-profile.sh

# The device is picked by ro.board.platform: kona = Retroid Pocket 5, kalama = AYN Odin 2 Mini.
# Fields (":"-separated): first CPU of the middle policy (4 on the RP5, 3 on the Odin), online CPUs, policy0 max kHz,
# middle policy max kHz, policy7 max kHz, GPU max Hz
profile() {
    case "$device" in
        rp5) profile_rp5 "$1" ;;
        odin2mini) profile_odin2mini "$1" ;;
        *) echo "no profiles for this device (ro.board.platform '$board'): kona (RP5) or kalama (Odin 2 Mini)" >&2; exit 1 ;;
    esac
}

# ---- Retroid Pocket 5 (kona / SD865) ---------------------------------------------------------------------------
# cpu0-3: Cortex-A55 (policy0, max 1804800) | cpu4-6: Cortex-A77 (policy4, max 2419200) | cpu7: Cortex-A77 prime
# (policy7, max 2841600) | GPU: Adreno 650, 305-670 MHz.
profile_rp5() {
    case "$1" in
        # 4x A73 2.0 + 4x A53 1.8: 4 A77 slowed down (an A77 does more per clock than an A73), 4 A55
        # 2026-10-25, 1st run (1.52/1.17/1.19 GHz): GB7 491/2021 vs 350/1310 for the 665 -> 2nd try below
        sd665)  echo "4:0-7:1171200:825600:844800:305000000" ;;
        # 4x A73 2.4 + 4x A53 1.9
        sd680)  echo "4:0-7:1612800:1382400:1401600:305000000" ;;
        # 2x A76 2.2 + 6x A55 1.9: 2 A77 (cpu4-5), cpu6-7 off; the device has 4 small cores, not 6
        sd480p) echo "4:0-5:1804800:1766400:0:305000000" ;;
        # 2x A78 2.2 + 6x A55 1.8
        sd695)  echo "4:0-5:1708800:2054400:0:400000000" ;;
        *) echo "unknown profile '$1' (sd665, sd680, sd480p, sd695)" >&2; exit 1 ;;
    esac
}

# ---- AYN Odin 2 Mini and AYN Thor (kalama / SD 8 Gen 2; the Thor's frequency tables are the Odin's, 2026-11-14) -------------------------------------------------
# cpu0-2: Cortex-A510 (policy0, max 2016000) | cpu3-4: Cortex-A715, cpu5-6: Cortex-A710 (policy3, max 2707200) |
# cpu7: Cortex-X3 (policy7, max 2956800) | GPU: Adreno 740, 124.8-680 MHz. core_ctl on cpu0, cpu3 and cpu7.
profile_odin2mini() {
    case "$1" in
        # 2x A76 2.2 + 6x A55 1.9: the 2 A715 (cpu3-4) as the A76, the A710 (cpu5-6) and the X3 off, the 3 A510 as
        # the A55; GPU at its lowest level. STARTING POINT 2026-10-27, not calibrated: the A715 does ~25% more per
        # clock than an A76 (2.2 / 1.25 = 1.79 GHz) and the A510 ~30% more than an A55 (1.9 / 1.3 = 1.46 GHz).
        # The A710 share policy3 with the A715, so they are switched off rather than slowed down.
        # GPU CALIBRATION 2026-11-19 20:13 (Lisbon), 3DMark Wild Life 2.7.5093 with this profile applied: 2468 and 2474
        # points (14.8 fps) at 124.8 MHz, with the GPU LLC slice on and off. Public reference for a real SD480+
        # (Adreno 619): Moto G53 5G median 982 (Wild Life Unlimited 973), benchmarks.ul.com. So this GPU is ~2.5x a
        # 619 even at its LOWEST level: frequency cannot calibrate it further. GPU-bound fps measured here must be read
        # as ~2.5x what a real 480+ gets (multiply by 0.40); CPU-bound results are unaffected.
        sd480p) echo "3:0-4:1459200:1785600:0:124800000" ;;
        *) echo "unknown profile '$1' for the Odin 2 Mini (sd480p)" >&2; exit 1 ;;
    esac
}

# ---- the part that runs on the device, as root -----------------------------------------------------------------
device_script() {
cat <<'EOF'
set -u
C=/sys/devices/system/cpu
G=/sys/class/kgsl/kgsl-3d0
SAVE=/data/local/tmp/hl-profile.orig
KEEP_PID=/data/local/tmp/hl-keep.pid
w() { # write a value, and say so when it did not stick
    [ -e "$1" ] || { echo "missing: $1"; return 0; }
    echo "$2" > "$1" 2>/dev/null || { echo "could not write $2 to $1"; return 0; }
    got=$(cat "$1" 2>/dev/null); [ "$got" = "$2" ] || echo "note: $1 = $got (wanted $2)"
}
in_list() { # in_list N "0-5": is cpu N inside the range?
    lo=${2%-*}; hi=${2#*-}; [ "$1" -ge "$lo" ] && [ "$1" -le "$hi" ]
}
LOG=/data/local/tmp/hl-profile.log
if [ "$1" = limit ] || [ "$1" = restore ]; then
    # the PServerBinder only hands back the first line of output: everything goes to a log the shell user can read
    exec > $LOG 2>&1
    chmod 644 $LOG
fi
case "$1" in
status)
    for n in 0 1 2 3 4 5 6 7; do
        printf "cpu%s online=%s cur=%s\n" $n "$(cat $C/cpu$n/online 2>/dev/null || echo 1)" "$(cat $C/cpu$n/cpufreq/scaling_cur_freq 2>/dev/null || echo -)"
    done
    for p in $C/cpufreq/policy*; do echo "$(basename $p) max=$(cat $p/scaling_max_freq 2>/dev/null || echo '?') min=$(cat $p/scaling_min_freq 2>/dev/null || echo '?')"; done
    echo "gpu $(cat $G/gpu_model 2>/dev/null) cur=$(cat $G/gpuclk) max=$(cat $G/max_gpuclk) busy=$(cat $G/gpu_busy_percentage 2>/dev/null)"
    echo "gpu temp=$(cat $G/temp 2>/dev/null)"
    for z in /sys/class/thermal/thermal_zone*; do t=$(cat $z/type 2>/dev/null); case "$t" in cpu*|gpu*) echo "$t $(cat $z/temp)";; esac; done 2>/dev/null | head -12
    [ -f $SAVE ] && echo "profile active (originals saved in $SAVE)" || echo "no profile active"
    ;;
limit)
    IFS=':' read MID ON P0 P4 P7 GPU <<PROF
$2
PROF
    if [ ! -f $SAVE ]; then
        { for p in $C/cpufreq/policy*; do echo "$p/scaling_max_freq $(cat $p/scaling_max_freq)"; echo "$p/scaling_min_freq $(cat $p/scaling_min_freq)"; done
          echo "$G/max_gpuclk $(cat $G/max_gpuclk)"
          [ -e $G/devfreq/max_freq ] && echo "$G/devfreq/max_freq $(cat $G/devfreq/max_freq)"
          for n in $MID 7; do [ -e $C/cpu$n/core_ctl/min_cpus ] && echo "$C/cpu$n/core_ctl/min_cpus $(cat $C/cpu$n/core_ctl/min_cpus)"; done
          [ -e /proc/sys/kernel/sched_boost ] && echo "/proc/sys/kernel/sched_boost $(cat /proc/sys/kernel/sched_boost)"
          for prm in cpu_min_freq cpu_max_freq; do
              [ -e /sys/module/msm_performance/parameters/$prm ] && for pair in $(cat /sys/module/msm_performance/parameters/$prm); do
                  echo "/sys/module/msm_performance/parameters/$prm $pair"; done
          done
        } > $SAVE
    fi
    # sched_boost (RP5 ships with 1): keeps the big cores boosted and puts a floor under their frequency
    [ -e /proc/sys/kernel/sched_boost ] && w /proc/sys/kernel/sched_boost 0
    # STOPPERF=1: also stop the vendor performance services that pin minimum frequencies (restore starts them again)
    if [ "${3:-0}" = 1 ]; then
        for svc in vendor.perfservice perf-hal-2-2 perf-hal-2-3 poweropt-service; do  # 2-3 and poweropt on the Odin 2 Mini (2026-11-13)
            [ "$(getprop init.svc.$svc)" = running ] && { stop $svc 2>/dev/null; echo "stopped $svc: $(getprop init.svc.$svc)"; echo "$svc" >> /data/local/tmp/hl-profile.stopped; }
        done
        sleep 1
    fi
    # core_ctl (Qualcomm) turns cores on and off by itself: pin it so it does not undo the profile
    for n in $MID 7; do [ -e $C/cpu$n/core_ctl/min_cpus ] && w $C/cpu$n/core_ctl/min_cpus 0; done
    for n in 1 2 3 4 5 6 7; do
        if in_list $n "$ON"; then w $C/cpu$n/online 1; else w $C/cpu$n/online 0; fi
    done
    # the device's performance mode pins scaling_min_freq high (policy7 min = max): the minimum goes down first,
    # or the kernel keeps the cores above the new maximum
    capf() { [ -e $C/cpufreq/$1 ] || return 0
             w $C/cpufreq/$1/scaling_min_freq "$(cat $C/cpufreq/$1/cpuinfo_min_freq)"
             w $C/cpufreq/$1/scaling_max_freq "$2"; }
    # Qualcomm's msm_performance: the perf HAL leaves a floor here (RP5: 1420800 on cpu0-3, 1862400 on cpu4-6), and the
    # kernel never lets scaling_max_freq go under it. One "cpu:freq" pair per write (a list of pairs is refused).
    M=/sys/module/msm_performance/parameters
    if [ -e $M/cpu_min_freq ]; then
        for n in 0 1 2 3 4 5 6 7; do
            if [ $n -lt $MID ]; then f=$P0; elif [ $n = 7 ]; then f=$P7; else f=$P4; fi
            [ "$f" = 0 ] && continue
            echo "$n:$(cat $C/cpu$n/cpufreq/cpuinfo_min_freq 2>/dev/null || echo 0)" > $M/cpu_min_freq 2>/dev/null
            echo "$n:$f" > $M/cpu_max_freq 2>/dev/null
        done
        echo "msm_performance min: $(cat $M/cpu_min_freq)"
        echo "msm_performance max: $(cat $M/cpu_max_freq)"
    fi
    capf policy0 "$P0"
    [ "$P4" != 0 ] && capf policy$MID "$P4"
    [ "$P7" != 0 ] && [ "$(cat $C/cpu7/online)" = 1 ] && capf policy7 "$P7"
    # GPU: by frequency, and by power level (the level whose frequency is the cap; level 0 is the fastest)
    w $G/max_gpuclk "$GPU"
    [ -e $G/devfreq/max_freq ] && w $G/devfreq/max_freq "$GPU"
    if [ -e $G/max_pwrlevel ] && [ -e $G/freq_table_mhz ]; then
        i=0; lvl=""
        for mhz in $(cat $G/freq_table_mhz); do [ $((mhz * 1000000)) -le "$GPU" ] && [ -z "$lvl" ] && lvl=$i; i=$((i + 1)); done
        [ -n "$lvl" ] && w $G/max_pwrlevel "$lvl"
    fi
    sleep 2
    echo "profile applied: online=$ON policy0=$P0 policy$MID=$P4 policy7=$P7 gpu=$GPU"
    echo "after 2 s (as root):"
    for p in $C/cpufreq/policy*; do [ -e $p/scaling_max_freq ] && echo "  $(basename $p) max=$(cat $p/scaling_max_freq) min=$(cat $p/scaling_min_freq) cur=$(cat $p/scaling_cur_freq)"; done
    echo "  gpu max=$(cat $G/max_gpuclk) cur=$(cat $G/gpuclk) max_pwrlevel=$(cat $G/max_pwrlevel 2>/dev/null)"
    echo "  sched_boost=$(cat /proc/sys/kernel/sched_boost 2>/dev/null)"
    [ -e $M/cpu_max_freq ] && echo "  msm_performance min: $(cat $M/cpu_min_freq)" && echo "  msm_performance max: $(cat $M/cpu_max_freq)"
    # KEEP=1: a loop that puts the CPU caps back when the firmware resets them (see HL_KEEP in the host header)
    if [ "${4:-0}" = 1 ]; then
        [ -f $KEEP_PID ] && kill "$(cat $KEEP_PID)" 2>/dev/null
        caps="policy0:$P0"
        [ "$P4" != 0 ] && caps="$caps policy$MID:$P4"
        [ "$P7" != 0 ] && [ "$(cat $C/cpu7/online)" = 1 ] && caps="$caps policy7:$P7"
        cat > /data/local/tmp/hl-keep.sh <<'KEEP'
# hl-keep.sh "<policy>:<kHz> ..." [pin]: puts each scaling_max_freq cap back (minimum first) when something resets it;
# pin=1 also holds scaling_min_freq at the cap
F=/sys/devices/system/cpu/cpufreq
n=0
while :; do
    for pair in $1; do
        p=${pair%:*}; want=${pair#*:}
        read cur < $F/$p/scaling_max_freq
        if [ "$cur" != "$want" ]; then
            read low < $F/$p/cpuinfo_min_freq
            echo "$low" > $F/$p/scaling_min_freq
            echo "$want" > $F/$p/scaling_max_freq
            n=$((n + 1)); echo "$(date +%T) $p max $cur -> $want (fix $n)" >> /data/local/tmp/hl-keep.log
        fi
        if [ "$2" = 1 ]; then
            read low < $F/$p/scaling_min_freq
            [ "$low" != "$want" ] && echo "$want" > $F/$p/scaling_min_freq
        fi
    done
    sleep 0.2
done
KEEP
        rm -f /data/local/tmp/hl-keep.log
        setsid nohup sh /data/local/tmp/hl-keep.sh "$caps" "${5:-0}" > /dev/null 2>&1 < /dev/null &
        echo $! > $KEEP_PID
        echo "keeper started (pid $(cat $KEEP_PID)): $caps pin=${5:-0}"
    fi
    ;;
restore)
    if [ -f $KEEP_PID ]; then
        kill "$(cat $KEEP_PID)" 2>/dev/null && echo "keeper stopped"
        rm -f $KEEP_PID /data/local/tmp/hl-keep.sh /data/local/tmp/hl-keep.log
    fi
    for n in 1 2 3 4 5 6 7; do w $C/cpu$n/online 1; done
    [ -e $G/max_pwrlevel ] && w $G/max_pwrlevel 0
    if [ -f /data/local/tmp/hl-profile.stopped ]; then
        for svc in $(cat /data/local/tmp/hl-profile.stopped); do start $svc 2>/dev/null; echo "started $svc: $(getprop init.svc.$svc)"; done
        rm -f /data/local/tmp/hl-profile.stopped
    fi
    if [ -f $SAVE ]; then
        # msm_performance first (its limits bound the rest), then the plain files
        while read f v; do case "$f" in */msm_performance/*) echo "$v" > "$f" 2>/dev/null ;; esac; done < $SAVE
        while read f v; do case "$f" in */msm_performance/*) ;; *) w "$f" "$v" ;; esac; done < $SAVE
        rm -f $SAVE
        echo "restored"
    else
        echo "all cores on; no saved values (no profile was active)"
    fi
    ;;
list)
    getprop ro.product.model; getprop ro.board.platform
    for p in $C/cpufreq/policy*; do echo "$p: cpus $(cat $p/related_cpus) | $(cat $p/scaling_available_frequencies)"; done
    echo "gpu $(cat $G/gpu_model 2>/dev/null): $(cat $G/gpu_available_frequencies)"
    ls -d $C/cpu*/core_ctl 2>/dev/null
    ;;
esac
EOF
}

board=$("${adb[@]}" shell getprop ro.board.platform 2>/dev/null | tr -d '\r')
case "$board" in kona) device=rp5 ;; kalama) device=odin2mini ;; *) device="" ;; esac

case "$cmd" in
    list|status|restore) arg="" ;;
    limit) [ -n "$target" ] || { echo "usage: $0 [-s SERIAL] limit sd665|sd680|sd480p|sd695" >&2; exit 1; }
           arg="$(profile "$target")" ;;
    *) echo "usage: $0 [-s SERIAL] list|status|limit <profile>|restore" >&2; exit 1 ;;
esac

# as root: through su (HL_ROOT), or through the hlprofile app and the PServerBinder. The script is pushed to
# /data/local/tmp first and the root side only gets "sh <file> <args>": the PServerBinder dies (DeadObjectException)
# on long commands, so the command has to stay short.
as_root() { # as_root <args for the device script>
    tmp=$(mktemp); device_script > "$tmp"
    "${adb[@]}" push -q "$tmp" "$remote" >/dev/null; rm -f "$tmp"
    if [ -n "$root" ]; then
        "${adb[@]}" shell "$root 'sh $remote $*'" 2>&1 | tr -d '\r'
    else
        "${adb[@]}" shell "am broadcast --include-stopped-packages -n com.dfdx047.hlprofile/.Run --es cmd 'sh $remote $*'" 2>&1 \
            | tr -d '\r' | awk '
                /^Broadcast completed:/ { on = 1; sub(/^Broadcast completed: result=[-0-9]+, data="/, ""); }
                on { print }' | sed '$ s/"$//'
    fi
}

uid_check() {
    if [ -n "$root" ]; then
        uid=$("${adb[@]}" shell "$root 'id -u'" 2>/dev/null | tr -d '\r')
    else
        res=$("${adb[@]}" shell "am broadcast --include-stopped-packages -n com.dfdx047.hlprofile/.Run --es cmd 'id -u'" 2>&1 | tr -d '\r' | grep '^Broadcast completed' || true)
        uid=$(echo "$res" | sed -n 's/.*data="\([0-9]*\).*/\1/p')
        [ -n "$res" ] || { echo "no answer from the hlprofile app: install it (adb install hlprofile-debug.apk)" >&2; exit 1; }
        case "$res" in *result=0,*) ;; *) echo "hlprofile: $res" >&2; exit 1 ;; esac
    fi
    [ "$uid" = 0 ] || { echo "not root (uid '$uid'). AYN/Retroid: install the hlprofile app and leave HL_ROOT empty; other devices: HL_ROOT=\"su -c\"" >&2; exit 1; }
}

if [ "$cmd" = limit ] || [ "$cmd" = restore ]; then
    uid_check
    as_root "$cmd" "${arg:-x}" "${HL_STOP_PERF:-0}" "${HL_KEEP:-0}" "${HL_PIN:-0}" >/dev/null
    "${adb[@]}" shell "cat /data/local/tmp/hl-profile.log" 2>/dev/null | tr -d '\r' || echo "(no log from the device)"
else
    # list and status read the sysfs as the shell user: no root needed
    tmp=$(mktemp); device_script > "$tmp"
    "${adb[@]}" push -q "$tmp" "$remote" >/dev/null; rm -f "$tmp"
    "${adb[@]}" shell "sh $remote $cmd"
fi
