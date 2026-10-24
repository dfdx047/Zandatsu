package org.libsdl.app;

import android.view.KeyEvent;

/**
 * Zandatsu (vindo do DBZ3-Android): comando virtual dos controles por toque.
 *
 * Regista no SDL um "joystick Android" que nao existe fisicamente e manda-lhe
 * botoes e eixos pelas mesmas funcoes JNI que o SDLControllerManager usa para
 * os comandos reais. Fica neste pacote porque essas funcoes nativas sao
 * package-private. O SDL cria sozinho o mapeamento de gamepad (A/B/X/Y,
 * ombros, start/back, D-pad, 2 analogicos e 2 gatilhos) a partir das mascaras,
 * e o driver SDL do runtime ve-o como um comando XInput normal.
 *
 * Eixos (ordem do SDL no Android): 0 LX, 1 LY, 2 RX, 3 RY, 4 LT, 5 RT.
 * Os gatilhos vao de -1 (solto) a 1 (premido): o SDL converte o eixo inteiro
 * (-32768..32767) para 0..32767 no gamepad.
 */
public final class SwordManPad {
    /** Id fora do intervalo dos InputDevice reais. */
    public static final int DEVICE_ID = 0x44460001;

    // Bits SDL_GAMEPAD_BUTTON_*: SOUTH..DPAD_RIGHT (0..14).
    private static final int BUTTON_MASK = 0x7FFF;
    // LX, LY, RX, RY, LT, RT.
    private static final int AXIS_MASK = 0x003F;

    private static boolean sAttached;

    private SwordManPad() {}

    public static synchronized boolean isAttached() {
        return sAttached;
    }

    /** Liga o comando virtual. Chamar depois do SDL arrancar (o jogo ja a correr). */
    public static synchronized boolean attach() {
        if (sAttached) {
            return true;
        }
        try {
            // Rumble do jogo -> vibrador do aparelho (ver SwordManHaptics).
            SwordManHaptics.install(SDL.getContext());
            // pid.codes (0x1209): VID/PID que nenhum driver HIDAPI do SDL reclama.
            // SDL 3.4: + acelerometro e giroscopio (o comando de toque nao os tem).
            SDLControllerManager.nativeAddJoystick(DEVICE_ID, "Zandatsu Touch",
                    "swordman-touch-pad", 0x1209, 0xDF03, BUTTON_MASK, 6, AXIS_MASK, 0,
                    true, false, false, false);
            sAttached = true;
            // Valores iniciais: analogicos ao centro e gatilhos soltos.
            for (int i = 0; i < 4; i++) {
                SDLControllerManager.onNativeJoy(DEVICE_ID, i, 0f);
            }
            SDLControllerManager.onNativeJoy(DEVICE_ID, 4, -1f);
            SDLControllerManager.onNativeJoy(DEVICE_ID, 5, -1f);
            return true;
        } catch (UnsatisfiedLinkError e) {
            return false;
        }
    }

    public static synchronized void detach() {
        if (!sAttached) {
            return;
        }
        try {
            SDLControllerManager.nativeRemoveJoystick(DEVICE_ID);
        } catch (UnsatisfiedLinkError ignored) {
        }
        sAttached = false;
    }

    public static void button(int keycode, boolean down) {
        if (!sAttached) {
            return;
        }
        if (down) {
            SDLControllerManager.onNativePadDown(DEVICE_ID, keycode, 0);
        } else {
            SDLControllerManager.onNativePadUp(DEVICE_ID, keycode, 0);
        }
    }

    public static void axis(int axis, float value) {
        if (!sAttached) {
            return;
        }
        SDLControllerManager.onNativeJoy(DEVICE_ID, axis, value);
    }

    // Codigos de tecla usados (KeyEvent) para cada botao.
    public static final int KEY_A = KeyEvent.KEYCODE_BUTTON_A;
    public static final int KEY_B = KeyEvent.KEYCODE_BUTTON_B;
    public static final int KEY_X = KeyEvent.KEYCODE_BUTTON_X;
    public static final int KEY_Y = KeyEvent.KEYCODE_BUTTON_Y;
    public static final int KEY_LB = KeyEvent.KEYCODE_BUTTON_L1;
    public static final int KEY_RB = KeyEvent.KEYCODE_BUTTON_R1;
    public static final int KEY_BACK = KeyEvent.KEYCODE_BUTTON_SELECT;
    public static final int KEY_START = KeyEvent.KEYCODE_BUTTON_START;
    public static final int KEY_L3 = KeyEvent.KEYCODE_BUTTON_THUMBL;
    public static final int KEY_R3 = KeyEvent.KEYCODE_BUTTON_THUMBR;
    public static final int KEY_UP = KeyEvent.KEYCODE_DPAD_UP;
    public static final int KEY_DOWN = KeyEvent.KEYCODE_DPAD_DOWN;
    public static final int KEY_LEFT = KeyEvent.KEYCODE_DPAD_LEFT;
    public static final int KEY_RIGHT = KeyEvent.KEYCODE_DPAD_RIGHT;
}
