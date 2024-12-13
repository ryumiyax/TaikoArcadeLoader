#include "poll.h"

#ifndef ASYNC_IO
#include "polldef.h"
#include <SDL3/SDL_main.h>
extern bool jpLayout;


bool currentKeyboardState[0xFF];
bool lastKeyboardState[0xFF];

bool currentControllerButtonsState[SDL_GAMEPAD_BUTTON_COUNT];
bool lastControllerButtonsState[SDL_GAMEPAD_BUTTON_COUNT];
SDLAxisState currentControllerAxisState;
SDLAxisState lastControllerAxisState;

SDL_Window *window;
SDL_Gamepad *controllers[255];

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
            *usePoll = true;
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
            *usePoll = true;
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

bool
InitializePoll (const HWND windowHandle) {
    bool hasRumble = true;
    SDL_SetMainReady ();

    SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
    SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");

    if (SDL_Init (SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS | SDL_INIT_VIDEO) != 0) {
        if (SDL_Init (SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS | SDL_INIT_VIDEO) == 0) {
            hasRumble = false;
        } else {
            LogMessage (LogLevel::ERROR,
                        std::string ("SDL_Init (SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS | SDL_INIT_VIDEO): ")
                            + SDL_GetError ());
            return false;
        }
    }

    if (const auto configPath = std::filesystem::current_path () / "gamecontrollerdb.txt";
        SDL_AddGamepadMappingsFromFile (configPath.string ().c_str ()) == -1)
        LogMessage (LogLevel::ERROR, "Cannot read gamecontrollerdb.txt");
    SDL_SetGamepadEventsEnabled (true);
    SDL_SetJoystickEventsEnabled (true);

    int numJoysticks = 0;
    auto* joyStickIds = SDL_GetJoysticks(&numJoysticks);
    if (joyStickIds == nullptr) {
        LogMessage (LogLevel::ERROR, "Error getting joystick ids");
        exit(1);
    }
    for (auto id:joyStickIds) {
        if (!SDL_IsGamepad (id)) continue;
        SDL_Gamepad *controller = SDL_OpenGamepad (id);

        if (!controller) {
            LogMessage (LogLevel::WARN, std::string ("Could not open gamecontroller ") + SDL_GetGamepadNameForID (id) + ": " + SDL_GetError ());
            continue;
        }

        LogMessage (LogLevel::DEBUG, "Init Controller connected!");
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

void
UpdatePoll (const HWND windowHandle) {
    if (windowHandle == nullptr || GetForegroundWindow () != windowHandle) return;

    memcpy (lastKeyboardState, currentKeyboardState, 255);
    memcpy (lastControllerButtonsState, currentControllerButtonsState, 21);
    lastMouseState          = currentMouseState;
    lastControllerAxisState = currentControllerAxisState;

    for (u8 i = 0; i < 0xFF; i++)
        currentKeyboardState[i] = GetAsyncKeyState (i) != 0;

    currentMouseState.ScrolledUp   = false;
    currentMouseState.ScrolledDown = false;

    GetCursorPos (&currentMouseState.Position);
    ScreenToClient (windowHandle, &currentMouseState.Position);

    SDL_Event event;
    SDL_Gamepad *controller;
    while (SDL_PollEvent (&event) != 0) {
        switch (event.type) {
        case SDL_EVENT_GAMEPAD_ADDED:
            if (!SDL_IsGamepad (event.cdevice.which)) break;

            controller = SDL_OpenGamepad (event.cdevice.which);
            if (!controller) {
                LogMessage (LogLevel::ERROR, std::string ("Could not open gamecontroller ") + SDL_GetGamepadNameForID (event.cdevice.which)
                                                 + ": " + SDL_GetError ());
                continue;
            }
            controllers[event.cdevice.which] = controller;
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (!SDL_IsGamepad (event.cdevice.which)) break;
            SDL_CloseGamepad (controllers[event.cdevice.which]);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (event.wheel.y > 0) currentMouseState.ScrolledUp = true;
            else if (event.wheel.y < 0) currentMouseState.ScrolledDown = true;
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: currentControllerButtonsState[event.gbutton.button] = event.gbutton.down; break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (event.gaxis.value > 1) {
                switch (event.gaxis.axis) {
                case SDL_GAMEPAD_AXIS_LEFTX: currentControllerAxisState.LeftRight = static_cast<float> (event.gaxis.value) / 32767; break;
                case SDL_GAMEPAD_AXIS_LEFTY: currentControllerAxisState.LeftDown = static_cast<float> (event.gaxis.value) / 32767; break;
                case SDL_GAMEPAD_AXIS_RIGHTX: currentControllerAxisState.RightRight = static_cast<float> (event.gaxis.value) / 32767; break;
                case SDL_GAMEPAD_AXIS_RIGHTY: currentControllerAxisState.RightDown = static_cast<float> (event.gaxis.value) / 32767; break;
                case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: currentControllerAxisState.LTriggerDown = static_cast<float> (event.gaxis.value) / 32767; break;
                case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
                    currentControllerAxisState.RTriggerDown = static_cast<float> (event.gaxis.value) / 32767;
                    break;
                default: break;
                }
            } else if (event.gaxis.value < -1) {
                switch (event.gaxis.axis) {
                case SDL_GAMEPAD_AXIS_LEFTX: currentControllerAxisState.LeftLeft = static_cast<float> (event.gaxis.value) / -32768; break;
                case SDL_GAMEPAD_AXIS_LEFTY: currentControllerAxisState.LeftUp = static_cast<float> (event.gaxis.value) / -32768; break;
                case SDL_GAMEPAD_AXIS_RIGHTX: currentControllerAxisState.RightLeft = static_cast<float> (event.gaxis.value) / -32768; break;
                case SDL_GAMEPAD_AXIS_RIGHTY: currentControllerAxisState.RightUp = static_cast<float> (event.gaxis.value) / -32768; break;
                default: break;
                }
            } else {
                switch (event.gaxis.axis) {
                case SDL_GAMEPAD_AXIS_LEFTX:
                    currentControllerAxisState.LeftRight = 0;
                    currentControllerAxisState.LeftLeft  = 0;
                    break;
                case SDL_GAMEPAD_AXIS_LEFTY:
                    currentControllerAxisState.LeftDown = 0;
                    currentControllerAxisState.LeftUp   = 0;
                    break;
                case SDL_GAMEPAD_AXIS_RIGHTX:
                    currentControllerAxisState.RightRight = 0;
                    currentControllerAxisState.RightLeft  = 0;
                    break;
                case SDL_GAMEPAD_AXIS_RIGHTY:
                    currentControllerAxisState.RightDown = 0;
                    currentControllerAxisState.RightUp   = 0;
                    break;
                case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: currentControllerAxisState.LTriggerDown = 0; break;
                case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: currentControllerAxisState.RTriggerDown = 0; break;
                default: break;
                }
            }
            break;
        default: break;
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
    ConfigValue rval{};
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

InternalButtonState
GetInternalButtonState (const Keybindings &bindings) {
    InternalButtonState buttons = {};

    for (size_t i = 0; i < ConfigKeyboardButtonsCount; i++) {
        if (bindings.keycodes[i] == 0) continue;
        if (KeyboardIsReleased (bindings.keycodes[i])) buttons.Released = true;
        if (KeyboardIsDown (bindings.keycodes[i])) buttons.Down = 1;
        if (KeyboardIsTapped (bindings.keycodes[i])) buttons.Tapped = true;
    }
    for (size_t i = 0; i < std::size (ConfigControllerButtons); i++) {
        if (bindings.buttons[i] == SDL_GAMEPAD_BUTTON_INVALID) continue;
        if (ControllerButtonIsReleased (bindings.buttons[i])) buttons.Released = true;
        if (ControllerButtonIsDown (bindings.buttons[i])) buttons.Down = 1;
        if (ControllerButtonIsTapped (bindings.buttons[i])) buttons.Tapped = true;
    }
    for (size_t i = 0; i < std::size (ConfigControllerAXIS); i++) {
        if (bindings.axis[i] == 0) continue;
        if (const float val = ControllerAxisIsReleased (bindings.axis[i]))
            buttons.Released = static_cast<bool> (val);                                                            // NOLINT(*-narrowing-conversions)
        if (const float val = ControllerAxisIsDown (bindings.axis[i])) buttons.Down = val;                         // NOLINT(*-narrowing-conversions)
        if (const float val = ControllerAxisIsTapped (bindings.axis[i])) buttons.Tapped = static_cast<bool> (val); // NOLINT(*-narrowing-conversions)
    }
    for (const auto i : bindings.scroll) {
        if (i == 0) continue;
        if (GetMouseScrollIsReleased (i)) buttons.Released = true;
        if (GetMouseScrollIsDown (i)) buttons.Down = 1;
        if (GetMouseScrollIsTapped (i)) buttons.Tapped = true;
    }

    return buttons;
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
KeyboardIsDown (const u8 keycode) {
    return currentKeyboardState[keycode];
}

bool
KeyboardIsUp (const u8 keycode) {
    return !KeyboardIsDown (keycode);
}

bool
KeyboardIsTapped (const u8 keycode) {
    return KeyboardIsDown (keycode) && KeyboardWasUp (keycode);
}

bool
KeyboardIsReleased (const u8 keycode) {
    return KeyboardIsUp (keycode) && KeyboardWasDown (keycode);
}

bool
KeyboardWasDown (const u8 keycode) {
    return lastKeyboardState[keycode];
}

bool
KeyboardWasUp (const u8 keycode) {
    return !KeyboardWasDown (keycode);
}

POINT
GetMousePosition () { return currentMouseState.Position; }

POINT
GetLastMousePosition () { return lastMouseState.Position; }

POINT
GetMouseRelativePosition () { return currentMouseState.RelativePosition; }

POINT
GetLastMouseRelativePosition () { return lastMouseState.RelativePosition; }

void
SetMousePosition (const POINT newPosition) {
    currentMouseState.Position = newPosition;
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
GetWasMouseScrollUp () {
    return lastMouseState.ScrolledUp;
}

bool
GetWasMouseScrollDown () {
    return lastMouseState.ScrolledDown;
}

bool
GetMouseScrollIsReleased (const Scroll scroll) {
    if (scroll == MOUSE_SCROLL_UP) return !GetMouseScrollUp () && GetWasMouseScrollUp ();
    return !GetMouseScrollDown () && GetWasMouseScrollDown ();
}

bool
GetMouseScrollIsDown (const Scroll scroll) {
    if (scroll == MOUSE_SCROLL_UP) return GetMouseScrollUp ();
    return GetMouseScrollDown ();
}

bool
GetMouseScrollIsTapped (const Scroll scroll) {
    if (scroll == MOUSE_SCROLL_UP) return GetMouseScrollUp () && !GetWasMouseScrollUp ();
    return GetMouseScrollDown () && !GetWasMouseScrollDown ();
}

bool
ControllerButtonIsDown (const SDL_GamepadButton button) {
    return currentControllerButtonsState[button];
}

bool
ControllerButtonIsUp (const SDL_GamepadButton button) {
    return !ControllerButtonIsDown (button);
}

bool
ControllerButtonWasDown (const SDL_GamepadButton button) {
    return lastControllerButtonsState[button];
}

bool
ControllerButtonWasUp (const SDL_GamepadButton button) {
    return !ControllerButtonWasDown (button);
}

bool
ControllerButtonIsTapped (const SDL_GamepadButton button) {
    return ControllerButtonIsDown (button) && ControllerButtonWasUp (button);
}

bool
ControllerButtonIsReleased (const SDL_GamepadButton button) {
    return ControllerButtonIsUp (button) && ControllerButtonWasDown (button);
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
ControllerAxisIsUp (const SDLAxis axis) {
    return !static_cast<bool> (ControllerAxisIsDown (axis));
}

float
ControllerAxisWasDown (const SDLAxis axis) {
    switch (axis) {
    case SDL_AXIS_LEFT_LEFT: return lastControllerAxisState.LeftLeft;
    case SDL_AXIS_LEFT_RIGHT: return lastControllerAxisState.LeftRight;
    case SDL_AXIS_LEFT_UP: return lastControllerAxisState.LeftUp;
    case SDL_AXIS_LEFT_DOWN: return lastControllerAxisState.LeftDown;
    case SDL_AXIS_RIGHT_LEFT: return lastControllerAxisState.RightLeft;
    case SDL_AXIS_RIGHT_RIGHT: return lastControllerAxisState.RightRight;
    case SDL_AXIS_RIGHT_UP: return lastControllerAxisState.RightUp;
    case SDL_AXIS_RIGHT_DOWN: return lastControllerAxisState.RightDown;
    case SDL_AXIS_LTRIGGER_DOWN: return lastControllerAxisState.LTriggerDown;
    case SDL_AXIS_RTRIGGER_DOWN: return lastControllerAxisState.RTriggerDown;
    default: return false;
    }
}

bool
ControllerAxisWasUp (const SDLAxis axis) {
    return !static_cast<bool> (ControllerAxisWasDown (axis));
}

bool
ControllerAxisIsTapped (const SDLAxis axis) {
    return static_cast<bool> (ControllerAxisIsDown (axis)) && ControllerAxisWasUp (axis);
}

bool
ControllerAxisIsReleased (const SDLAxis axis) {
    return ControllerAxisIsUp (axis) && static_cast<bool> (ControllerAxisWasDown (axis));
}

bool
IsButtonTapped (const Keybindings &bindings) {
    return GetInternalButtonState (bindings).Tapped;
}

bool
IsButtonReleased (const Keybindings &bindings) {
    return GetInternalButtonState (bindings).Released;
}

float
IsButtonDown (const Keybindings &bindings) {
    return GetInternalButtonState (bindings).Down;
}
#endif