#!/bin/bash
# lab-measure.sh <serial> <outdir> <secs> [package] — measures the running PC lab game on the device:
# GPU busy % and clock every 2 s (kgsl, root), CPU % per thread over the window. fps comes from the run log
# ("Present: frame N, X fps over 5.0 s"). Writes <outdir>/measure.txt.
set -e
O=$1; R=$2; S=${3:-30}; P=${4:-com.dfdx047.zandatsu.pclab}
mkdir -p "$R"
adb -s "$O" shell "p=\$(pidof $P); for t in /proc/\$p/task/*; do echo \"\$(basename \$t) \$(cut -d' ' -f14,15,39 \$t/stat) \$(cat \$t/comm | tr ' ' _)\"; done" > "$R/s1"
: > "$R/gpu"; end=$(( $(date +%s) + S ))
while [ "$(date +%s)" -lt $end ]; do adb -s "$O" shell "su -c 'cat /sys/class/kgsl/kgsl-3d0/gpu_busy_percentage /sys/class/kgsl/kgsl-3d0/gpuclk'" | tr '\n' ' ' >> "$R/gpu"; echo >> "$R/gpu"; sleep 2; done
adb -s "$O" shell "p=\$(pidof $P); for t in /proc/\$p/task/*; do echo \"\$(basename \$t) \$(cut -d' ' -f14,15,39 \$t/stat) \$(cat \$t/comm | tr ' ' _)\"; done" > "$R/s2"
python3 - "$R" "$S" <<'PY'
import sys,re
R,S=sys.argv[1],float(sys.argv[2])
def parse(f):
    d={}
    for l in open(f):
        p=l.split()
        if len(p)>=5: d[p[0]]=(int(p[1])+int(p[2]),p[3],p[4])
    return d
a,b=parse(R+'/s1'),parse(R+'/s2')
rows=sorted(((b[k][0]-a[k][0])/S,k,b[k][1],b[k][2]) for k in b if k in a)[::-1]
busy=[];clk=[]
for l in open(R+'/gpu'):
    m=re.findall(r'(\d+)',l)
    if len(m)>=2: busy.append(int(m[0])); clk.append(int(m[-1]))
out=[f'window {S:.0f} s', f'CPU total {sum(r[0] for r in rows):.0f}%']
out+= [f'  {r[3]} (tid {r[1]}) {r[0]:.0f}% cpu{r[2]}' for r in rows[:6]]
if busy: out.append(f'GPU busy avg {sum(busy)/len(busy):.0f}% (min {min(busy)} max {max(busy)}), clock {max(clk)/1e6:.1f} MHz')
open(R+'/measure.txt','w').write('\n'.join(out)+'\n'); print('\n'.join(out))
PY
rm -f "$R/s1" "$R/s2"
