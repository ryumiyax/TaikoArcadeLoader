#include "poll.h"

struct KeyCodePair {
    const char *string;
    u8 keycode;
};
size_t ConfigKeyboardButtonsCount      = 0;
KeyCodePair *ConfigKeyboardButtons     = nullptr;
KeyCodePair ConfigKeyboardButtons_US[] = {
    // Reference:https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
    // Wayback Machine:https://web.archive.org/web/20231223135232/https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
    // Row 1
    {"ESCAPE", VK_ESCAPE},
    {"F1", VK_F1},
    {"F2", VK_F2},
    {"F3", VK_F3},
    {"F4", VK_F4},
    {"F5", VK_F5},
    {"F6", VK_F6},
    {"F7", VK_F7},
    {"F8", VK_F8},
    {"F9", VK_F9},
    {"F10", VK_F10},
    {"F11", VK_F11},
    {"F12", VK_F12},

    // Row 2
    {"`", VK_OEM_3},
    {"1", '1'},
    {"2", '2'},
    {"3", '3'},
    {"4", '4'},
    {"5", '5'},
    {"6", '6'},
    {"7", '7'},
    {"8", '8'},
    {"9", '9'},
    {"0", '0'},
    {"-", VK_OEM_MINUS},
    {"=", VK_OEM_PLUS},
    {"BACKSPACE", VK_BACK},

    // Row 3
    {"TAB", VK_TAB},
    {"Q", 'Q'},
    {"W", 'W'},
    {"E", 'E'},
    {"R", 'R'},
    {"T", 'T'},
    {"Y", 'Y'},
    {"U", 'U'},
    {"I", 'I'},
    {"O", 'O'},
    {"P", 'P'},
    {"[", VK_OEM_4},
    {"]", VK_OEM_6},
    {"BACKSLASH", VK_OEM_5},

    // Row 4
    {"CAPS_LOCK", VK_CAPITAL},
    {"A", 'A'},
    {"S", 'S'},
    {"D", 'D'},
    {"F", 'F'},
    {"G", 'G'},
    {"H", 'H'},
    {"J", 'J'},
    {"K", 'K'},
    {"L", 'L'},
    {";", VK_OEM_1},
    {"'", VK_OEM_7},
    {"ENTER", VK_RETURN},

    // Row 5
    {"SHIFT", VK_SHIFT},
    {"Z", 'Z'},
    {"X", 'X'},
    {"C", 'C'},
    {"V", 'V'},
    {"B", 'B'},
    {"N", 'N'},
    {"M", 'M'},
    {",", VK_OEM_COMMA},
    {".", VK_OEM_PERIOD},
    {"SLASH", VK_OEM_2},

    // Row 6
    {"CONTROL", VK_CONTROL},
    {"L_WIN", VK_LWIN},
    {"ALT", VK_MENU},
    {"SPACE", VK_SPACE},
    {"R_WIN", VK_RWIN},
    {"MENU", VK_APPS},

    // Other Keys
    // PrtSc is more important when making snapshots, therefore comment it as reserved
    //{"PRINT_SCREEN", VK_SNAPSHOT},
    {"SCROLL_LOCK", VK_SCROLL},
    {"PAUSE", VK_PAUSE},
    {"INSERT", VK_INSERT},
    {"DELETE", VK_DELETE},
    {"HOME", VK_HOME},
    {"END", VK_END},
    {"PAGE_UP", VK_PRIOR},
    {"PAGE_DOWN", VK_NEXT},

    // Arrow Keys
    {"UPARROW", VK_UP},
    {"LEFTARROW", VK_LEFT},
    {"DOWNARROW", VK_DOWN},
    {"RIGHTARROW", VK_RIGHT},

    // NUMPAD Keys
    {"NUM_LOCK", VK_NUMLOCK},
    {"DIVIDE", VK_DIVIDE},
    {"MULTIPLY", VK_MULTIPLY},
    {"SUBTRACT", VK_SUBTRACT},
    {"NUM7", VK_NUMPAD7},
    {"NUM8", VK_NUMPAD8},
    {"NUM9", VK_NUMPAD9},
    {"ADD", VK_ADD},
    {"NUM4", VK_NUMPAD4},
    {"NUM5", VK_NUMPAD5},
    {"NUM6", VK_NUMPAD6},
    {"NUM1", VK_NUMPAD1},
    {"NUM2", VK_NUMPAD2},
    {"NUM3", VK_NUMPAD3},
    {"NUM0", VK_NUMPAD0},
    {"DECIMAL", VK_DECIMAL},
};
KeyCodePair ConfigKeyboardButtons_JP[] = {
    // Reference:https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
    // Wayback Machine:https://web.archive.org/web/20231223135232/https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
    // Row 1
    {"ESCAPE", VK_ESCAPE},
    {"F1", VK_F1},
    {"F2", VK_F2},
    {"F3", VK_F3},
    {"F4", VK_F4},
    {"F5", VK_F5},
    {"F6", VK_F6},
    {"F7", VK_F7},
    {"F8", VK_F8},
    {"F9", VK_F9},
    {"F10", VK_F10},
    {"F11", VK_F11},
    {"F12", VK_F12},

    // Row 2
    {"1", '1'},
    {"2", '2'},
    {"3", '3'},
    {"4", '4'},
    {"5", '5'},
    {"6", '6'},
    {"7", '7'},
    {"8", '8'},
    {"9", '9'},
    {"0", '0'},
    {"-", VK_OEM_MINUS},
    {"^", VK_OEM_7},
    {"YEN", VK_OEM_5},
    {"BACKSPACE", VK_BACK},

    // Row 3
    {"TAB", VK_TAB},
    {"Q", 'Q'},
    {"W", 'W'},
    {"E", 'E'},
    {"R", 'R'},
    {"T", 'T'},
    {"Y", 'Y'},
    {"U", 'U'},
    {"I", 'I'},
    {"O", 'O'},
    {"P", 'P'},
    {"@", VK_OEM_3},
    {"[", VK_OEM_4},

    // Row 4
    {"CAPS_LOCK", VK_CAPITAL},
    {"A", 'A'},
    {"S", 'S'},
    {"D", 'D'},
    {"F", 'F'},
    {"G", 'G'},
    {"H", 'H'},
    {"J", 'J'},
    {"K", 'K'},
    {"L", 'L'},
    {";", VK_OEM_PLUS},
    {":", VK_OEM_1},
    {"]", VK_OEM_6},
    {"ENTER", VK_RETURN},

    // Row 5
    {"SHIFT", VK_SHIFT},
    {"Z", 'Z'},
    {"X", 'X'},
    {"C", 'C'},
    {"V", 'V'},
    {"B", 'B'},
    {"N", 'N'},
    {"M", 'M'},
    {",", VK_OEM_COMMA},
    {".", VK_OEM_PERIOD},
    {"SLASH", VK_OEM_2},
    {"BACKSLASH", VK_OEM_102},

    // Row 6
    {"CONTROL", VK_CONTROL},
    {"L_WIN", VK_LWIN},
    {"ALT", VK_MENU},
    {"SPACE", VK_SPACE},
    {"R_WIN", VK_RWIN},
    {"MENU", VK_APPS},

    // Other Keys
    // PrtSc is more important when making snapshots, therefore comment it as reserved
    //{"PRINT_SCREEN", VK_SNAPSHOT},
    {"SCROLL_LOCK", VK_SCROLL},
    {"PAUSE", VK_PAUSE},
    {"INSERT", VK_INSERT},
    {"DELETE", VK_DELETE},
    {"HOME", VK_HOME},
    {"END", VK_END},
    {"PAGE_UP", VK_PRIOR},
    {"PAGE_DOWN", VK_NEXT},

    // Arrow Keys
    {"UPARROW", VK_UP},
    {"LEFTARROW", VK_LEFT},
    {"DOWNARROW", VK_DOWN},
    {"RIGHTARROW", VK_RIGHT},

    // NUMPAD Keys
    {"NUM_LOCK", VK_NUMLOCK},
    {"DIVIDE", VK_DIVIDE},
    {"MULTIPLY", VK_MULTIPLY},
    {"SUBTRACT", VK_SUBTRACT},
    {"NUM7", VK_NUMPAD7},
    {"NUM8", VK_NUMPAD8},
    {"NUM9", VK_NUMPAD9},
    {"ADD", VK_ADD},
    {"NUM4", VK_NUMPAD4},
    {"NUM5", VK_NUMPAD5},
    {"NUM6", VK_NUMPAD6},
    {"NUM1", VK_NUMPAD1},
    {"NUM2", VK_NUMPAD2},
    {"NUM3", VK_NUMPAD3},
    {"NUM0", VK_NUMPAD0},
    {"DECIMAL", VK_DECIMAL},
};

struct {
    const char *string;
    SDL_GamepadButton button;
} ConfigControllerButtons[] = {
    {"SDL_A", SDL_GAMEPAD_BUTTON_SOUTH},
    {"SDL_B", SDL_GAMEPAD_BUTTON_EAST},
    {"SDL_X", SDL_GAMEPAD_BUTTON_WEST},
    {"SDL_Y", SDL_GAMEPAD_BUTTON_NORTH},
    {"SDL_BACK", SDL_GAMEPAD_BUTTON_BACK},
    {"SDL_GUIDE", SDL_GAMEPAD_BUTTON_GUIDE},
    {"SDL_START", SDL_GAMEPAD_BUTTON_START},
    {"SDL_LSTICK_PRESS", SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {"SDL_RSTICK_PRESS", SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {"SDL_LSHOULDER", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {"SDL_RSHOULDER", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {"SDL_DPAD_UP", SDL_GAMEPAD_BUTTON_DPAD_UP},
    {"SDL_DPAD_DOWN", SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {"SDL_DPAD_LEFT", SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {"SDL_DPAD_RIGHT", SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    {"SDL_MISC", SDL_GAMEPAD_BUTTON_MISC1},
    {"SDL_PADDLE1", SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1},
    {"SDL_PADDLE2", SDL_GAMEPAD_BUTTON_LEFT_PADDLE1},
    {"SDL_PADDLE3", SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2},
    {"SDL_PADDLE4", SDL_GAMEPAD_BUTTON_LEFT_PADDLE2},
    {"SDL_TOUCHPAD", SDL_GAMEPAD_BUTTON_TOUCHPAD},
};

struct {
    const char *string;
    SDLAxis axis;
} ConfigControllerAXIS[] = {
    {"SDL_LSTICK_LEFT", SDL_AXIS_LEFT_LEFT},   {"SDL_LSTICK_UP", SDL_AXIS_LEFT_UP},        {"SDL_LSTICK_DOWN", SDL_AXIS_LEFT_DOWN},
    {"SDL_LSTICK_RIGHT", SDL_AXIS_LEFT_RIGHT}, {"SDL_RSTICK_LEFT", SDL_AXIS_RIGHT_LEFT},   {"SDL_RSTICK_UP", SDL_AXIS_RIGHT_UP},
    {"SDL_RSTICK_DOWN", SDL_AXIS_RIGHT_DOWN},  {"SDL_RSTICK_RIGHT", SDL_AXIS_RIGHT_RIGHT}, {"SDL_LTRIGGER", SDL_AXIS_LTRIGGER_DOWN},
    {"SDL_RTRIGGER", SDL_AXIS_RTRIGGER_DOWN},
};

struct {
    const char *string;
    Scroll scroll;
} ConfigMouseScroll[] = {
    {"SCROLL_UP", MOUSE_SCROLL_UP},
    {"SCROLL_DOWN", MOUSE_SCROLL_DOWN},
};

struct MouseState {
    POINT Position;
    POINT RelativePosition;
    bool ScrolledUp;
    bool ScrolledDown;
} currentMouseState, lastMouseState;