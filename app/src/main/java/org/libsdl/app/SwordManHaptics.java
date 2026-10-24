package org.libsdl.app;

import android.content.Context;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;

/**
 * DBZ3-Android / Zandatsu: vibracao do jogo (rumble) no motor do aparelho.
 *
 * O SDL so faz rumble em comandos que tenham vibrador proprio. O comando
 * embutido de handhelds (Odin, Retroid) e os comandos Bluetooth sem motor nao
 * tem, por isso a vibracao do jogo nao chegava a lado nenhum. O comando virtual
 * dos controles por toque (SwordManPad) anuncia rumble e fica sempre ligado ao
 * mesmo jogador; o rumble que o jogo manda para ele vai para o vibrador do
 * aparelho. Os outros pedidos seguem para o tratador original do SDL.
 */
final class SwordManHaptics extends SDLHapticHandler {
    private final SDLHapticHandler mBase;
    private final Vibrator mVibrator;

    private SwordManHaptics(SDLHapticHandler base, Context context) {
        mBase = base;
        Vibrator v;
        if (Build.VERSION.SDK_INT >= 31) {
            VibratorManager vm = (VibratorManager) context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
            v = vm != null ? vm.getDefaultVibrator() : null;
        } else {
            v = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
        }
        mVibrator = (v != null && v.hasVibrator()) ? v : null;
    }

    /** Instala por cima do tratador do SDL (depois do SDLControllerManager.initialize). */
    static synchronized void install(Context context) {
        if (SDLControllerManager.mHapticHandler instanceof SwordManHaptics) {
            return;
        }
        SDLHapticHandler base = SDLControllerManager.mHapticHandler;
        if (base == null) {
            base = Build.VERSION.SDK_INT >= 31 ? new SDLHapticHandler_API31() : new SDLHapticHandler_API26();
        }
        SDLControllerManager.mHapticHandler = new SwordManHaptics(base, context.getApplicationContext());
    }

    private void vibrate(float intensity, int length) {
        if (mVibrator == null) {
            return;
        }
        if (intensity <= 0.01f || length <= 0) {
            mVibrator.cancel();
            return;
        }
        int amp = Math.max(1, Math.min(255, Math.round(intensity * 255f)));
        try {
            mVibrator.vibrate(VibrationEffect.createOneShot(Math.min(length, 5000), amp));
        } catch (Exception ignored) {
        }
    }

    @Override
    void run(int device_id, float intensity, int length) {
        if (device_id == SwordManPad.DEVICE_ID) {
            vibrate(intensity, length);
        } else {
            mBase.run(device_id, intensity, length);
        }
    }

    @Override
    void rumble(int device_id, float low_frequency_intensity, float high_frequency_intensity, int length) {
        if (device_id == SwordManPad.DEVICE_ID) {
            vibrate(Math.max(low_frequency_intensity, high_frequency_intensity), length);
        } else {
            mBase.rumble(device_id, low_frequency_intensity, high_frequency_intensity, length);
        }
    }

    @Override
    void stop(int device_id) {
        if (device_id == SwordManPad.DEVICE_ID) {
            if (mVibrator != null) {
                mVibrator.cancel();
            }
        } else {
            mBase.stop(device_id);
        }
    }

    @Override
    synchronized void pollHapticDevices() {
        mBase.pollHapticDevices();
    }

    @Override
    synchronized protected SDLHaptic getHaptic(int device_id) {
        return mBase.getHaptic(device_id);
    }
}
