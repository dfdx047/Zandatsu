# R8 no APK de release (12/11): encolhe as bibliotecas (Compose, Material, os icones estendidos) e deixa o codigo do app
# e do SDL inteiros. Os dois sao chamados por nome a partir do codigo nativo (JNI: Java_com_dfdx047_zandatsu_*, as
# classes org.libsdl.app que o libSDL3.so procura) e o app e pequeno: nao vale o risco de renomear.
-keep class com.dfdx047.zandatsu.** { *; }
-keep class org.libsdl.app.** { *; }
-keepclasseswithmembernames,includedescriptorclasses class * { native <methods>; }
-dontobfuscate
