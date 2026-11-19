#pragma once
#include <android/input.h>
#include <android/keycodes.h>
#include <android/log.h>
#include <array>
#include <cmath>
#include <cstdint>

// Raw pad state for the guest's DirectInput joystick (guest_input.inc): sticks
// and triggers in -1..1 / 0..1, buttons in the XInput-for-DirectInput order
// (A B X Y LB RB Back Start LS RS) and the d-pad as a hat.
struct GuestPadState {
    float leftX=0,leftY=0,rightX=0,rightY=0,leftTrigger=0,rightTrigger=0;
    uint32_t buttons=0;
    int hatX=0,hatY=0;
};

// Physical gamepads (Bluetooth or USB). The game reads them as a DirectInput
// joystick (GuestPadState). Until it acquires that joystick, and on screens that
// only take the keyboard, buttons, sticks, triggers and the hat are also folded
// into DirectInput scan codes, with a racing layout while driving and a menu
// layout elsewhere. Keys are recomputed from the whole pad state on every
// event, so a button and a stick that both mean "left" never release each other.
class GamepadInput {
public:
    enum Button : unsigned {A,B,X,Y,L1,R1,L2,R2,Start,Select,ThumbL,ThumbR,Up,Down,Left,Right,ButtonCount};
    template<class SetKey>
    bool key(const AInputEvent* event,bool driving,SetKey&& setKey){
        int32_t code=AKeyEvent_getKeyCode(event),action=AKeyEvent_getAction(event);
        int button=buttonFor(code);
        if(button<0){
            if(isGamepad(event)&&action==AKEY_EVENT_ACTION_DOWN&&AKeyEvent_getRepeatCount(event)==0)
                __android_log_print(ANDROID_LOG_INFO,"NFSU2","Gamepad key %d not mapped (device %d)",code,AInputEvent_getDeviceId(event));
            return false;
        }
        if(action!=AKEY_EVENT_ACTION_DOWN&&action!=AKEY_EVENT_ACTION_UP)return true;
        buttons_[button]=action==AKEY_EVENT_ACTION_DOWN;
        apply(driving,setKey);return true;
    }
    template<class SetKey>
    bool motion(const AInputEvent* event,bool driving,SetKey&& setKey){
        if(!(AInputEvent_getSource(event)&AINPUT_SOURCE_CLASS_JOYSTICK))return false;
        auto axis=[&](int32_t which){return AMotionEvent_getAxisValue(event,which,0);};
        stickX_=axis(AMOTION_EVENT_AXIS_X);stickY_=axis(AMOTION_EVENT_AXIS_Y);
        hatX_=axis(AMOTION_EVENT_AXIS_HAT_X);hatY_=axis(AMOTION_EVENT_AXIS_HAT_Y);
        // Android's standard right stick is Z/RZ.
        rightX_=axis(AMOTION_EVENT_AXIS_Z);rightY_=axis(AMOTION_EVENT_AXIS_RZ);
        // Pads report triggers as LTRIGGER/RTRIGGER, BRAKE/GAS or both.
        leftTrigger_=std::fmax(axis(AMOTION_EVENT_AXIS_LTRIGGER),axis(AMOTION_EVENT_AXIS_BRAKE));
        rightTrigger_=std::fmax(axis(AMOTION_EVENT_AXIS_RTRIGGER),axis(AMOTION_EVENT_AXIS_GAS));
        apply(driving,setKey);return true;
    }
    // The game switched between menus and driving, or started/stopped reading
    // the pad as a joystick: remap whatever is held.
    template<class SetKey> void refresh(bool driving,SetKey&& setKey){if(driving!=driving_||emulate_!=sentEmulation_)apply(driving,setKey);}
    template<class SetKey> void releaseAll(SetKey&& setKey){
        buttons_.fill(false);stickX_=stickY_=rightX_=rightY_=hatX_=hatY_=leftTrigger_=rightTrigger_=0;apply(driving_,setKey);
    }
    // false while the game reads the pad as a DirectInput joystick.
    void setKeyEmulation(bool emulate){emulate_=emulate;}
    GuestPadState padState()const{
        GuestPadState state;
        state.leftX=stickX_;state.leftY=stickY_;state.rightX=rightX_;state.rightY=rightY_;
        state.leftTrigger=std::fmax(leftTrigger_,buttons_[L2]?1.f:0.f);
        state.rightTrigger=std::fmax(rightTrigger_,buttons_[R2]?1.f:0.f);
        static constexpr Button order[]={A,B,X,Y,L1,R1,Select,Start,ThumbL,ThumbR};
        for(unsigned i=0;i<10;++i)if(buttons_[order[i]])state.buttons|=1u<<i;
        state.hatX=buttons_[Left]||hatX_<-.5f?-1:buttons_[Right]||hatX_>.5f?1:0;
        state.hatY=buttons_[Up]||hatY_<-.5f?-1:buttons_[Down]||hatY_>.5f?1:0;
        return state;
    }
    static bool isGamepad(const AInputEvent* event){
        int32_t source=AInputEvent_getSource(event);
        return (source&AINPUT_SOURCE_GAMEPAD)==AINPUT_SOURCE_GAMEPAD||(source&AINPUT_SOURCE_JOYSTICK)==AINPUT_SOURCE_JOYSTICK;
    }
private:
    std::array<bool,ButtonCount> buttons_{};
    float stickX_=0,stickY_=0,rightX_=0,rightY_=0,hatX_=0,hatY_=0,leftTrigger_=0,rightTrigger_=0;
    std::array<bool,256> sent_{};
    bool driving_=false,emulate_=true,sentEmulation_=true;

    static int buttonFor(int32_t code){
        switch(code){
        case AKEYCODE_BUTTON_A:case AKEYCODE_BUTTON_1:return A;
        case AKEYCODE_BUTTON_B:case AKEYCODE_BUTTON_2:return B;
        case AKEYCODE_BUTTON_X:case AKEYCODE_BUTTON_3:return X;
        case AKEYCODE_BUTTON_Y:case AKEYCODE_BUTTON_4:return Y;
        case AKEYCODE_BUTTON_L1:case AKEYCODE_BUTTON_5:return L1;
        case AKEYCODE_BUTTON_R1:case AKEYCODE_BUTTON_6:return R1;
        case AKEYCODE_BUTTON_L2:case AKEYCODE_BUTTON_7:return L2;
        case AKEYCODE_BUTTON_R2:case AKEYCODE_BUTTON_8:return R2;
        case AKEYCODE_BUTTON_SELECT:case AKEYCODE_BUTTON_9:case AKEYCODE_BACK:return Select;
        case AKEYCODE_BUTTON_START:case AKEYCODE_BUTTON_10:case AKEYCODE_MENU:return Start;
        case AKEYCODE_BUTTON_THUMBL:case AKEYCODE_BUTTON_11:return ThumbL;
        case AKEYCODE_BUTTON_THUMBR:case AKEYCODE_BUTTON_12:return ThumbR;
        case AKEYCODE_DPAD_UP:return Up;case AKEYCODE_DPAD_DOWN:return Down;
        case AKEYCODE_DPAD_LEFT:return Left;case AKEYCODE_DPAD_RIGHT:return Right;
        case AKEYCODE_DPAD_CENTER:return A;
        default:return -1;
        }
    }
    template<class SetKey> void apply(bool driving,SetKey& setKey){
        std::array<bool,256> want{};
        // Hysteresis keeps a stick resting near the threshold from chattering.
        auto past=[&](float value,unsigned scan){return value>(sent_[scan]?.25f:.4f);};
        bool left=buttons_[Left]||hatX_<-.5f||past(-stickX_,0xcb),right=buttons_[Right]||hatX_>.5f||past(stickX_,0xcd);
        bool up=buttons_[Up]||hatY_<-.5f,down=buttons_[Down]||hatY_>.5f;
        want[0xcb]=left;want[0xcd]=right;
        if(driving){
            // Xbox-style racing: RT gas, LT brake/reverse, A handbrake, B nitrous,
            // X camera, Y look back, RB/LB shift up/down, Start pause.
            want[0xc8]=up||buttons_[R2]||past(rightTrigger_,0xc8);
            want[0xd0]=down||buttons_[L2]||past(leftTrigger_,0xd0);
            want[0x39]=buttons_[A];want[0x1c]=buttons_[A];want[0x38]=buttons_[B];want[0x2e]=buttons_[X];want[0x30]=buttons_[Y];
            want[0x2a]=buttons_[R1];want[0x1d]=buttons_[L1];
            want[0x01]=buttons_[Start]||buttons_[Select];
        }else{
            want[0xc8]=up||past(-stickY_,0xc8);want[0xd0]=down||past(stickY_,0xd0);
            want[0x1c]=buttons_[A]||buttons_[Start];want[0x01]=buttons_[B]||buttons_[Select];
            want[0x39]=buttons_[X];want[0x2a]=buttons_[Y];
        }
        driving_=driving;sentEmulation_=emulate_;
        if(!emulate_)want.fill(false);
        for(unsigned scan=0;scan<256;++scan)if(want[scan]!=sent_[scan]){sent_[scan]=want[scan];setKey(scan,want[scan]);}
    }
};
