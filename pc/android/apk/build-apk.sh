#!/bin/bash
# Build the lab APK (manifest + libmgrlab.so only). Output: build/pc/android/mgrpc-lab.apk
set -e
ROOT=$(cd "$(dirname "$0")/../../.." && pwd); SDK=${ANDROID_HOME:-$HOME/Android/Sdk}; BT=$SDK/build-tools/36.0.0
OUT=$ROOT/build/pc/android/apk; mkdir -p "$OUT/lib/arm64-v8a"
cp "$ROOT/build/pc/android/libmgrlab.so" "$OUT/lib/arm64-v8a/"
for h in hook_impl main_hook file_redirect_hook gsl_alloc_hook; do
  f=$(find "$ROOT/build/pc/android/_deps" -name "lib$h.so" | head -1); [ -n "$f" ] && cp "$f" "$OUT/lib/arm64-v8a/"
done
"$BT/aapt2" link -o "$OUT/base.apk" --manifest "$ROOT/pc/android/apk/AndroidManifest.xml" -I "$SDK/platforms/android-35/android.jar"
(cd "$OUT" && zip -q -r base.apk lib)
"$BT/zipalign" -f -p 16 "$OUT/base.apk" "$OUT/aligned.apk"
"$BT/apksigner" sign --ks "$HOME/.android/debug.keystore" --ks-pass pass:android --key-pass pass:android \
    --out "$ROOT/build/pc/android/mgrpc-lab.apk" "$OUT/aligned.apk"
unzip -l "$ROOT/build/pc/android/mgrpc-lab.apk" | tail -n +1 | grep -E 'lib/|Manifest|files'
