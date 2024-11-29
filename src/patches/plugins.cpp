#include "constants.h"
#include "helpers.h"
#include "patches.h"

namespace patches::Plugins {
    std::vector<HMODULE> plugins = {};

    typedef void   (*BasicEvent)          ();
    typedef void   (*WaitTouchEvent)      (CallBackTouchCard callback, uint64_t touchData);
    typedef void   (*SendVersionEvent)    (GameVersion gameVersion);
    typedef bool   (*CheckEvent)          ();
    typedef size_t (*CopyDataEvent)       (size_t size, uint8_t *buffer);
    typedef void   (*SendCardReaderEvent) (CommitCardCallback touch);
    typedef void   (*SendQRScannerEvent)  (CommitQrCallback scan);
    typedef void   (*SendQRLoginEvent)    (CommitQrLoginCallback login);
    typedef void   (*StatusChangeEvent)   (size_t type, bool status);

    void
    Init () {
        for (auto plugin : plugins) {
            auto event = GetProcAddress (plugin, "Init");
            if (event) ((BasicEvent)event) ();
        }
    }
    void
    Update () {
        for (auto plugin : plugins) {
            auto event = GetProcAddress (plugin, "Update");
            if (event) ((BasicEvent)event) ();
        }
    }
    void
    Exit () {
        for (auto plugin : plugins) {
            auto event = GetProcAddress (plugin, "Exit");
            if (event) ((BasicEvent)event) ();
        }
    }
    // Card API
    void 
    WaitTouch (CallBackTouchCard callback, uint64_t touchData) {
        for (auto plugin : plugins) {
            auto event = GetProcAddress (plugin, "WaitTouch");
            if (event) ((WaitTouchEvent)event) (callback, touchData);
        }
    }
    // QR API (deprecated)
    void 
    InitQr (GameVersion gameVersion) {
        for (auto plugin : plugins) {
            auto event = GetProcAddress (plugin, "InitQr");
            if (event) ((SendVersionEvent)event) (gameVersion);
        }
    }
    void
    UsingQr () {
        for (auto plugin : plugins) {
            auto event = GetProcAddress (plugin, "UsingQr");
            if (event) ((BasicEvent)event) ();
        }
    }
    void * 
    CheckQr () {
        for (auto plugin : plugins) {
            auto event = GetProcAddress (plugin, "UsingQr");
            if (event && ((CheckEvent)event) ()) return plugin;
        }
        return nullptr;
    }
    size_t 
    GetQr (void *plugin, size_t size, uint8_t *buffer) {
        auto event = GetProcAddress (*(HMODULE *)plugin, "GetQr");
        if (event) return ((CopyDataEvent)event) (size, buffer);
        else return 0;
    }
    // New API
    void
    InitVersion (GameVersion gameVersion) {
        for (auto plugin : plugins) {
            auto event = GetProcAddress (plugin, "InitVersion");
            if (event) ((SendVersionEvent)event) (gameVersion);
        }
    }
    void
    InitCardReader (CommitCardCallback touch) {
        for (auto plugin : plugins) {
            auto event = GetProcAddress (plugin, "InitCardReader");
            if (event) ((SendCardReaderEvent)event) (touch);
        }
    }
    void
    InitQRScanner (CommitQrCallback scan) {
        for (auto plugin : plugins) {
            auto event = GetProcAddress (plugin, "InitQRScanner");
            if (event) ((SendQRScannerEvent)event) (scan);
        }
    }
    void
    InitQRLogin (CommitQrLoginCallback login) {
        for (auto plugin : plugins) {
            auto event = GetProcAddress (plugin, "InitQRLogin");
            if (event) ((SendQRLoginEvent)event) (login);
        }
    }
    void
    UpdateStatus (size_t type, bool status) {
        // printWarning ("Send UpdateStatus type=%d status=%d", type, status);
        for (auto plugin : plugins) {
            auto event = GetProcAddress (plugin, "UpdateStatus");
            if (event) ((StatusChangeEvent)event) (type, status);
        }
    }

    // Plugins Loader
    void LoadPlugins () {
        auto pluginPath = std::filesystem::current_path () / "plugins";

        if (std::filesystem::exists (pluginPath)) {
            for (const auto &entry : std::filesystem::directory_iterator (pluginPath)) {
                if (entry.path ().extension () == ".dll") {
                    auto name      = entry.path ().wstring ();
                    auto shortName = entry.path ().filename ().wstring ();
                    if (HMODULE hModule = LoadLibraryW (name.c_str ()); !hModule) {
                        LogMessage (LogLevel::ERROR, L"Failed to load plugin " + shortName);
                    } else {
                        plugins.push_back (hModule);
                        LogMessage (LogLevel::INFO, L"Loaded plugin " + shortName);
                    }
                }
            }
        }
    }




}