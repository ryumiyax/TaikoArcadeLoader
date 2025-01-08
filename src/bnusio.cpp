#include <queue>
#include "constants.h"
#include "helpers.h"
#include "patches/patches.h"
#include "bnusio.h"
#include "poll.h"

extern GameVersion gameVersion;
extern std::vector<HMODULE> plugins;
extern u64 song_data_size;
extern void *song_data;
extern char accessCode1[21];
extern char accessCode2[21];
extern char chipId1[33];
extern char chipId2[33];

extern bool emulateUsio;
extern bool acceptInvalidCards;
extern HWND hGameWnd;

Keybindings EXIT          = {.keycodes = {VK_ESCAPE}};
Keybindings TEST          = {.keycodes = {VK_F1}};
Keybindings SERVICE       = {.keycodes = {VK_F2}};
Keybindings DEBUG_UP      = {.keycodes = {VK_UP}};
Keybindings DEBUG_DOWN    = {.keycodes = {VK_DOWN}};
Keybindings DEBUG_ENTER   = {.keycodes = {VK_RETURN}};
Keybindings COIN_ADD      = {.keycodes = {VK_RETURN}, .buttons = {SDL_GAMEPAD_BUTTON_START}};
Keybindings CARD_INSERT_1 = {.keycodes = {'P'}};
Keybindings CARD_INSERT_2 = {};
Keybindings QR_DATA_READ  = {.keycodes = {'Q'}};
Keybindings QR_IMAGE_READ = {.keycodes = {'W'}};
Keybindings P1_LEFT_BLUE  = {.keycodes = {'D'}, .axis = {SDL_AXIS_LEFT_DOWN}};
Keybindings P1_LEFT_RED   = {.keycodes = {'F'}, .axis = {SDL_AXIS_LEFT_RIGHT}};
Keybindings P1_RIGHT_RED  = {.keycodes = {'J'}, .axis = {SDL_AXIS_RIGHT_RIGHT}};
Keybindings P1_RIGHT_BLUE = {.keycodes = {'K'}, .axis = {SDL_AXIS_RIGHT_DOWN}};
Keybindings P2_LEFT_BLUE  = {.keycodes = {'Z'}};
Keybindings P2_LEFT_RED   = {.keycodes = {'X'}};
Keybindings P2_RIGHT_RED  = {.keycodes = {'C'}};
Keybindings P2_RIGHT_BLUE = {.keycodes = {'V'}};
// Keybindings ALL[] = { EXIT, TEST, SERVICE, DEBUG_UP, DEBUG_DOWN, DEBUG_ENTER, COIN_ADD, 
//     CARD_INSERT_1, CARD_INSERT_2, QR_DATA_READ, QR_IMAGE_READ, P1_LEFT_BLUE, P1_LEFT_RED,
//     P1_RIGHT_RED, P1_RIGHT_BLUE, P2_LEFT_BLUE, P2_LEFT_RED, P2_RIGHT_RED, P2_RIGHT_BLUE };

const int exitWait  = 100;
int exited          = 0;
bool testEnabled    = false;
bool insert_coin    = false;
bool insert_service = false;
u16 coin_count      = 0;
u16 service_count   = 0;
bool inited         = false;
bool updateByCoin   = false;
HWND windowHandle   = nullptr;
float axisThreshold = 0.6f;
u8 inputState       = 1 | (1 << 2);
bool globalKeyboard = false;

namespace bnusio {
HMODULE bnusioOriginal = LoadLibrary ("bnusio_original.dll");
#define BNUSIO_EXPORT(returnType, functionName, ...) \
    typedef returnType (*type_##functionName) (__VA_ARGS__); \
    type_##functionName ori_##functionName = bnusioOriginal == nullptr ? nullptr : (type_##functionName)GetProcAddress (bnusioOriginal, #functionName); \
    returnType functionName (__VA_ARGS__)

#define CALL_BNUSIO_FALSE(functionName, ...) \
    { if (!emulateUsio && ori_##functionName) return ori_##functionName (__VA_ARGS__); else return 0; }

typedef u16 (__fastcall *AnalogMethod) (const u8);
AnalogMethod analogMethod = nullptr;

extern "C" {
BNUSIO_EXPORT (i64, bnusio_Open)                                     CALL_BNUSIO_FALSE (bnusio_Open)
BNUSIO_EXPORT (i64, bnusio_Close)                                    CALL_BNUSIO_FALSE (bnusio_Close)
BNUSIO_EXPORT (i64, bnusio_Communication, i32 a1)                    CALL_BNUSIO_FALSE (bnusio_Communication, a1)
BNUSIO_EXPORT (u8, bnusio_IsConnected)                               CALL_BNUSIO_FALSE (bnusio_IsConnected)
BNUSIO_EXPORT (i32, bnusio_ResetIoBoard)                             CALL_BNUSIO_FALSE (bnusio_ResetIoBoard)
BNUSIO_EXPORT (u16, bnusio_GetStatusU16, u16 a1)                     CALL_BNUSIO_FALSE (bnusio_GetStatusU16, a1)
BNUSIO_EXPORT (u8, bnusio_GetStatusU8, u16 a1)                       CALL_BNUSIO_FALSE (bnusio_GetStatusU8, a1)
BNUSIO_EXPORT (u16, bnusio_GetRegisterU16, i16 a1)                   CALL_BNUSIO_FALSE (bnusio_GetRegisterU16, a1)
BNUSIO_EXPORT (u8, bnusio_GetRegisterU8, u16 a1)                     CALL_BNUSIO_FALSE (bnusio_GetRegisterU8, a1)
BNUSIO_EXPORT (void *, bnusio_GetBuffer, u16 a1, i64 a2, i16 a3)     CALL_BNUSIO_FALSE (bnusio_GetBuffer, a1, a2, a3)
BNUSIO_EXPORT (i64, bnusio_SetRegisterU16, u16 a1, u16 a2)           CALL_BNUSIO_FALSE (bnusio_SetRegisterU16, a1, a2)
BNUSIO_EXPORT (i64, bnusio_SetRegisterU8, u16 a1, u8 a2)            CALL_BNUSIO_FALSE (bnusio_SetRegisterU8, a1, a2)
BNUSIO_EXPORT (i64, bnusio_SetBuffer, u16 a1, i32 a2, i16 a3)        CALL_BNUSIO_FALSE (bnusio_SetBuffer, a1, a2, a3)
BNUSIO_EXPORT (void *, bnusio_GetSystemError)                        CALL_BNUSIO_FALSE (bnusio_GetSystemError)
BNUSIO_EXPORT (i64, bnusio_SetSystemError, i16 a1)                   CALL_BNUSIO_FALSE (bnusio_SetSystemError, a1)
BNUSIO_EXPORT (i64, bnusio_ClearSram)                                CALL_BNUSIO_FALSE (bnusio_ClearSram)
BNUSIO_EXPORT (void *, bnusio_GetExpansionMode)                      CALL_BNUSIO_FALSE (bnusio_GetExpansionMode)
BNUSIO_EXPORT (i64, bnusio_SetExpansionMode, i16 a1)                 CALL_BNUSIO_FALSE (bnusio_SetExpansionMode, a1)
BNUSIO_EXPORT (u8, bnusio_IsWideUsio)                                CALL_BNUSIO_FALSE (bnusio_IsWideUsio)
BNUSIO_EXPORT (u64, bnusio_GetSwIn64)                                CALL_BNUSIO_FALSE (bnusio_GetSwIn64)
BNUSIO_EXPORT (u8, bnusio_GetGout, u8 a1)                            CALL_BNUSIO_FALSE (bnusio_GetGout, a1)
BNUSIO_EXPORT (i64, bnusio_SetGout, u8 a1, u8 a2)                    CALL_BNUSIO_FALSE (bnusio_SetGout, a1, a2)
BNUSIO_EXPORT (u64, bnusio_GetEncoder)                               CALL_BNUSIO_FALSE (bnusio_GetEncoder)
BNUSIO_EXPORT (i64, bnusio_GetCoinLock, u8 a1)                       CALL_BNUSIO_FALSE (bnusio_GetCoinLock, a1)
BNUSIO_EXPORT (i64, bnusio_SetCoinLock, u8 a1, u8 a2)                CALL_BNUSIO_FALSE (bnusio_SetCoinLock, a1, a2)
BNUSIO_EXPORT (i64, bnusio_GetCDOut, u8 a1)                          CALL_BNUSIO_FALSE (bnusio_GetCDOut, a1)
BNUSIO_EXPORT (i64, bnusio_SetCDOut, u8 a1, u8 a2)                   CALL_BNUSIO_FALSE (bnusio_SetCDOut, a1, a2)
BNUSIO_EXPORT (i64, bnusio_GetHopOut, u8 a1)                         CALL_BNUSIO_FALSE (bnusio_GetHopOut, a1)
BNUSIO_EXPORT (i64, bnusio_SetHopOut, u8 a1, u8 a2)                  CALL_BNUSIO_FALSE (bnusio_SetHopOut, a1, a2)
BNUSIO_EXPORT (void *, bnusio_SetPLCounter, i16 a1)                  CALL_BNUSIO_FALSE (bnusio_SetPLCounter, a1)
BNUSIO_EXPORT (char *, bnusio_GetIoBoardName)                        CALL_BNUSIO_FALSE (bnusio_GetIoBoardName)
BNUSIO_EXPORT (i64, bnusio_SetHopperRequest, u16 a1, i16 a2)         CALL_BNUSIO_FALSE (bnusio_SetHopperRequest, a1, a2)
BNUSIO_EXPORT (i64, bnusio_SetHopperLimit, u16 a1, i16 a2)           CALL_BNUSIO_FALSE (bnusio_SetHopperLimit, a1, a2)
BNUSIO_EXPORT (i64, bnusio_SramRead, i32 a1, u8 a2, i32 a3, u16 a4)  CALL_BNUSIO_FALSE (bnusio_SramRead, a1, a2, a3, a4)
BNUSIO_EXPORT (i64, bnusio_SramWrite, i32 a1, u8 a2, i32 a3, u16 a4) CALL_BNUSIO_FALSE (bnusio_SramWrite, a1, a2, a3, a4)
BNUSIO_EXPORT (void *, bnusio_GetCoinError, i32 a1)                  CALL_BNUSIO_FALSE (bnusio_GetCoinError, a1)
BNUSIO_EXPORT (void *, bnusio_GetServiceError, i32 a1)               CALL_BNUSIO_FALSE (bnusio_GetServiceError, a1)
BNUSIO_EXPORT (i64, bnusio_DecCoin, i32 a1, u16 a2)                  CALL_BNUSIO_FALSE (bnusio_DecCoin, a1, a2)
BNUSIO_EXPORT (i64, bnusio_DecService, i32 a1, u16 a2)               CALL_BNUSIO_FALSE (bnusio_DecService, a1, a2)
BNUSIO_EXPORT (i64, bnusio_ResetCoin)                                CALL_BNUSIO_FALSE (bnusio_ResetCoin)
BNUSIO_EXPORT (size_t, bnusio_GetFirmwareVersion) {
    if (!emulateUsio && ori_bnusio_GetFirmwareVersion) return ori_bnusio_GetFirmwareVersion();
    else return 126;
}
BNUSIO_EXPORT (u32, bnusio_GetSwIn) {
    if (!emulateUsio && ori_bnusio_GetSwIn) return ori_bnusio_GetSwIn ();
    u32 sw = 0;
    sw |= static_cast<u32> (testEnabled) << 7;
    sw |= static_cast<u32> (IsButtonDown (DEBUG_ENTER)) << 9;
    sw |= static_cast<u32> (IsButtonDown (DEBUG_DOWN)) << 12;
    sw |= static_cast<u32> (IsButtonDown (DEBUG_UP)) << 13;
    sw |= static_cast<u32> (IsButtonDown (SERVICE)) << 14;
    return sw;
}
BNUSIO_EXPORT (u16, bnusio_GetAnalogIn, const u8 which) {
    if (!emulateUsio && ori_bnusio_GetAnalogIn) return ori_bnusio_GetAnalogIn (which);
    if (analogMethod) return analogMethod (which);
    else return 0;
}
BNUSIO_EXPORT (u16, bnusio_GetCoin, i32 a1) {
    if (updateByCoin && a1 == 1) bnusio::Update ();
    if (!emulateUsio && ori_bnusio_GetCoin) return ori_bnusio_GetCoin (a1);
    return coin_count;
}
BNUSIO_EXPORT (u16, bnusio_GetService, i32 a1) {
    if (!emulateUsio && ori_bnusio_GetService) return ori_bnusio_GetService (a1);
    return service_count;
}
}

bool analogInput = false;
SDLAxis analogBindings[] = {
    SDL_AXIS_LEFT_LEFT,  SDL_AXIS_LEFT_RIGHT,  SDL_AXIS_LEFT_DOWN,  SDL_AXIS_LEFT_UP,  // P1: LB, LR, RR, RB
    SDL_AXIS_RIGHT_LEFT, SDL_AXIS_RIGHT_RIGHT, SDL_AXIS_RIGHT_DOWN, SDL_AXIS_RIGHT_UP, // P2: LB, LR, RR, RB
};
u16 __fastcall
AnalogInputAxis (const u8 which) {
    u16 analogIn = (u16)(32768 * ControllerAxisIsDown (analogBindings[which])) + 1;
    return analogIn > 100 ? analogIn : 0;
}

bool waitAll = false;
short drumWaitPeriod = 0;
u16 buttonWaitPeriod[] = { 0, 0 };
std::queue<u8> buttonQueue[] = { {}, {} };
bool valueStates[] = {false, false, false, false, false, false, false, false};
Keybindings *analogButtons[] = {
    &P1_LEFT_BLUE, &P1_LEFT_RED, &P1_RIGHT_RED, &P1_RIGHT_BLUE, &P2_LEFT_BLUE, &P2_LEFT_RED, &P2_RIGHT_RED, &P2_RIGHT_BLUE
};
u16 __fastcall
AnalogInputWaitPeriod (const u8 which) {
    const auto button = analogButtons[which];
    const bool blue = !(which % 4 % 3);
    const int player = which / 4;

    u16 analogValue = 0;
    if (which == 0) for (int i=0; i<2; i++) if (buttonWaitPeriod[i] > 0) buttonWaitPeriod[i]--;
    if (buttonWaitPeriod[player] == 0 && !buttonQueue[player].empty () && buttonQueue[player].front () == which) {
        buttonWaitPeriod[player] = drumWaitPeriod;
        buttonQueue[player].pop ();

        const u16 hitValue = !valueStates[which] ? 50 : 51;
        valueStates[which] = !valueStates[which];
        analogValue = (hitValue << 15) / 100 + 1;
    }
    if (IsButtonTapped (*button)) {
        if ((waitAll || blue) && buttonWaitPeriod[player] > 0) {
            buttonQueue[player].push (which);
            return analogValue;
        }
        buttonWaitPeriod[player] = drumWaitPeriod;

        const u16 hitValue = !valueStates[which] ? 50 : 51;
        valueStates[which] = !valueStates[which];
        analogValue = (hitValue << 15) / 100 + 1;
    }
    return analogValue;
}

int cooldown[8] = { 0 };
u8 blueCooldown = 1, redCooldown = 1;
u16 __fastcall
AnalogInputCooldown (const u8 which) {
    const auto button = analogButtons[which];
    const bool blue = !(which % 4 % 3);

    if (cooldown[which] > 0) cooldown[which]--;
    if (IsButtonTapped (*button)) {
        if (cooldown[which] > 0) return 0;
        cooldown[which] = blue ? blueCooldown : redCooldown;
        const u16 hitValue = !valueStates[which] ? 50 : 51;
        valueStates[which] = !valueStates[which];
        return (hitValue << 15) / 100 + 1;
    } else return 0;
}

u16 __fastcall
AnalogInputSimple (const u8 which) {
    const auto button = analogButtons[which];
    if (IsButtonTapped (*button)) {
        const u16 hitValue = !valueStates[which] ? 50 : 51;
        valueStates[which] = !valueStates[which];
        return (hitValue << 15) / 100 + 1;
    }
    return 0;
}

void
Init () {
    SetKeyboardButtons ();

    int fpsLimit = 0;

    const auto configPath = std::filesystem::current_path () / "config.toml";
    const std::unique_ptr<toml_table_t, void (*) (toml_table_t *)> config_ptr (openConfig (configPath), toml_free);
    if (config_ptr) {
        const toml_table_t *config = config_ptr.get ();
        if (const auto controller = openConfigSection (config, "controller")) {
            drumWaitPeriod   = static_cast<short> (readConfigInt (controller, "wait_period", drumWaitPeriod));
            analogInput      = readConfigBool (controller, "analog_input", analogInput);
            globalKeyboard   = readConfigBool (controller, "global_keyboard", globalKeyboard);
        }
        // auto graphics = openConfigSection (config, "graphics");
        // if (graphics) {
        //     fpsLimit = (int)readConfigInt (graphics, "fpslimit", fpsLimit);
        // }
    }

    if (analogInput) {
        LogMessage (LogLevel::WARN, "[Analog Type] Axis: All the keyboard drum inputs have been disabled");
        analogMethod = AnalogInputAxis;
    } else if (drumWaitPeriod > 0) {
        LogMessage (LogLevel::WARN, "[Analog Type] WaitPeriod: Fast input might be queued");
        analogMethod = AnalogInputWaitPeriod;
    } else if (drumWaitPeriod == 0) {
        LogMessage (LogLevel::INFO, "[Analog Type] Cooldown: Fastest and original input");
        analogMethod = AnalogInputCooldown;
    } else {
        LogMessage (LogLevel::INFO, "[Analog Type] Simple: Fastest and original input");
        analogMethod = AnalogInputSimple;
    }

    updateByCoin = fpsLimit == 0;
    // if (updateByCoin) {
    //     LogMessage (LogLevel::INFO, "fpsLimit is set to 0, bnusio::Update() will invoke in getCoin callback");
    // }
    const auto keyConfigPath = std::filesystem::current_path () / "keyconfig.toml";
    const std::unique_ptr<toml_table_t, void (*) (toml_table_t *)> keyConfig_ptr (openConfig (keyConfigPath), toml_free);
    if (keyConfig_ptr) {
        inputState = 0;
        if (analogInput) inputState |= (1 << 2);
        const toml_table_t *keyConfig = keyConfig_ptr.get ();
        SetConfigValue (keyConfig, "EXIT", &EXIT, &inputState);

        SetConfigValue (keyConfig, "TEST", &TEST, &inputState);
        SetConfigValue (keyConfig, "SERVICE", &SERVICE, &inputState);
        SetConfigValue (keyConfig, "DEBUG_UP", &DEBUG_UP, &inputState);
        SetConfigValue (keyConfig, "DEBUG_DOWN", &DEBUG_DOWN, &inputState);
        SetConfigValue (keyConfig, "DEBUG_ENTER", &DEBUG_ENTER, &inputState);

        SetConfigValue (keyConfig, "COIN_ADD", &COIN_ADD, &inputState);
        SetConfigValue (keyConfig, "CARD_INSERT_1", &CARD_INSERT_1, &inputState);
        SetConfigValue (keyConfig, "CARD_INSERT_2", &CARD_INSERT_2, &inputState);
        SetConfigValue (keyConfig, "QR_DATA_READ", &QR_DATA_READ, &inputState);
        SetConfigValue (keyConfig, "QR_IMAGE_READ", &QR_IMAGE_READ, &inputState);

        SetConfigValue (keyConfig, "P1_LEFT_BLUE", &P1_LEFT_BLUE, &inputState);
        SetConfigValue (keyConfig, "P1_LEFT_RED", &P1_LEFT_RED, &inputState);
        SetConfigValue (keyConfig, "P1_RIGHT_RED", &P1_RIGHT_RED, &inputState);
        SetConfigValue (keyConfig, "P1_RIGHT_BLUE", &P1_RIGHT_BLUE, &inputState);
        SetConfigValue (keyConfig, "P2_LEFT_BLUE", &P2_LEFT_BLUE, &inputState);
        SetConfigValue (keyConfig, "P2_LEFT_RED", &P2_LEFT_RED, &inputState);
        SetConfigValue (keyConfig, "P2_RIGHT_RED", &P2_RIGHT_RED, &inputState);
        SetConfigValue (keyConfig, "P2_RIGHT_BLUE", &P2_RIGHT_BLUE, &inputState);
    }

    LogMessage (LogLevel::INFO, "Finish Loading keyconfig.toml  useKeyboard={} useMouse={} useController={}", 
        (inputState & 1) ? "true" : "false", (inputState & (1 << 1)) ? "true" : "false", (inputState & (1 << 2)) ? "true" : "false");

    if (!emulateUsio && !exists (std::filesystem::current_path () / "bnusio_original.dll")) {
        emulateUsio = true;
        LogMessage (LogLevel::ERROR, "bnusio_original.dll not found! usio emulation enabled");
    }

    if (!emulateUsio && bnusioOriginal) {
        LogMessage (LogLevel::WARN, "USIO emulation disabled");
    }
}

void
Update () {
    if (exited && ++exited >= exitWait) ExitProcess (0);
    if (!inited) {
        windowHandle = FindWindowA ("nuFoundation.Window", nullptr);
        InitializePoll (windowHandle);
        patches::Plugins::Init ();
        inited = true;
    }
    UpdatePoll (windowHandle);

    std::vector<uint8_t> buffer = {};
    if (IsButtonTapped (COIN_ADD)) coin_count++;
    if (IsButtonTapped (SERVICE)) service_count++;
    if (IsButtonTapped (TEST)) testEnabled = !testEnabled;
    if (exited == 0 && IsButtonTapped (EXIT)) {
        LogMessage (LogLevel::INFO, "Exit by Press Exit Button!");
        exited += 1; testEnabled = 1; std::thread([](){patches::Plugins::Exit ();}).detach();
    }
    if (GameVersion::CHN00 == gameVersion) {
        if (IsButtonTapped (CARD_INSERT_1)) patches::Scanner::Qr::CommitLogin (accessCode1);
        if (IsButtonTapped (CARD_INSERT_2)) patches::Scanner::Qr::CommitLogin (accessCode2);
    } else {
        if (IsButtonTapped (CARD_INSERT_1)) patches::Scanner::Card::Commit (accessCode1, chipId1);
        if (IsButtonTapped (CARD_INSERT_2)) patches::Scanner::Card::Commit (accessCode2, chipId2);
    }
    if (IsButtonTapped (QR_DATA_READ))  patches::Scanner::Qr::Commit (patches::Scanner::Qr::ReadQRData (buffer));
    if (IsButtonTapped (QR_IMAGE_READ)) patches::Scanner::Qr::Commit (patches::Scanner::Qr::ReadQRImage (buffer));

    patches::Plugins::Update ();
    patches::Scanner::Update ();
}

void
Close () {
    // patches::Plugins::Exit ();
    CleanupLogger ();
}
} // namespace bnusio
