#include "poll.h"

#ifdef ASYNC_IO
#include "polldef.h"
extern bool jpLayout;
extern int exited;

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

bool wndForeground = false;

bool currentKeyboardState[0xFF] = { false };
int keyboardCount[0xFF] = { 0 };
int keyboardDiff[0xFF] = { 0 };
bool currentControllerButtonsState[SDL_CONTROLLER_BUTTON_MAX] = { false };
int controllerCount[SDL_CONTROLLER_BUTTON_MAX] = { 0 };
int controllerDiff[SDL_CONTROLLER_BUTTON_MAX] = { 0 };
char currentMouseWheelDirection = 0;
int mouseWheelCount[2] = { 0 };
int mouseWheelDiff[2] = { 0 };

int maxCount = 1;

SDLAxisState currentControllerAxisState;

SDL_Window *window;
SDL_GameController *controllers[255];

std::thread updatePollThread;

void
SetKeyboardButtons () {
    ConfigKeyboardButtonsCount = jpLayout ? std::size (ConfigKeyboardButtons_JP) : std::size (ConfigKeyboardButtons_US);
    ConfigKeyboardButtons      = static_cast<KeyCodePair *> (malloc (ConfigKeyboardButtonsCount * sizeof (KeyCodePair)));
    memcpy (ConfigKeyboardButtons, jpLayout ? ConfigKeyboardButtons_JP : ConfigKeyboardButtons_US, ConfigKeyboardButtonsCount * sizeof (KeyCodePair));
}

void
SetConfigValue (const toml_table_t *table, const char *key, Keybindings *key_bind, bool *usePoll) {
    const toml_array_t *array = toml_array_in (table, key);
    if (!array) {
        LogMessage (LogLevel::WARN, std::string (key) + ": Cannot find array");
        return;
    }

    memset (key_bind, 0, sizeof (*key_bind));
    for (size_t i = 0; i < std::size (key_bind->buttons); i++)
        key_bind->buttons[i] = SDL_CONTROLLER_BUTTON_INVALID;

    for (int idx = 0;; idx++) {
        const auto [ok, u] = toml_string_at (array, idx);
        if (!ok) break;
        const ConfigValue value = StringToConfigEnum (u.s);
        free (u.s);

        switch (value.type) {
        case keycode: {
            for (int i = 0; i < std::size (key_bind->keycodes); i++) {
                if (key_bind->keycodes[i] == 0) {
                    key_bind->keycodes[i] = value.keycode;
                    break;
                }
            }
            break;
        }
        case button: {
            *usePoll = true;
            for (int i = 0; i < std::size (key_bind->buttons); i++) {
                if (key_bind->buttons[i] == SDL_CONTROLLER_BUTTON_INVALID) {
                    key_bind->buttons[i] = value.button;
                    break;
                }
            }
            break;
        }
        case axis: {
            *usePoll = true;
            for (int i = 0; i < std::size (key_bind->axis); i++) {
                if (key_bind->axis[i] == 0) {
                    key_bind->axis[i] = value.axis;
                    break;
                }
            }
        }
        case scroll: {
            for (int i = 0; i < std::size (key_bind->scroll); i++) {
                if (key_bind->scroll[i] == 0) {
                    key_bind->scroll[i] = value.scroll;
                    break;
                }
            }
            break;
        }
        default: break;
        }
    }
}

// 钩子句柄
HHOOK keyboardHook;
HHOOK mouseHook;

// 钩子回调函数
LRESULT CALLBACK InputProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        switch (wParam) {
            case WM_KEYDOWN: if (wndForeground) {
                KBDLLHOOKSTRUCT* pKeyboard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
                DWORD keyCode = pKeyboard->vkCode;
                if (!currentKeyboardState[keyCode]) keyboardCount[keyCode] = MIN (keyboardCount[keyCode] + 1, maxCount);
                currentKeyboardState[keyCode] = true;
            } break;
            // keyup will accepted even if you're not focus in this window
            case WM_KEYUP: {
                KBDLLHOOKSTRUCT* pKeyboard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
                DWORD keyCode = pKeyboard->vkCode;
                currentKeyboardState[keyCode] = false;
            } break;
            case WM_MOUSEWHEEL: if (wndForeground) {
                MSLLHOOKSTRUCT *pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
                int wheelDelta = GET_WHEEL_DELTA_WPARAM(pMouseStruct->mouseData);
                if (wheelDelta > 0) {
                        mouseWheelCount[0] = MIN (mouseWheelCount[0] + wheelDelta, maxCount);
                        currentMouseState.ScrolledUp = true;
                    } else if (wheelDelta < 0) {
                        mouseWheelCount[1] = MIN (mouseWheelCount[1] - wheelDelta, maxCount);
                        currentMouseState.ScrolledDown = true;
                    }
            } break;
        }
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

void
InitializeKeyboard () {
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, InputProc, nullptr, 0);
    if (keyboardHook == nullptr) LogMessage (LogLevel::ERROR, "Failed to install keyboard hook!\n");
    mouseHook = SetWindowsHookEx(WH_MOUSE_LL, InputProc, nullptr, 0);
    if (maxCount > 1) LogMessage (LogLevel::ERROR, "CHEATING MODE! max count is set to {}", maxCount);
    atexit (DisposeKeyboard);
}

void
DisposeKeyboard () {
    if (keyboardHook) UnhookWindowsHookEx(keyboardHook);
    if (mouseHook) UnhookWindowsHookEx(mouseHook);
}

// void
// UpdateLoop () {
//     LogMessage (LogLevel::DEBUG, "Enter Controller UpdateLoop");
    
//     while (!exited) {
        
//     }
// }

bool
InitializePoll (HWND windowHandle) {
    LogMessage (LogLevel::DEBUG, "InitializePoll");
    bool hasRumble = true;
    wndForeground = windowHandle == GetForegroundWindow ();
    SDL_SetMainReady ();

    SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
    SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");

    if (SDL_Init (SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_VIDEO) != 0) {
        if (SDL_Init (SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_VIDEO) == 0) {
            hasRumble = false;
        } else {
            LogMessage (LogLevel::ERROR,
                        std::string ("SDL_Init (SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_VIDEO): ")
                            + SDL_GetError ());
            return false;
        }
    }

    auto configPath = std::filesystem::current_path () / "gamecontrollerdb.txt";
    if (SDL_GameControllerAddMappingsFromFile (configPath.string ().c_str ()) == -1)
        LogMessage (LogLevel::ERROR, "Cannot read gamecontrollerdb.txt");
    SDL_GameControllerEventState (SDL_ENABLE);
    SDL_JoystickEventState (SDL_ENABLE);

    for (int i = 0; i < SDL_NumJoysticks (); i++) {
        if (!SDL_IsGameController (i)) continue;

        SDL_GameController *controller = SDL_GameControllerOpen (i);
        if (!controller) {
            LogMessage (LogLevel::WARN, std::string ("Could not open gamecontroller ") + SDL_GameControllerNameForIndex (i) + ": " + SDL_GetError ());
            continue;
        }

        LogMessage (LogLevel::DEBUG, "Init Controller connected!");
        controllers[i] = controller;
    }

    window = SDL_CreateWindowFrom (windowHandle);
    if (window == NULL) LogMessage (LogLevel::ERROR, std::string ("SDL_CreateWindowFrom (windowHandle): ") + SDL_GetError ());

    atexit (DisposePoll);

    // updatePollThread = std::thread (UpdateLoop);
    // updatePollThread.detach ();

    return hasRumble;
}

bool
CheckForegroundWindow (HWND processWindow) {
    if (processWindow == nullptr) return false;
    if (wndForeground ^ (processWindow == GetForegroundWindow ())) {
        wndForeground = !wndForeground;
        LogMessage (LogLevel::DEBUG, "window focus={}", wndForeground);
    }
    return wndForeground;
}

void
UpdatePoll (HWND windowHandle, bool useController) {
    if (!CheckForegroundWindow (windowHandle)) return; 

    currentMouseState.ScrolledUp   = false;
    currentMouseState.ScrolledDown = false;
    for (int i=0; i<255; i++) keyboardCount[i] -= (bool)keyboardDiff[i]; 
    for (int i=0; i<SDL_CONTROLLER_BUTTON_MAX; i++) controllerCount[i] -= (bool)controllerDiff[i];
    for (int i=0; i<2; i++) if (mouseWheelCount[i] > 0) mouseWheelCount[i] -= (bool)mouseWheelDiff[i];
    std::fill_n (keyboardDiff, 0xFF, 0);
    std::fill_n (controllerDiff, SDL_CONTROLLER_BUTTON_MAX, 0);
    std::fill_n (mouseWheelDiff, 2, 0);

    // GetCursorPos (&currentMouseState.Position);
    // ScreenToClient (windowHandle, &currentMouseState.Position);
    if (useController) {
        SDL_Event event;
        SDL_GameController *controller;
        while (SDL_PollEvent (&event) != 0) {
            // LogMessage (LogLevel::DEBUG, "Receive SDL Event! type={}", (u64)event.type);
            switch (event.type) {
            case SDL_CONTROLLERDEVICEADDED:
                if (!SDL_IsGameController (event.cdevice.which)) break;
                controller = SDL_GameControllerOpen (event.cdevice.which);
                if (!controller) {
                    LogMessage (LogLevel::ERROR, std::string ("Could not open gamecontroller ") + SDL_GameControllerNameForIndex (event.cdevice.which)
                                                    + ": " + SDL_GetError ());
                    continue;
                }
                LogMessage (LogLevel::DEBUG, "Controller connected!");
                controllers[event.cdevice.which] = controller;
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                LogMessage (LogLevel::DEBUG, "Controller removed!");
                if (!SDL_IsGameController (event.cdevice.which)) break;
                SDL_GameControllerClose (controllers[event.cdevice.which]);
                break;
            // case SDL_MOUSEWHEEL:
            //     if (wndForeground) {
            //         // LogMessage (LogLevel::DEBUG, "Mouse Wheel moves {}", event.wheel.y);
            //         if (event.wheel.y > 0) {
            //             mouseWheelCount[0] = MIN (mouseWheelCount[0] + event.wheel.y, maxCount);
            //             // mouseWheelCount[1] = 0;
            //             currentMouseState.ScrolledUp = true;
            //         } else if (event.wheel.y < 0) {
            //             mouseWheelCount[1] = MIN (mouseWheelCount[1] - event.wheel.y, maxCount);
            //             // mouseWheelCount[0] = 0;
            //             currentMouseState.ScrolledDown = true;
            //         }
            //     }
            //     break;
            case SDL_CONTROLLERBUTTONUP: 
                // keyup will accepted even if you're not focus in this window
                currentControllerButtonsState[event.cbutton.button] = false; break;
            case SDL_CONTROLLERBUTTONDOWN: 
                if (wndForeground) {
                    if (!currentControllerButtonsState[event.cbutton.button]) {
                        controllerCount[event.cbutton.button] = MIN (controllerCount[event.cbutton.button] + 1, maxCount);
                    }
                    currentControllerButtonsState[event.cbutton.button] = true; 
                } else LogMessage (LogLevel::WARN, "Controller button pressed but window not focused!");
                break;
            case SDL_CONTROLLERAXISMOTION:
                float sensorValue = event.caxis.value == 0 ? 0 : event.caxis.value / (event.caxis.value > 0 ? 32767.0f : -32768.0f);
                if (wndForeground || sensorValue == 0) {
                    switch (event.caxis.axis) {
                    case SDL_CONTROLLER_AXIS_LEFTX: currentControllerAxisState.LeftRight = sensorValue; break;
                    case SDL_CONTROLLER_AXIS_LEFTY: currentControllerAxisState.LeftDown = sensorValue; break;
                    case SDL_CONTROLLER_AXIS_RIGHTX: currentControllerAxisState.RightRight = sensorValue; break;
                    case SDL_CONTROLLER_AXIS_RIGHTY: currentControllerAxisState.RightDown = sensorValue; break;
                    case SDL_CONTROLLER_AXIS_TRIGGERLEFT: currentControllerAxisState.LTriggerDown = sensorValue; break;
                    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: currentControllerAxisState.RTriggerDown = sensorValue; break;
                    }
                }
                break;
            }
        }
    }
}

void
DisposePoll () {
    SDL_DestroyWindow (window);
    SDL_Quit ();
}

ConfigValue
StringToConfigEnum (const char *value) {
    ConfigValue rval = {};
    for (size_t i = 0; i < ConfigKeyboardButtonsCount; ++i)
        if (!strcmp (value, ConfigKeyboardButtons[i].string)) {
            rval.type    = keycode;
            rval.keycode = ConfigKeyboardButtons[i].keycode;
            return rval;
        }
    for (const auto &[string, button_] : ConfigControllerButtons)
        if (!strcmp (value, string)) {
            rval.type   = button;
            rval.button = button_;
            return rval;
        }
    for (const auto &[string, axis_] : ConfigControllerAXIS)
        if (!strcmp (value, string)) {
            rval.type = axis;
            rval.axis = axis_;
            return rval;
        }
    for (auto &[string, scroll_] : ConfigMouseScroll)
        if (!strcmp (value, string)) {
            rval.type   = scroll;
            rval.scroll = scroll_;
            return rval;
        }

    LogMessage (LogLevel::ERROR, std::string (value) + ": Unknown value");
    return rval;
}

void
SetRumble (int left, int right, int length) {
    for (auto &controller : controllers) {
        if (!controller || !SDL_GameControllerHasRumble (controller)) continue;
        SDL_GameControllerRumble (controller, left, right, length);
    }
}

bool
KeyboardIsDown (const uint8_t keycode) {
    return currentKeyboardState[keycode];
}

int maxKeyboardCount = 1;
bool
KeyboardIsTapped (const uint8_t keycode) {
    if (keyboardCount[keycode] > 0) {
        if (maxKeyboardCount < keyboardCount[keycode]) {
            maxKeyboardCount = keyboardCount[keycode];
            LogMessage (LogLevel::DEBUG, "MAX KEYBOARD COLLECTED {} {}", keycode, maxKeyboardCount);
        }
        keyboardDiff[keycode] = 1;
        return true;
    } return false;
}

bool
GetMouseScrollUp () {
    return currentMouseState.ScrolledUp;
}

bool
GetMouseScrollDown () {
    return currentMouseState.ScrolledDown;
}

bool
GetMouseScrollIsDown (const Scroll scroll) {
    if (scroll == MOUSE_SCROLL_UP) return GetMouseScrollUp ();
    else return GetMouseScrollDown ();
}

int maxWheelCount = 1;
bool
GetMouseScrollIsTapped (const Scroll scroll) {
    if (scroll == MOUSE_SCROLL_INVALID) return false;
    if (mouseWheelCount[scroll - 1] > 0) {
        if (maxWheelCount < mouseWheelCount[scroll - 1]) {
            maxWheelCount = mouseWheelCount[scroll - 1];
            LogMessage (LogLevel::DEBUG, "MAX WHEEL COLLECTED {} {}", (int)scroll, maxWheelCount);
        }
        mouseWheelDiff[scroll - 1] = 1;
        return true;
    } return false;
}

bool
ControllerButtonIsDown (const SDL_GameControllerButton button) {
    return currentControllerButtonsState[button];
}

int maxButtonCount = 1;
bool
ControllerButtonIsTapped (const SDL_GameControllerButton button) {
    if (controllerCount[button] > 0) {
        if (maxButtonCount < controllerCount[button]) {
            maxButtonCount = controllerCount[button];
            LogMessage (LogLevel::DEBUG, "MAX BUTTON COLLECTED {} {}", (int)button, maxButtonCount);
        }
        controllerDiff[button] = 1;
        return true;
    } return false;
}

float
ControllerAxisIsDown (const SDLAxis axis) {
    switch (axis) {
    case SDL_AXIS_LEFT_LEFT: return currentControllerAxisState.LeftLeft;
    case SDL_AXIS_LEFT_RIGHT: return currentControllerAxisState.LeftRight;
    case SDL_AXIS_LEFT_UP: return currentControllerAxisState.LeftUp;
    case SDL_AXIS_LEFT_DOWN: return currentControllerAxisState.LeftDown;
    case SDL_AXIS_RIGHT_LEFT: return currentControllerAxisState.RightLeft;
    case SDL_AXIS_RIGHT_RIGHT: return currentControllerAxisState.RightRight;
    case SDL_AXIS_RIGHT_UP: return currentControllerAxisState.RightUp;
    case SDL_AXIS_RIGHT_DOWN: return currentControllerAxisState.RightDown;
    case SDL_AXIS_LTRIGGER_DOWN: return currentControllerAxisState.LTriggerDown;
    case SDL_AXIS_RTRIGGER_DOWN: return currentControllerAxisState.RTriggerDown;
    default: return false;
    }
}

bool
ControllerAxisIsTapped (const SDLAxis axis) {
    return ControllerAxisIsDown (axis);
}

bool
IsButtonTapped (const Keybindings &bindings) {

    for (size_t i = 0; i < ConfigKeyboardButtonsCount; i++) {
        if (bindings.keycodes[i] == 0) continue;
        if (KeyboardIsTapped (bindings.keycodes[i])) return true;
    }
    for (size_t i = 0; i < std::size (ConfigControllerButtons); i++) {
        if (bindings.buttons[i] == SDL_CONTROLLER_BUTTON_INVALID) continue;
        if (ControllerButtonIsTapped (bindings.buttons[i])) return true;
    }
    for (size_t i = 0; i < std::size (ConfigControllerAXIS); i++) {
        if (bindings.axis[i] == 0) continue;
        if (float val = ControllerAxisIsTapped (bindings.axis[i]); val > 0) return true; 
    }
    for (size_t i = 0; i < std::size (ConfigMouseScroll); i++) {
        if (bindings.scroll[i] == 0) continue;
        if (GetMouseScrollIsTapped (bindings.scroll[i])) return true;
    }
    return false;
    // return GetInternalButtonState (bindings).Tapped;
}

float
IsButtonDown (const Keybindings &bindings) {
     for (size_t i = 0; i < ConfigKeyboardButtonsCount; i++) {
        if (bindings.keycodes[i] == 0) continue;
        if (KeyboardIsDown (bindings.keycodes[i])) return 1.0f;
    }
    for (size_t i = 0; i < std::size (ConfigControllerButtons); i++) {
        if (bindings.buttons[i] == SDL_CONTROLLER_BUTTON_INVALID) continue;
        if (ControllerButtonIsDown (bindings.buttons[i])) return 1.0f;
    }
    for (size_t i = 0; i < std::size (ConfigControllerAXIS); i++) {
        if (bindings.axis[i] == 0) continue;
        if (float val = ControllerAxisIsDown (bindings.axis[i]); val > 0) return val;
    }
    for (size_t i = 0; i < std::size (ConfigMouseScroll); i++) {
        if (bindings.scroll[i] == 0) continue;
        if (GetMouseScrollIsDown (bindings.scroll[i])) return 1.0f;
    }
    return 0.0f;
    // return GetInternalButtonState (bindings).Down;
}
#endif