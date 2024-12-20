#include "poll.h"

#ifdef ASYNC_IO
#include "polldef.h"
#define SDL_MAIN_NOIMPL
#include <SDL3/SDL_main.h>

extern bool jpLayout;
extern int exited;

bool wndForeground = false;

bool currentKeyboardState[0xFF] = { false };
uint8_t keyboardCount[0xFF] = { 0 };
uint8_t keyboardDiff[0xFF] = { 0 };
bool keyboardClean = false;
bool currentControllerButtonsState[SDL_GAMEPAD_BUTTON_COUNT] = { false };
uint8_t controllerCount[SDL_GAMEPAD_BUTTON_COUNT] = { 0 };
uint8_t controllerDiff[SDL_GAMEPAD_BUTTON_COUNT] = { 0 };
char currentMouseWheelDirection = 0;
uint8_t mouseWheelCount[2] = { 0 };
uint8_t mouseWheelDiff[2] = { 0 };
SDLAxisState currentControllerAxisState;
uint8_t controllerAxisCount[SDL_AXIS_MAX] = { 0 };
uint8_t controllerAxisDiff[SDL_AXIS_MAX] = { 0 };

int maxCount = 2;

SDL_Window *window;
SDL_Gamepad *controllers[255];
bool flipped[255];

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
        key_bind->buttons[i] = SDL_GAMEPAD_BUTTON_INVALID;

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
                if (key_bind->buttons[i] == SDL_GAMEPAD_BUTTON_INVALID) {
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
                auto* pKeyboard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
                DWORD keyCode = pKeyboard->vkCode;
                if (!currentKeyboardState[keyCode]) keyboardCount[keyCode] = std::min (keyboardCount[keyCode] + 1, maxCount);
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
                        mouseWheelCount[0] = std::min (mouseWheelCount[0] + wheelDelta, maxCount);
                        currentMouseState.ScrolledUp = true;
                    } else if (wheelDelta < 0) {
                        mouseWheelCount[1] = std::min (mouseWheelCount[1] - wheelDelta, maxCount);
                        currentMouseState.ScrolledDown = true;
                    }
            } break;
        default: break;
        }
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY: PostQuitMessage(0); break;
        default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

HWND CreateInvisibleWindow(HINSTANCE hInstance) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "InvisibleWindowClass";
    if (!RegisterClass(&wc)) return nullptr;
    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        "Invisible Window",
        0, 0, 0, 0, 0,
        NULL, NULL, hInstance, NULL
    );
    if (hwnd) SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    return hwnd;
}

void KeyboardMainLoop() {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    HWND hwnd = CreateInvisibleWindow(hInstance);
    if (!hwnd) LogMessage (LogLevel::ERROR, "Failed to create window!\n");
     keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, InputProc, nullptr, 0);
    if (keyboardHook == nullptr) LogMessage (LogLevel::ERROR, "Failed to install keyboard hook!\n");
    mouseHook = SetWindowsHookEx(WH_MOUSE_LL, InputProc, nullptr, 0);
    if (mouseHook == nullptr) LogMessage (LogLevel::ERROR, "Failed to install mouse hook!\n");
    if (maxCount > 2) LogMessage (LogLevel::ERROR, "CHEATING MODE! max count is set to {}", maxCount);
    LogMessage (LogLevel::WARN, "(experimental) Using Async IO!");
    MSG msg;
    while (GetMessage(&msg, hwnd, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (keyboardHook) UnhookWindowsHookEx(keyboardHook);
    if (mouseHook) UnhookWindowsHookEx(mouseHook);
}

void
InitializeKeyboard () {
    std::thread (KeyboardMainLoop).detach ();
    // atexit (DisposeKeyboard);
}

void
DisposeKeyboard () {
    // if (keyboardHook) UnhookWindowsHookEx(keyboardHook);
    // if (mouseHook) UnhookWindowsHookEx(mouseHook);
}

void
CheckFlipped (uint32_t which, SDL_Gamepad *gamepad) {
    if (SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_B) {
        flipped[which] = true;
    } else flipped[which] = false;
}

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

    if (!SDL_Init (SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS | SDL_INIT_VIDEO)) {
        if (SDL_Init (SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS | SDL_INIT_VIDEO)) {
            hasRumble = false;
        } else {
            LogMessage (LogLevel::ERROR, std::string ("SDL_Init (SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS | SDL_INIT_VIDEO): ") + SDL_GetError ());
            return false;
        }
    }

    auto configPath = std::filesystem::current_path () / "gamecontrollerdb.txt";
    if (SDL_AddGamepadMappingsFromFile (configPath.string ().c_str ()) == -1)
        LogMessage (LogLevel::ERROR, "Cannot read gamecontrollerdb.txt");
    SDL_SetGamepadEventsEnabled (true);
    SDL_SetJoystickEventsEnabled (true);

    int numJoysticks = 0;
    auto* joyStickIds = SDL_GetJoysticks(&numJoysticks);
    if (joyStickIds == nullptr) {
        LogMessage (LogLevel::ERROR, "Error getting joystick ids");
        exit(1);
    }
    for (int i = 0; i < numJoysticks; i++) {
        auto id = joyStickIds[i];
        if (!SDL_IsGamepad (id)) continue;
        SDL_Gamepad *controller = SDL_OpenGamepad (id);

        if (!controller) {
            LogMessage (LogLevel::WARN, std::string ("Could not open gamecontroller ") + SDL_GetGamepadNameForID (id) + ": " + SDL_GetError ());
            continue;
        }

        LogMessage (LogLevel::DEBUG, "Init Controller connected!");
        CheckFlipped (id, controller);
        controllers[id] = controller;
    }
    SDL_free (joyStickIds);

    SDL_PropertiesID props = SDL_CreateProperties();
    if(props == 0) {
        LogMessage (LogLevel::ERROR, "Unable to create SDL_Properties");
        exit(1);
    }
    SDL_SetPointerProperty (props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, windowHandle);
    window = SDL_CreateWindowWithProperties (props);
    if (window == NULL) LogMessage (LogLevel::ERROR, std::string ("SDL_CreateWindowFrom (windowHandle): ") + SDL_GetError ());

    atexit (DisposePoll);
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

uint8_t
CorrectButton (uint32_t which, uint8_t button) {
    if (!flipped[which]) return button;
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return SDL_GAMEPAD_BUTTON_EAST;
    case SDL_GAMEPAD_BUTTON_EAST: return SDL_GAMEPAD_BUTTON_SOUTH;
    case SDL_GAMEPAD_BUTTON_WEST: return SDL_GAMEPAD_BUTTON_NORTH;
    case SDL_GAMEPAD_BUTTON_NORTH: return SDL_GAMEPAD_BUTTON_WEST;
    default: return button;
    }
}

void
UpdatePoll (HWND windowHandle, bool useController) {
    if (!CheckForegroundWindow (windowHandle)) return; 

    currentMouseState.ScrolledUp   = false;
    currentMouseState.ScrolledDown = false;
    if (keyboardClean) {
        keyboardClean = false;
        for (int i=0; i<0xFF; i++) keyboardCount[i] -= (bool)keyboardDiff[i]; 
        memset (keyboardDiff, 0, 0XFF);
    }
    if (mouseWheelCount[0] > 0) mouseWheelCount[0] -= (bool)mouseWheelDiff[0];
    if (mouseWheelCount[1] > 0) mouseWheelCount[1] -= (bool)mouseWheelDiff[1];
    memset (mouseWheelDiff, 0, 2);

    // GetCursorPos (&currentMouseState.Position);
    // ScreenToClient (windowHandle, &currentMouseState.Position);
    if (useController) {
        for (int i=0; i<SDL_GAMEPAD_BUTTON_COUNT; i++) controllerCount[i] -= (bool)controllerDiff[i];
        for (int i=1; i<SDL_AXIS_MAX; i++) controllerAxisCount[i] -= (bool)controllerAxisDiff[i];
        memset (controllerDiff, 0, SDL_GAMEPAD_BUTTON_COUNT);
        memset (controllerAxisDiff, 0, SDL_AXIS_MAX);
        SDL_Event event;
        SDL_Gamepad *controller;
        while (SDL_PollEvent (&event) != 0) {
            switch (event.type) {
            case SDL_EVENT_GAMEPAD_ADDED:
                if (!SDL_IsGamepad (event.gdevice.which)) break;
                controller = SDL_OpenGamepad (event.gdevice.which);
                if (!controller) {
                    LogMessage (LogLevel::ERROR, "Could not open gamecontroller {}: {}", SDL_GetGamepadNameForID (event.gdevice.which), SDL_GetError ());
                    continue;
                }
                LogMessage (LogLevel::DEBUG, "Controller connected!");
                CheckFlipped (event.gdevice.which, controller);
                controllers[event.gdevice.which] = controller;
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                LogMessage (LogLevel::DEBUG, "Controller removed!");
                if (!SDL_IsGamepad (event.gdevice.which)) break;
                SDL_CloseGamepad (controllers[event.gdevice.which]);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_UP: {
                uint8_t button = CorrectButton (event.gbutton.which, event.gbutton.button);
                currentControllerButtonsState[button] = false;
            } break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN: 
                if (wndForeground) {
                    uint8_t button = CorrectButton (event.gbutton.which, event.gbutton.button);
                    LogMessage (LogLevel::DEBUG, "Controller {}: button {} pressed, remapped to {}", event.gbutton.which, event.gbutton.button, button);
                    if (!currentControllerButtonsState[button]) {
                        controllerCount[button] = std::min (controllerCount[button] + 1, maxCount);
                    }
                    currentControllerButtonsState[button] = true;
                } else LogMessage (LogLevel::DEBUG, "Controller button pressed but window not focused!");
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                float sensorValue = event.gaxis.value == 0 ? 0 : event.gaxis.value / (event.gaxis.value > 0 ? 32767.0f : -32768.0f);
                if (wndForeground || sensorValue == 0) {
                    switch (event.gaxis.axis) {
                    case SDL_GAMEPAD_AXIS_LEFTX: if (sensorValue < 0) {
                        currentControllerAxisState.LeftRight = 0.0f;
                        if (currentControllerAxisState.LeftLeft == 0.0f) {
                            controllerAxisCount[SDL_AXIS_LEFT_LEFT] = std::min (controllerAxisCount[SDL_AXIS_LEFT_LEFT] + 1, maxCount);
                        }
                        currentControllerAxisState.LeftLeft = -sensorValue;
                    } else {
                        currentControllerAxisState.LeftLeft = 0.0f;
                        if (currentControllerAxisState.LeftRight == 0.0f) {
                            controllerAxisCount[SDL_AXIS_LEFT_RIGHT] = std::min (controllerAxisCount[SDL_AXIS_LEFT_RIGHT] + 1, maxCount);
                        }
                        currentControllerAxisState.LeftRight = sensorValue;
                    } break;
                    case SDL_GAMEPAD_AXIS_LEFTY: if (sensorValue < 0) {
                        currentControllerAxisState.LeftDown = 0.0f;
                        if (currentControllerAxisState.LeftUp == 0.0f) {
                            controllerAxisCount[SDL_AXIS_LEFT_UP] = std::min (controllerAxisCount[SDL_AXIS_LEFT_UP] + 1, maxCount);
                        }
                        currentControllerAxisState.LeftUp = -sensorValue;
                    } else {
                        currentControllerAxisState.LeftUp = 0.0f;
                        if (currentControllerAxisState.LeftDown == 0.0f) {
                            controllerAxisCount[SDL_AXIS_LEFT_DOWN] = std::min (controllerAxisCount[SDL_AXIS_LEFT_DOWN] + 1, maxCount);
                        }
                        currentControllerAxisState.LeftDown = sensorValue;
                    } break;
                    case SDL_GAMEPAD_AXIS_RIGHTX: if (sensorValue < 0) {
                        currentControllerAxisState.RightRight = 0.0f;
                        if (currentControllerAxisState.RightLeft == 0.0f) {
                            controllerAxisCount[SDL_AXIS_RIGHT_LEFT] = std::min (controllerAxisCount[SDL_AXIS_RIGHT_LEFT] + 1, maxCount);
                        }
                        currentControllerAxisState.RightLeft = -sensorValue;
                    } else {
                        currentControllerAxisState.RightLeft = 0.0f;
                        if (currentControllerAxisState.RightRight == 0.0f) {
                            controllerAxisCount[SDL_AXIS_RIGHT_RIGHT] = std::min (controllerAxisCount[SDL_AXIS_RIGHT_RIGHT] + 1, maxCount);
                        }
                        currentControllerAxisState.RightRight = sensorValue;
                    } break;
                    case SDL_GAMEPAD_AXIS_RIGHTY: if (sensorValue < 0) {
                        currentControllerAxisState.RightDown = 0.0f;
                        if (currentControllerAxisState.RightUp == 0.0f) {
                            controllerAxisCount[SDL_AXIS_RIGHT_UP] = std::min (controllerAxisCount[SDL_AXIS_RIGHT_UP] + 1, maxCount);
                        }
                        currentControllerAxisState.RightUp = -sensorValue;
                    } else {
                        currentControllerAxisState.RightUp = 0.0f;
                        if (currentControllerAxisState.RightDown == 0.0f) {
                            controllerAxisCount[SDL_AXIS_RIGHT_DOWN] = std::min (controllerAxisCount[SDL_AXIS_RIGHT_DOWN] + 1, maxCount);
                        }
                        currentControllerAxisState.RightDown = sensorValue;
                    } break;
                    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: {
                        if (currentControllerAxisState.LTriggerDown == 0.0f) {
                            controllerAxisCount[SDL_AXIS_LTRIGGER_DOWN] = std::min (controllerAxisCount[SDL_AXIS_LTRIGGER_DOWN] + 1, maxCount);
                        }
                        currentControllerAxisState.LTriggerDown = sensorValue; 
                    } break;
                    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: {
                        if (currentControllerAxisState.RTriggerDown == 0.0f) {
                            controllerAxisCount[SDL_AXIS_RTRIGGER_DOWN] = std::min (controllerAxisCount[SDL_AXIS_RTRIGGER_DOWN] + 1, maxCount);
                        }
                        currentControllerAxisState.RTriggerDown = sensorValue;
                    } break;
                } break;
                }
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
        const auto props = SDL_GetGamepadProperties(controller);
        const bool hasRumble = SDL_GetBooleanProperty (props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
        if (!controller || !hasRumble) continue;
        SDL_RumbleGamepad (controller, left, right, length);
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
        keyboardDiff[keycode] = 1;
        keyboardClean = true;
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
        mouseWheelDiff[scroll - 1] = 1;
        return true;
    } return false;
}

bool
ControllerButtonIsDown (const SDL_GamepadButton button) {
    return currentControllerButtonsState[button];
}

int maxButtonCount = 1;
bool
ControllerButtonIsTapped (const SDL_GamepadButton button) {
    if (controllerCount[button] > 0) {
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
    default: return 0.0f;
    }
}

bool
ControllerAxisIsTapped (const SDLAxis axis) {
    if (controllerAxisCount[axis] > 0) {
        controllerAxisDiff[axis] = 1;
        return true;
    } return false;
}

bool
IsButtonTapped (const Keybindings &bindings) {
    for (size_t i = 0; i < ConfigKeyboardButtonsCount; i++) {
        if (bindings.keycodes[i] == 0) continue;
        if (KeyboardIsTapped (bindings.keycodes[i])) return true;
    }
    for (size_t i = 0; i < std::size (ConfigControllerButtons); i++) {
        if (bindings.buttons[i] == SDL_GAMEPAD_BUTTON_INVALID) continue;
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
}

float
IsButtonDown (const Keybindings &bindings) {
     for (size_t i = 0; i < ConfigKeyboardButtonsCount; i++) {
        if (bindings.keycodes[i] == 0) continue;
        if (KeyboardIsDown (bindings.keycodes[i])) return 1.0f;
    }
    for (size_t i = 0; i < std::size (ConfigControllerButtons); i++) {
        if (bindings.buttons[i] == SDL_GAMEPAD_BUTTON_INVALID) continue;
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
}
#endif