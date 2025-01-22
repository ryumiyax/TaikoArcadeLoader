#include "bnusio.h"
#include "constants.h"
#include "helpers.h"
#include "patches/patches.h"
#include "poll.h"
#include "logger.h"
#include <dbghelp.h>

auto gameVersion = GameVersion::UNKNOWN;
std::vector<HMODULE> plugins;
u64 song_data_size = 1024 * 1024 * 64;
void *song_data;

std::string server      = "127.0.0.1";
std::string port        = "54430";
std::string chassisId   = "284111080000";
std::string shopId      = "TAIKO ARCADE LOADER";
std::string gameVerNum  = "00.00";
std::string countryCode = "JPN";
char fullAddress[256]   = {};
char placeId[16]        = {};
char accessCode1[21]    = "00000000000000000001";
char accessCode2[21]    = "00000000000000000002";
char chipId1[33]        = "00000000000000000000000000000001";
char chipId2[33]        = "00000000000000000000000000000002";
bool windowed           = false;
bool autoIme            = true;
bool jpLayout           = false;
bool cursor             = true;
bool emulateUsio        = true;
bool emulateCardReader  = true;
bool emulateQr          = true;
bool acceptInvalidCards = false;
HKL currentLayout       = nullptr;
i32 xRes                = 1920;
i32 yRes                = 1080;
bool vsync              = false;

std::string logLevelStr = "INFO";
bool logToFile          = true;

HWND hGameWnd;
FAST_HOOK (i32, ShowMouse, PROC_ADDRESS ("user32.dll", "ShowCursor"), bool) { return originalShowMouse.stdcall<i32> (true); }
FAST_HOOK (i32, ExitWindows, PROC_ADDRESS ("user32.dll", "ExitWindowsEx")) { ExitProcess (0); }
// FAST_HOOK (ATOM, HookRegisterClassA, PROC_ADDRESS ("winuser.h", "RegisterClassA"), WNDCLASSA *lpWndClass) {
//     LogMessage (LogLevel::DEBUG, "RegisterClassA name={}", lpWndClass->lpszClassName);
//     return originalHookRegisterClassA.call<ATOM> (lpWndClass);
// }
// FAST_HOOK (ATOM, HookRegisterClassW, PROC_ADDRESS ("winuser.h", "RegisterClassW"), WNDCLASSW *lpWndClass) {
//     LogMessage (LogLevel::DEBUG, L"RegisterClassW name={}", lpWndClass->lpszClassName);
//     return originalHookRegisterClassW.call<ATOM> (lpWndClass);
// }
// FAST_HOOK (ATOM, HookRegisterClassExA, PROC_ADDRESS ("winuser.h", "RegisterClassExA"), WNDCLASSEXA *lpWndClass) {
//     LogMessage (LogLevel::DEBUG, "RegisterClassExA name={}", lpWndClass->lpszClassName);
//     return originalHookRegisterClassExA.call<ATOM> (lpWndClass);
// }
// FAST_HOOK (ATOM, HookRegisterClassExW, PROC_ADDRESS ("winuser.h", "RegisterClassExW"), WNDCLASSEXW *lpWndClass) {
//     LogMessage (LogLevel::DEBUG, L"RegisterClassExW name={}", lpWndClass->lpszClassName);
//     return originalHookRegisterClassExW.call<ATOM> (lpWndClass);
// }
FAST_HOOK (HWND, CreateWindow, PROC_ADDRESS ("user32.dll", "CreateWindowExW"), DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle,
      i32 X, i32 Y, i32 nWidth, i32 nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
    if (lpWindowName != nullptr) {
        if (wcscmp (lpWindowName, L"Taiko") == 0) {
            if (windowed) dwStyle = WS_TILEDWINDOW ^ WS_MAXIMIZEBOX ^ WS_THICKFRAME;

            hGameWnd
                = originalCreateWindow.stdcall<HWND> (dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
            return hGameWnd;
        }
    }
    return originalCreateWindow.stdcall<HWND> (dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}
FAST_HOOK (bool, SetWindowPosition, PROC_ADDRESS ("user32.dll", "SetWindowPos"), HWND hWnd, HWND hWndInsertAfter, i32 X, i32 Y, i32 cx, i32 cy,
      u32 uFlags) {
    if (hWnd == hGameWnd) {
        RECT rw, rc;
        GetWindowRect (hWnd, &rw);
        GetClientRect (hWnd, &rc);
        cx = rw.right - rw.left - (rc.right - rc.left) + cx;
        cy = rw.bottom - rw.top - (rc.bottom - rc.top) + cy;
    }
    return originalSetWindowPosition.stdcall<bool> (hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

FAST_HOOK (void, ExitProcessHook, PROC_ADDRESS ("kernel32.dll", "ExitProcess"), u32 uExitCode) {
    bnusio::Close ();
    originalExitProcessHook.stdcall<void> (uExitCode);
}

FAST_HOOK (i32, XinputGetState, PROC_ADDRESS ("xinput9_1_0.dll", "XInputGetState")) { return ERROR_DEVICE_NOT_CONNECTED; }
FAST_HOOK (i32, XinputSetState, PROC_ADDRESS ("xinput9_1_0.dll", "XInputSetState")) { return ERROR_DEVICE_NOT_CONNECTED; }
FAST_HOOK (i32, XinputGetCapabilites, PROC_ADDRESS ("xinput9_1_0.dll", "XInputGetCapabilities")) { return ERROR_DEVICE_NOT_CONNECTED; }

FAST_HOOK (i32, ssleay_Shutdown, PROC_ADDRESS ("ssleay32.dll", "SSL_shutdown")) { return 1; }

FAST_HOOK (i64, UsbFinderInitialize, PROC_ADDRESS ("nbamUsbFinder.dll", "nbamUsbFinderInitialize")) { return 0; }
FAST_HOOK (i64, UsbFinderRelease, PROC_ADDRESS ("nbamUsbFinder.dll", "nbamUsbFinderRelease")) { return 0; }
FAST_HOOK (i64, UsbFinderGetSerialNumber, PROC_ADDRESS ("nbamUsbFinder.dll", "nbamUsbFinderGetSerialNumber"), i32 a1, char *a2) {
    strcpy (a2, chassisId.c_str ());
    return 0;
}

FAST_HOOK (i32, ws2_getaddrinfo, PROC_ADDRESS ("ws2_32.dll", "getaddrinfo"), const char *node, char *service, void *hints, void *out) {
    return originalws2_getaddrinfo.stdcall<i32> (server.c_str (), service, hints, out);
}

void
GetGameVersion () {
    wchar_t w_path[MAX_PATH];
    GetModuleFileNameW (nullptr, w_path, MAX_PATH);
    const std::filesystem::path path (w_path);

    if (!exists (path) || !path.has_filename ()) {
        MessageBoxA (nullptr, "Failed to find executable", nullptr, MB_OK);
        ExitProcess (0);
    }

    std::ifstream stream (path, std::ios::binary);
    if (!stream.is_open ()) {
        MessageBoxA (nullptr, "Failed to read executable", nullptr, MB_OK);
        ExitProcess (0);
    }

    stream.seekg (0, std::ifstream::end);
    const size_t length = stream.tellg ();
    stream.seekg (0, std::ifstream::beg);

    const auto buf = static_cast<char *> (calloc (length + 1, sizeof (char)));
    stream.read (buf, length);

    gameVersion = static_cast<GameVersion> (XXH64 (buf, length, 0));

    stream.close ();
    free (buf);

    switch (gameVersion) {
    case GameVersion::JPN00:
    case GameVersion::JPN08:
    case GameVersion::JPN39:
    case GameVersion::CHN00: break;
    default: {
        LogMessage (LogLevel::ERROR, "Detected GameVersion is Unsupported!");
        MessageBoxA (nullptr, "Unsupported game version", nullptr, MB_OK); ExitProcess (0);
    }
    }
}

void
CreateCard () {
    LogMessage (LogLevel::INFO, "Creating card.ini");
    constexpr char hexCharacterTable[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    char buf[64]                       = {};
    srand (static_cast<unsigned int> (time (nullptr)));

    std::generate_n (buf, 20, [&] { return hexCharacterTable[rand () % 10]; });
    WritePrivateProfileStringA ("card", "accessCode1", buf, ".\\card.ini");
    std::generate_n (buf, 32, [&] { return hexCharacterTable[rand () % 16]; });
    WritePrivateProfileStringA ("card", "chipId1", buf, ".\\card.ini");
    std::generate_n (buf, 20, [&] { return hexCharacterTable[rand () % 10]; });
    WritePrivateProfileStringA ("card", "accessCode2", buf, ".\\card.ini");
    std::generate_n (buf, 32, [&] { return hexCharacterTable[rand () % 16]; });
    WritePrivateProfileStringA ("card", "chipId2", buf, ".\\card.ini");
}

BOOL
DllMain (HMODULE module, const DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        // This is bad, don't do this
        // I/O in DllMain can easily cause a deadlock

        // Init logger for loading config
        auto start = std::chrono::high_resolution_clock::now();
        InitializeLogger (GetLogLevel (logLevelStr), logToFile);
        patches::Timer::Init ();

        LogMessage (LogLevel::INFO, "Loading config...");

        std::string version                    = "auto";
        const std::filesystem::path configPath = std::filesystem::current_path () / "config.toml";
        const std::unique_ptr<toml_table_t, void (*) (toml_table_t *)> config_ptr (openConfig (configPath), toml_free);
        if (config_ptr) {
            const toml_table_t *config = config_ptr.get ();
            if (const auto amauthConfig = openConfigSection (config, "amauth")) {
                server      = readConfigString (amauthConfig, "server", server);
                port        = readConfigString (amauthConfig, "port", port);
                chassisId   = readConfigString (amauthConfig, "chassis_id", chassisId);
                shopId      = readConfigString (amauthConfig, "shop_id", shopId);
                gameVerNum  = readConfigString (amauthConfig, "game_ver", gameVerNum);
                countryCode = readConfigString (amauthConfig, "country_code", countryCode);

                std::strcat (fullAddress, server.c_str ());
                if (!port.empty ()) {
                    std::strcat (fullAddress, ":");
                    std::strcat (fullAddress, port.c_str ());
                }

                std::strcat (placeId, countryCode.c_str ());
                std::strcat (placeId, "0FF0");
            }
            if (const auto patches = openConfigSection (config, "patches")) version = readConfigString (patches, "version", version);
            if (const auto emulation = openConfigSection (config, "emulation")) {
                emulateUsio        = readConfigBool (emulation, "usio", emulateUsio);
                emulateCardReader  = readConfigBool (emulation, "card_reader", emulateCardReader);
                acceptInvalidCards = readConfigBool (emulation, "accept_invalid", acceptInvalidCards);
                emulateQr          = readConfigBool (emulation, "qr", emulateQr);
            }
            if (const auto graphics = openConfigSection (config, "graphics")) {
                windowed = readConfigBool (graphics, "windowed", windowed);
                cursor   = readConfigBool (graphics, "cursor", cursor);
                if (auto res = openConfigSection (graphics, "res")) {
                    xRes = static_cast<i32> (readConfigInt (res, "x", xRes));
                    yRes = static_cast<i32> (readConfigInt (res, "y", yRes));
                }
                vsync = readConfigBool (graphics, "vsync", vsync);
            }
            if (const auto keyboard = openConfigSection (config, "keyboard")) {
                autoIme  = readConfigBool (keyboard, "auto_ime", autoIme);
                jpLayout = readConfigBool (keyboard, "jp_layout", jpLayout);
            }

            if (const auto logging = openConfigSection (config, "logging")) {
                logLevelStr = readConfigString (logging, "log_level", logLevelStr);
                logToFile   = readConfigBool (logging, "log_to_file", logToFile);
            }
        }

        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE);
        auto activeWindow = GetActiveWindow();
        HMONITOR monitor = MonitorFromWindow(activeWindow, MONITOR_DEFAULTTONEAREST);

        // Get the logical width and height of the monitor
        MONITORINFOEX monitorInfoEx;
        monitorInfoEx.cbSize = sizeof(monitorInfoEx);
        GetMonitorInfo(monitor, &monitorInfoEx);
        auto cxLogical = monitorInfoEx.rcMonitor.right - monitorInfoEx.rcMonitor.left;
        auto cyLogical = monitorInfoEx.rcMonitor.bottom - monitorInfoEx.rcMonitor.top;

        // Get the physical width and height of the monitor
        DEVMODE devMode;
        devMode.dmSize = sizeof(devMode);
        devMode.dmDriverExtra = 0;
        EnumDisplaySettings(monitorInfoEx.szDevice, ENUM_CURRENT_SETTINGS, &devMode);
        auto cxPhysical = devMode.dmPelsWidth;
        auto cyPhysical = devMode.dmPelsHeight;

        // Calculate the scaling factor
        auto horizontalScale = (static_cast<double> (cxPhysical) / static_cast<double> (cxLogical));
        auto verticalScale = (static_cast<double> (cyPhysical) / static_cast<double> (cyLogical));
        if (windowed) {
            // Game will automatically adjust scale
            xRes = (int)(xRes / horizontalScale);
            yRes = (int)(yRes / verticalScale);
        } else {
            xRes = cxLogical;
            yRes = cyLogical;
            if (yRes * 16 > xRes * 9)      yRes = (int)(xRes * 9.0 / 16.0);
            else if (yRes * 16 < xRes * 9) xRes = (int)(yRes * 16.0 / 9.0);
        }

        LogMessage (LogLevel::INFO, "Scale Rate: x={} y={}", horizontalScale, verticalScale);
        LogMessage (LogLevel::INFO, "Boot with {} mode ({}x{})", windowed ? "window" : "fullscreen", xRes, yRes);

        if (autoIme) {
            currentLayout = GetKeyboardLayout (0);
            auto engLayout = LoadKeyboardLayout (TEXT ("00000409"), KLF_ACTIVATE);
            ActivateKeyboardLayout (engLayout, KLF_SETFORPROCESS);
            LogMessage (LogLevel::DEBUG, "Initial change KeyboardLayout {}", LOWORD(engLayout));
        }

        // Update the logger with the level read from config file.
        InitializeLogger (GetLogLevel (logLevelStr), logToFile);
        LogMessage (LogLevel::INFO, "Application started.");

        if (version == "auto") GetGameVersion ();
        else if (version == "JPN00") gameVersion = GameVersion::JPN00;
        else if (version == "JPN08") gameVersion = GameVersion::JPN08;
        else if (version == "JPN39") gameVersion = GameVersion::JPN39;
        else if (version == "CHN00") gameVersion = GameVersion::CHN00;
        else {
            LogMessage (LogLevel::ERROR, "Input GameVersion is UNKNOWN!");
            MessageBoxA (nullptr, "Unknown patch version", nullptr, MB_OK);
            ExitProcess (0);
        }
        LogMessage (LogLevel::INFO, "GameVersion is {}", GameVersionToString (gameVersion));

        patches::Plugins::LoadPlugins ();
        patches::Plugins::InitVersion (gameVersion);

        if (!std::filesystem::exists (".\\card.ini")) CreateCard ();
        GetPrivateProfileStringA ("card", "accessCode1", accessCode1, accessCode1, 21, ".\\card.ini");
        GetPrivateProfileStringA ("card", "chipId1", chipId1, chipId1, 33, ".\\card.ini");
        GetPrivateProfileStringA ("card", "accessCode2", accessCode2, accessCode2, 21, ".\\card.ini");
        GetPrivateProfileStringA ("card", "chipId2", chipId2, chipId2, 33, ".\\card.ini");

        LogMessage (LogLevel::INFO, "==== Loading patches, please wait...");

        if (windowed && cursor) INSTALL_FAST_HOOK (ShowMouse);
        INSTALL_FAST_HOOK (ExitWindows);
        INSTALL_FAST_HOOK (CreateWindow);
        // INSTALL_FAST_HOOK (HookRegisterClassA);
        // INSTALL_FAST_HOOK (HookRegisterClassW);
        // INSTALL_FAST_HOOK (HookRegisterClassExA);
        // INSTALL_FAST_HOOK (HookRegisterClassExW);
        INSTALL_FAST_HOOK (SetWindowPosition);

        INSTALL_FAST_HOOK (ExitProcessHook);

        INSTALL_FAST_HOOK (XinputGetState);
        INSTALL_FAST_HOOK (XinputSetState);
        INSTALL_FAST_HOOK (XinputGetCapabilites);

        INSTALL_FAST_HOOK (ssleay_Shutdown);

        INSTALL_FAST_HOOK (UsbFinderInitialize);
        INSTALL_FAST_HOOK (UsbFinderRelease);
        INSTALL_FAST_HOOK (UsbFinderGetSerialNumber);

        INSTALL_FAST_HOOK (ws2_getaddrinfo);

        bnusio::Init ();

        switch (gameVersion) {
        case GameVersion::UNKNOWN: break;
        case GameVersion::JPN00: patches::JPN00::Init (); break;
        case GameVersion::JPN08: patches::JPN08::Init (); break;
        case GameVersion::JPN39: patches::JPN39::Init (); break;
        case GameVersion::CHN00: patches::CHN00::Init (); break;
        }

        patches::Scanner::Init ();
        patches::Audio::Init ();
        // patches::Dxgi::Init ();
        patches::AmAuth::Init ();
        patches::Language::Init ();
        patches::TestMode::Init ();
        patches::LayeredFs::Init ();
        // patches::UnlimitSong::Init ();

        std::chrono::duration<double> duration = std::chrono::high_resolution_clock::now() - start;
        LogMessage (LogLevel::INFO, "==== Finished Loading patches! using: {:.2f}ms", duration.count () * 1000);
    }
    return true;
}
