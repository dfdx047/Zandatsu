#pragma once
#include <string>
#include <vector>
// Maps the owner's PE data, binds imports, tests lifted code and attempts CRT
// entry until the first unsupported host service. Returns an honest status.
std::string connectGuestRuntime(const char* executable);
void requestGuestRuntimeStop();
void setGuestPaused(bool paused);
void setGuestDisplaySize(unsigned width,unsigned height);
void setGuestKey(unsigned scan,bool down);
void clearGuestInput();
bool guestDrivingControls();
// A message for the player that does not stop the game (shown by GameActivity); empty when none.
std::string takeGuestNotice();
// Physical gamepad seen by the game as a DirectInput joystick: axes X, Y, Z
// (triggers), Rx, Ry, Rz in -1..1; buttons A B X Y LB RB Back Start LS RS as bits.
void setGuestPad(const float axes[6],uint32_t buttons,int hatX,int hatY);
void setGuestPadConnected(bool connected);
// True while the game has the joystick acquired (then the pad stops emulating keys).
bool guestGamepadInUse();
// Launcher option; only before the guest boots.
void setGuestWidescreen(bool enabled);
void setGuestOnlineServer(const std::string& host);
/** Touch as mouse: x/y as a fraction of the game view; phase 0 down, 1 move, 2 up (left button). */
void setGuestPointer(float x,float y,int phase);
/** Posts WM_CHAR messages (the game's text fields read those); 8 is backspace. */
void typeGuestText(const std::vector<uint32_t>& characters);
void setGuestLanguage(const char* language);
void setGuestResolution(unsigned width,unsigned height);
void setGuestFrameLimit(unsigned framesPerSecond);
