# Zandatsu PC: R8 encolhe as bibliotecas e deixa o codigo da app inteiro. O carregador nativo (libmgrlab.so) regista
# os metodos nativos do PcGameActivity por nome (RegisterNatives: nativeKey, nativeMotion, nativeTouchPad,
# nativePointer, nativePause).
-keep class com.dfdx047.zandatsu.pc.** { *; }
-keepclasseswithmembernames,includedescriptorclasses class * { native <methods>; }
-dontobfuscate
