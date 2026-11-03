# Respostas às issues, 03/11 (rascunho, não publicadas)

## #24 (Mali-G615, céu preto, crash na escada depois do RAY)

Thanks for the log, it had the crash. After 10 minutes of play, the game crashed inside the Mali driver's shader compiler (`libGLES_mali.so`, in `cmpbe_v2_compile_multiple_shaders`) while Native was building a new pipeline. #17 has the same crash at the same address in the driver, twice, on v1.1. So one specific shader trips a bug in the driver; it isn't memory or heat.

The next test build (1.3.0-beta1) records which pipeline is being compiled. If the driver crashes, the next launch logs that pipeline and compiles one at a time. If the same pipeline crashes it a second time, it is skipped. You may crash once or twice more at the same spot, and then it should go through. Please send the **Export logs** zip after that: it will name the shader, so I can fix it properly.

**Black sky:** still no cause. Nothing fails in the log.
**Blade Mode at 15-18 fps:** same as #35. In your screenshot the GPU takes 14 ms and the frame 37 ms, so it's CPU work, not graphics. The beta gives the system performance hint more game threads. I can't promise that fixes it.

## #27 (PowerVR BXM-8-256)

Thanks, the two zips had what I needed. In them, Native crashed 14 times, 0.4 to 8 seconds after the first frames. Every crash is inside the GPU driver (`vulkan.mtk.so`, memory corruption caught by Scudo) while several threads were building pipelines at the same time. The crash point inside the driver changes every time. That points at the driver not handling parallel pipeline creation, not at a bad shader.

The next test build (1.3.0-beta1) builds pipelines one at a time on PowerVR. The first load may take longer. If 360p still crashes on it, please send a log: I only found this one crash in your zips.

The green spots on Native and the white screen on Emulated: not looked into yet.

## #41 (Adreno 710, quedas para 0 fps no Emulado)

Thanks, good to hear most of it works now. Your log shows the cause of the drops. On Emulated, new effects (shaders) are compiled while you play. That compile was meant to run in the background, but every frame submission waited for all pending compiles. So each new effect froze the game: 308 compiles in 15 minutes of level 1, and the freeze the watchdog caught was inside the Adreno shader compiler.

The next test build (1.3.0-beta1) no longer waits: a new effect may be missing for a few frames instead of the game stopping. Compiled effects stay in the cache, so each one costs this only once.

The 360p spots are tracked in #9.

## #35 (Mali-G615, desempenho)

A follow-up with what I found. I compared your log with my test devices. On your G615 the game's threads use about 94 ms of CPU per frame together, against about 36 ms on a Snapdragon 865 for the same work. My guess, which I haven't measured, is that the game's worker threads run on the small cores.

The next test build (1.3.0-beta1) gives the system performance hint up to 8 game threads (it was 4, picked once in the menus) and picks them again every 30 s, so the combat threads are included. If the fps changes, please send a log either way.

**Black sky:** still no cause, same as #24.
**PanVK:** it's in the Drivers tab on Mali in the beta. I haven't tested it.
