#include "constants.h"
#include "helpers.h"
#include "patches.h"
#include <ReadBarcode.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <queue>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include "stb_image.h"

extern GameVersion gameVersion;
extern std::vector<HMODULE> plugins;
extern bool acceptInvalidCards;
extern bool emulateCardReader;
extern bool emulateQr;
extern char accessCode1[21];
extern char accessCode2[21];
extern char chipId1[33];
extern char chipId2[33];

namespace patches::Scanner {
namespace Card {
    State state = State::Disable;

    i32           *attachData;
    u64            touchData;
    CallbackAttach callbackAttach;
    CallbackTouch  callbackTouch;

    std::string accessCodeTemplate = "00000000000000000000";
    std::string chipIdTemplate     = "00000000000000000000000000000000";

    u8 cardData[168] = {
        0x01, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x92, 0x2E, 0x58, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x5C, 0x97, 0x44, 0xF0, 0x88, 0x04, 0x00, 0x43, 0x26, 0x2C, 0x33, 0x00, 0x04,
        0x06, 0x10, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4E, 0x42, 0x47, 0x49, 0x43, 0x36,
        0x00, 0x00, 0xFA, 0xE9, 0x69, 0x00, 0xF6, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    namespace Internal {
        void
        AgentInsertCard (uint8_t cardData[168], bool internalInvoke) {
            if (callbackTouch) {
                state = internalInvoke ? State::Disable : State::CopyWait;
                patches::Plugins::UpdateStatus (1, false);
                callbackTouch (0, 0, cardData, touchData);
            }
        }

        void
        AgentInsertCardPlugin (int32_t a1, int32_t a2, uint8_t *a3, uint64_t a4) {
            if (callbackTouch) {
                state = State::CopyWait;
                patches::Plugins::UpdateStatus (1, false);
                callbackTouch (a1, a2, a3, a4);
            }
        }

        void
        AgentInsertCardOfficial (int32_t a1, int32_t a2, uint8_t *a3, uint64_t a4) {
            if (callbackTouch) {
                state = State::CopyWait;
                patches::Plugins::UpdateStatus (1, false);
                if (acceptInvalidCards && !a3[0]) {
                    char AccessId[21] = "00000000000000000001";
                    uint8_t UID[8] = {a3[12], a3[14], a3[15], a3[16], 0x90, 0x00, 0x00, 0x00};
                    uint64_t ReversedAccessID;
                    for (int i = 0; i < 8; i++)
                        ReversedAccessID = (ReversedAccessID << 8) | UID[i];
                    sprintf(AccessId, "%020llu", ReversedAccessID);
                    std::string accessCode = std::string (AccessId), chipId = "";
                    if (chipId.length () == 0) chipId = accessCode;
                    if (chipId.length () < 32) chipId = accessCodeTemplate.substr (0, 32 - chipId.length ()) + chipId;
                    for (size_t i = 0; i < 32; i++) *(cardData + 0x2C + i) = i < chipId.length() ? chipId[i] : 0;
                    for (size_t i = 0; i < 20; i++) *(cardData + 0x50 + i) = i < accessCode.length() ? accessCode[i] : 0;
                    callbackTouch (0, 0, cardData, a4);
                } else callbackTouch (a1, a2, a3, a4);
            }
        }

        bool
        Commit0 (std::string accessCode, std::string chipId, bool internalInvoke) {
            if (chipId.length () == 0) chipId = accessCode;
            if (chipId.length () < 32) chipId = accessCodeTemplate.substr (0, 32 - chipId.length ()) + chipId;
            for (size_t i = 0; i < 32; i++) *(cardData + 0x2C + i) = i < chipId.length() ? chipId[i] : 0;
            for (size_t i = 0; i < 20; i++) *(cardData + 0x50 + i) = i < accessCode.length() ? accessCode[i] : 0;
            if (!internalInvoke) LogMessage (LogLevel::INFO, "[Card] Insert Card accessCode: \"{}\" chipId: \"{}\"", accessCode, chipId);
            patches::Scanner::Card::Internal::AgentInsertCard (cardData, internalInvoke);
            return true;
        }
    }    

    HOOK (u64, bngrw_Init, PROC_ADDRESS ("bngrw.dll", "BngRwInit")) { return 0; }
    HOOK (void, bngrw_Fin, PROC_ADDRESS ("bngrw.dll", "BngRwFin")) { return; }
    HOOK (u64, bngrw_IsCmdExec, PROC_ADDRESS ("bngrw.dll", "BngRwIsCmdExec")) { return 0xFFFFFFFF; }
    HOOK (i32, bngrw_ReqSendUrl, PROC_ADDRESS ("bngrw.dll", "BngRwReqSendUrlTo")) { return 1; }
    HOOK (u64, bngrw_ReqLed, PROC_ADDRESS ("bngrw.dll", "BngRwReqLed")) { return 1; }
    HOOK (u64, bngrw_ReqBeep, PROC_ADDRESS ("bngrw.dll", "BngRwReqBeep")) { return 1; }
    HOOK (u64, bngrw_ReqAction, PROC_ADDRESS ("bngrw.dll", "BngRwReqAction")) { return 1; }
    HOOK (u64, bngrw_ReqSetLedPower, PROC_ADDRESS ("bngrw.dll", "BngRwReqSetLedPower")) { return 0; }
    HOOK (u64, bngrw_GetRetryCount, PROC_ADDRESS ("bngrw.dll", "BngRwGetTotalRetryCount")) { return 0; }
    HOOK (u64, bngrw_GetFwVersion, PROC_ADDRESS ("bngrw.dll", "BngRwGetFwVersion")) { return 0; }
    HOOK (u64, bngrw_ReqFwVersionUp, PROC_ADDRESS ("bngrw.dll", "BngRwReqFwVersionUp")) { return 1; }
    HOOK (u64, bngrw_ReqFwCleanup, PROC_ADDRESS ("bngrw.dll", "BngRwReqFwCleanup")) { return 1; }
    HOOK (u64, bngrw_ReadMifare, PROC_ADDRESS ("bngrw.dll", "BngRwExReadMifareAllBlock")) { return 0xFFFFFF9C; }
    HOOK (u64, bngrw_GetStationID, PROC_ADDRESS ("bngrw.dll", "BngRwGetStationID")) { return 0; }
    HOOK (i32, bngrw_ReqSendMail, PROC_ADDRESS ("bngrw.dll", "BngRwReqSendMailTo")) { return 1; }
    HOOK (i32, bngrw_ReqLatchID, PROC_ADDRESS ("bngrw.dll", "BngRwReqLatchID")) { return 1; }
    HOOK (u64, bngrw_ReqAiccAuth, PROC_ADDRESS ("bngrw.dll", "BngRwReqAiccAuth")) { return 1; }
    HOOK (u64, bngrw_DevReset, PROC_ADDRESS ("bngrw.dll", "BngRwDevReset")) { return 1; }       // Invoke when enter testmode
    HOOK (i32, bngrw_ReqCancel, PROC_ADDRESS ("bngrw.dll", "BngRwReqCancel")) { 
        if (state != State::Disable) {
            state = State::Disable;
            patches::Scanner::Card::Internal::Commit0 (accessCodeTemplate, chipIdTemplate, true);
        }
        return 1;
    }
    HOOK (u64, bngrw_Attach, PROC_ADDRESS ("bngrw.dll", "BngRwAttach"), i32 a1, char *a2, i32 a3, i32 a4, CallbackAttach callback, i32 *a6) {
        callbackAttach = callback;
        attachData = a6;
        return 1;
    }
    HOOK (u64, bngrw_ReqWaitTouch, PROC_ADDRESS ("bngrw.dll", "BngRwReqWaitTouch"), u32 a1, i32 a2, u32 a3, CallbackTouch callback, u64 a5) {
        state = State::Ready;
        patches::Plugins::UpdateStatus (1, true);
        patches::Plugins::WaitTouch (Internal::AgentInsertCardPlugin, a5);
        callbackTouch = callback;
        touchData = a5;
        return 1;
    }

    HOOK (i64, bngrw_ReqCancelOfficial, PROC_ADDRESS ("bngrw.dll", "BngRwReqCancel"), u32 a1) { 
        if (state != State::Disable) {
            state = State::Disable;
            patches::Plugins::UpdateStatus (1, false);
        }
        return originalbngrw_ReqCancelOfficial(a1);
    }
    HOOK (u64, bngrw_ReqWaitTouchOfficial, PROC_ADDRESS ("bngrw.dll", "BngRwReqWaitTouch"), u32 a1, i32 a2, u32 a3, CallbackTouch callback, u64 a5) {
        state = State::Ready;
        patches::Plugins::UpdateStatus (1, true);
        callbackTouch = callback;
        touchData = a5;
        return originalbngrw_ReqWaitTouchOfficial (a1, a2, a3, Internal::AgentInsertCardOfficial, a5);
    }

    bool
    Commit(std::string accessCode, std::string chipId) {
        if (!emulateCardReader) {
            LogMessage (LogLevel::DEBUG, "[Card] Not emulate CardReader!");
            return false;
        }
        if (state != State::Ready) {
            LogMessage (LogLevel::DEBUG, "[Card] Not Waiting for Touch, please wait!");
            return false;
        }
        if (accessCode.length() == 0 || accessCode.length() > 20) {
            LogMessage (LogLevel::ERROR, "[Card] Not an effective accessCode: \"{}\"", accessCode);
            return false;
        }
        if (chipId.length() > 32) {
            LogMessage (LogLevel::ERROR, "[Card] Not an effective chipId: \"{}\"", chipId);
            return false;
        }
        return patches::Scanner::Card::Internal::Commit0 (accessCode, chipId, false);
    }

    void
    Update() {
        if (callbackAttach) callbackAttach (0, 0, attachData);
    }

    void
    Init() {
        LogMessage (LogLevel::INFO, "Init Card patches");
        if (!emulateCardReader) {
            LogMessage (LogLevel::WARN, "[Card] Card reader emulation disabled!");
            INSTALL_HOOK (bngrw_ReqCancelOfficial);
            INSTALL_HOOK (bngrw_ReqWaitTouchOfficial);
            // patches::Plugins::InitCardReader (patches::Scanner::Card::Commit);
            return;
        }

        INSTALL_HOOK (bngrw_Init)
        INSTALL_HOOK (bngrw_Fin);
        INSTALL_HOOK (bngrw_IsCmdExec);
        INSTALL_HOOK (bngrw_ReqCancel);
        INSTALL_HOOK (bngrw_ReqWaitTouch);
        INSTALL_HOOK (bngrw_ReqSendUrl);
        INSTALL_HOOK (bngrw_ReqLed);
        INSTALL_HOOK (bngrw_ReqBeep);
        INSTALL_HOOK (bngrw_ReqAction);
        INSTALL_HOOK (bngrw_ReqSetLedPower);
        INSTALL_HOOK (bngrw_GetRetryCount);
        INSTALL_HOOK (bngrw_GetFwVersion);
        INSTALL_HOOK (bngrw_ReqFwVersionUp);
        INSTALL_HOOK (bngrw_ReqFwCleanup);
        INSTALL_HOOK (bngrw_ReadMifare);
        INSTALL_HOOK (bngrw_GetStationID);
        INSTALL_HOOK (bngrw_ReqSendMail);
        INSTALL_HOOK (bngrw_ReqLatchID);
        INSTALL_HOOK (bngrw_ReqAiccAuth);
        INSTALL_HOOK (bngrw_Attach);
        INSTALL_HOOK (bngrw_DevReset);
        patches::Plugins::InitCardReader (patches::Scanner::Card::Commit);
    }
}
namespace Qr {
    std::queue<std::vector<uint8_t>> scanQueue;
    State state = State::Disable;
    long long lastScan;

    HOOK_DYNAMIC (char, QrInit, i64) { return 1; }
    HOOK_DYNAMIC (char, QrClose, i64) { return 1; }
    HOOK_DYNAMIC (char, QrRead, i64 a1) {
        *(DWORD *)(a1 + 40) = 1;
        *(DWORD *)(a1 + 16) = 1;
        *(BYTE *)(a1 + 112) = 0;
        return 1;
    }
    HOOK_DYNAMIC (i64, CallQrUnknown, i64) { return 1; }
    HOOK_DYNAMIC (bool, Send1, i64 a1) {
        *(BYTE *)(a1 + 88) = 1;
        *(i64 *)(a1 + 32)  = *(i64 *)(a1 + 24);
        *(WORD *)(a1 + 89) = 0;
        return true;
    }
    HOOK_DYNAMIC (bool, Send2, i64 a1) {
        *(WORD *)(a1 + 88) = 0;
        *(BYTE *)(a1 + 90) = 0;
        return true;
    }
    HOOK_DYNAMIC (bool, Send3, i64, char) { return true; }
    HOOK_DYNAMIC (bool, Send4, i64, const void *, i64) { return true; }
    HOOK_DYNAMIC (i64, CopyData, i64, void *dest, int length) {
        patches::Plugins::UsingQr ();
        lastScan = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now ().time_since_epoch ()).count ();
        if (state == State::CopyWait && scanQueue.size () > 0) {
            std::vector<uint8_t> data = scanQueue.front ();
            if ((int)data.size () > length) {
                LogMessage (LogLevel::ERROR, "[QR] Not an effective code, length: {} require: {}", data.size (), length);
            }
            std::stringstream hexStream;
            hexStream << std::hex << std::uppercase << std::setfill ('0') << std::setw (2);
            for (size_t i = 0; i < data.size (); i++) hexStream << static_cast<int> (data[i]) << " ";
            LogMessage (LogLevel::INFO, "[QR] Read QRData size: {} data: {}\n", data.size (), hexStream.str ());
            memcpy (dest, data.data (), data.size () + 1);
            scanQueue.pop ();
            if (scanQueue.size () == 0) state = State::Ready;
            return data.size ();
        } else if (state == State::Disable) {
            while (!scanQueue.empty ()) scanQueue.pop ();
            state = State::Ready;
            patches::Plugins::UpdateStatus (2, true);
        }
        return 0;
    }

    bool
    Commit (std::vector<uint8_t> &buffer) {
        if (!emulateQr) {
            LogMessage (LogLevel::DEBUG, "[QR] Not emulate QR Scanner!");
            return false;
        }
        if (state == State::Disable) {
            LogMessage (LogLevel::DEBUG, "[QR] Not Ready to accept QRData!");
            return false;
        }
        if (buffer.size () == 0) {
            LogMessage (LogLevel::ERROR, "[QR] Not an effective code, length: 0");
            return false;
        }
        if (scanQueue.empty() || scanQueue.back() != buffer) {
            std::vector<uint8_t> scanData = {};
            for (uint8_t byte_data : buffer) 
                scanData.push_back (byte_data);
            scanQueue.push (scanData);
        }
        if (state == State::Ready) state = State::CopyWait;
        return true;
    }

    bool
    CommitLogin (std::string accessCode) {
        if (!emulateQr) {
            LogMessage (LogLevel::DEBUG, "[QR] Not emulate QR Scanner!");
            return false;
        }
        std::vector<uint8_t> buffer = {};
        std::string accessQRDataBase = "BNTTCNID";
        if (!accessCode.starts_with (accessQRDataBase)) 
            for (char word : accessQRDataBase) buffer.push_back (word);
        for (char word : accessCode) buffer.push_back (word);
        return patches::Scanner::Qr::Commit (buffer);
    }

    void
    Update () {
        if (state != State::Disable) {
            if ((lastScan + 200) < std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now ().time_since_epoch ()).count ()) {
                state = State::Disable;
                patches::Plugins::UpdateStatus (2, false);
            } else {
                void *plugin = patches::Plugins::CheckQr ();
                if (plugin) {
                    uint8_t *space = (uint8_t *)calloc (600, sizeof (uint8_t));
                    size_t size = patches::Plugins::GetQr (plugin, 600, space);
                    if (size > 0) {
                        std::vector<uint8_t> data = {};
                        for (size_t i = 0; i < size; i ++) data.push_back (space[i]);
                        patches::Scanner::Qr::Commit (data);
                    }
                }
            }
        }
    }

    std::vector<uint8_t> &
    ReadQRData (std::vector<uint8_t> &buffer) {
        std::string serial = "";
        u16 type           = 0;
        std::vector<i64> songNoes;

        buffer.clear ();
        auto configPath = std::filesystem::current_path () / "config.toml";
        std::unique_ptr<toml_table_t, void (*) (toml_table_t *)> config_ptr (openConfig (configPath), toml_free);
        if (config_ptr) {
            auto qr = openConfigSection (config_ptr.get (), "qr");
            if (qr) {
                auto data = openConfigSection (qr, "data");
                if (data) {
                    serial   = readConfigString (data, "serial", "");
                    type     = (u16) readConfigInt (data, "type", 0);
                    songNoes = readConfigIntArray (data, "song_no", songNoes);
                }
            }
        }
        std::vector<uint8_t> header = { 0x53, 0x31, 0x32, 0x00, 0x00, 0xFF, 0xFF, (uint8_t)serial.size (), 0x01, 0x00 };
        for (uint8_t byte_data : header) buffer.push_back (byte_data);
        for (char word : serial)         buffer.push_back ((uint8_t)word);
        if (type == 5) {
            std::vector<uint8_t> folder = { 0xFF, 0xFF, (uint8_t)(songNoes.size () * 2), (uint8_t)(type & 0xFF), (uint8_t)((type >> 8) & 0xFF) };
            for (uint8_t byte_data : folder) buffer.push_back (byte_data);
            for (i64 songNo : songNoes) {
                buffer.push_back ((uint8_t)(songNo & 0xFF));
                buffer.push_back ((uint8_t)((songNo >> 8) & 0xFF));
            }
        }
        buffer.push_back (0xEE);
        buffer.push_back (0xFF);
        return buffer;
    }

    std::vector<uint8_t> &
    ReadQRImage (std::vector<uint8_t> &buffer) {
        std::string imagePath = "";

        buffer.clear ();
        auto configPath = std::filesystem::current_path () / "config.toml";
        std::unique_ptr<toml_table_t, void (*) (toml_table_t *)> config_ptr (openConfig (configPath), toml_free);
        if (config_ptr) {
            auto qr = openConfigSection (config_ptr.get (), "qr");
            if (qr) imagePath = readConfigString (qr, "image_path", "");
        }
        std::u8string u8PathStr (imagePath.begin (), imagePath.end ());
        std::filesystem::path u8Path (u8PathStr);
        if (!std::filesystem::is_regular_file (u8Path)) {
            LogMessage (LogLevel::ERROR, "Failed to open image: {} (file not found)", u8Path.string());
            return buffer;
        }
        int width, height, channels;
        std::unique_ptr<stbi_uc, void (*) (void *)> img_buffer (stbi_load (u8Path.string ().c_str (), &width, &height, &channels, 3), stbi_image_free);
        if (!img_buffer) {
            LogMessage (LogLevel::ERROR, "Failed to read image: {} ({})\n", u8Path.string(), stbi_failure_reason ());
            return buffer;
        }
        ZXing::ImageView image{img_buffer.get (), width, height, ZXing::ImageFormat::RGB};
        auto result = ReadBarcode (image);
        if (!result.isValid ()) {
            LogMessage (LogLevel::ERROR, "Failed to read QR: {} ({})\n", u8Path.string(), ToString (result.error ()));
            return buffer;
        }
        for (uint8_t byte_data : result.bytes()) buffer.push_back (byte_data);
        return buffer;
    }

    void
    Init () {
        LogMessage (LogLevel::INFO, "Init Qr patches");

        if (!emulateQr) {
            LogMessage (LogLevel::WARN, "[QR] QR emulation disabled!");
            return;
        }
        patches::Plugins::InitQr (gameVersion);
        SetConsoleOutputCP (CP_UTF8);
        auto amHandle = reinterpret_cast<u64> (GetModuleHandle ("AMFrameWork.dll"));
        switch (gameVersion) {
            case GameVersion::JPN00: {
                INSTALL_HOOK_DYNAMIC (QrInit, (LPVOID)(amHandle + 0x1B3E0));
                INSTALL_HOOK_DYNAMIC (QrClose, (LPVOID)(amHandle + 0x1B5B0));
                INSTALL_HOOK_DYNAMIC (QrRead, (LPVOID)(amHandle + 0x1B600));
                INSTALL_HOOK_DYNAMIC (CallQrUnknown, (LPVOID)(amHandle + 0xFD40));
                INSTALL_HOOK_DYNAMIC (Send1, (LPVOID)(amHandle + 0x1BBB0));
                INSTALL_HOOK_DYNAMIC (Send2, (LPVOID)(amHandle + 0x1BBF0));
                INSTALL_HOOK_DYNAMIC (Send3, (LPVOID)(amHandle + 0x1BC60));
                // JPN00 has no Send4
                INSTALL_HOOK_DYNAMIC (CopyData, (LPVOID)(amHandle + 0x1BC30));
                break;
            }
            case GameVersion::JPN08: {
                INSTALL_HOOK_DYNAMIC (QrInit, (LPVOID)(amHandle + 0x1BA00));
                INSTALL_HOOK_DYNAMIC (QrClose, (LPVOID)(amHandle + 0x1BBD0));
                INSTALL_HOOK_DYNAMIC (QrRead, (LPVOID)(amHandle + 0x1BC20));
                INSTALL_HOOK_DYNAMIC (CallQrUnknown, (LPVOID)(amHandle + 0xFD40));
                INSTALL_HOOK_DYNAMIC (Send1, (LPVOID)(amHandle + 0x1C220));
                INSTALL_HOOK_DYNAMIC (Send2, (LPVOID)(amHandle + 0x1C260));
                INSTALL_HOOK_DYNAMIC (Send3, (LPVOID)(amHandle + 0x1C2D0));
                // JPN08 has no Send4
                INSTALL_HOOK_DYNAMIC (CopyData, (LPVOID)(amHandle + 0x1C2A0));
                break;
            }
            case GameVersion::JPN39: {
                INSTALL_HOOK_DYNAMIC (QrInit, (LPVOID)(amHandle + 0x1EDC0));
                INSTALL_HOOK_DYNAMIC (QrClose, (LPVOID)(amHandle + 0x1EF60));
                INSTALL_HOOK_DYNAMIC (QrRead, (LPVOID)(amHandle + 0x1EFB0));
                INSTALL_HOOK_DYNAMIC (CallQrUnknown, (LPVOID)(amHandle + 0x11A70));
                INSTALL_HOOK_DYNAMIC (Send1, (LPVOID)(amHandle + 0x1F5B0));
                INSTALL_HOOK_DYNAMIC (Send2, (LPVOID)(amHandle + 0x1F5F0));
                INSTALL_HOOK_DYNAMIC (Send3, (LPVOID)(amHandle + 0x1F660));
                INSTALL_HOOK_DYNAMIC (Send4, (LPVOID)(amHandle + 0x1F690));
                INSTALL_HOOK_DYNAMIC (CopyData, (LPVOID)(amHandle + 0x1F630));
                break;
            }
            case GameVersion::CHN00: {
                INSTALL_HOOK_DYNAMIC (QrInit, (LPVOID)(amHandle + 0x161B0));
                INSTALL_HOOK_DYNAMIC (QrClose, (LPVOID)(amHandle + 0x16350));
                INSTALL_HOOK_DYNAMIC (QrRead, (LPVOID)(amHandle + 0x163A0));
                INSTALL_HOOK_DYNAMIC (CallQrUnknown, (LPVOID)(amHandle + 0x8F60));
                INSTALL_HOOK_DYNAMIC (Send1, (LPVOID)(amHandle + 0x16940));
                INSTALL_HOOK_DYNAMIC (Send2, (LPVOID)(amHandle + 0x16990));
                INSTALL_HOOK_DYNAMIC (Send3, (LPVOID)(amHandle + 0x16A00));
                INSTALL_HOOK_DYNAMIC (Send4, (LPVOID)(amHandle + 0x16A30));
                INSTALL_HOOK_DYNAMIC (CopyData, (LPVOID)(amHandle + 0x169D0));
                break;
            }
            default: {
                break;
            }
        }
        patches::Plugins::InitQRScanner (patches::Scanner::Qr::Commit);
        patches::Plugins::InitQRLogin (patches::Scanner::Qr::CommitLogin);
    }
}

void
Update() {
    patches::Scanner::Card::Update ();
    patches::Scanner::Qr::Update ();
}

void
Init() {
    LogMessage (LogLevel::INFO, "Init Scanner patches");
    patches::Scanner::Card::Init ();
    patches::Scanner::Qr::Init ();
}
}