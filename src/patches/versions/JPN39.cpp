// ReSharper disable CppTooWideScopeInitStatement
#include "helpers.h"
#include "../patches.h"
#include <map>

namespace patches::JPN39 {
int language = 0;
FAST_HOOK_DYNAMIC (char, AMFWTerminate, i64) {
    LogMessage (LogLevel::HOOKS, "AMFWTerminate was called");
    return 0;
}

FAST_HOOK_DYNAMIC (i64, curl_easy_setopt, i64 a1, i64 a2, i64 a3, i64 a4, i64 a5) {
    LogMessage (LogLevel::HOOKS, "Garmc curl_easy_setopt was called");
    originalcurl_easy_setopt.call<i64> (a1, 64, 0, 0, 0);
    originalcurl_easy_setopt.call<i64> (a1, 81, 0, 0, 0);
    return originalcurl_easy_setopt.call<i64> (a1, a2, a3, a4, a5);
}
FAST_HOOK_DYNAMIC (void, garmc_logger_log, i64 a1, int a2, void *a3, char a4) {
    // remove garmc log
    return;
}

FUNCTION_PTR (i64, GetPlayDataManagerRef, ASLR (0x140024AC0), i64);

i64 lua_State = 0;
FAST_HOOK (i64, luaL_newstate, PROC_ADDRESS ("lua51.dll", "luaL_newstate")) { return lua_State = originalluaL_newstate.call<i64> (); }
FUNCTION_PTR (void, lua_settop, PROC_ADDRESS ("lua51.dll", "lua_settop"), i64, i32);
FUNCTION_PTR (void, lua_replace, PROC_ADDRESS ("lua51.dll", "lua_replace"), i64, i32);
FUNCTION_PTR (void, lua_pushcclosure, PROC_ADDRESS ("lua51.dll", "lua_pushcclosure"), i64, i64, i32);
FUNCTION_PTR (void, lua_pushboolean, PROC_ADDRESS ("lua51.dll", "lua_pushboolean"), i64, i32);
FUNCTION_PTR (const char *, lua_pushstring, PROC_ADDRESS ("lua51.dll", "lua_pushstring"), i64, const char *);
FUNCTION_PTR (i32, lua_toboolean, PROC_ADDRESS ("lua51.dll", "lua_toboolean"), i64, i32);
FUNCTION_PTR (const char *, lua_tolstring, PROC_ADDRESS ("lua51.dll", "lua_tolstring"), u64, i32, size_t *);
FUNCTION_PTR (i32, lua_pcall, PROC_ADDRESS ("lua51.dll", "lua_pcall"), i64, i32, i32, i32);
FUNCTION_PTR (i32, luaL_loadstring, PROC_ADDRESS ("lua51.dll", "luaL_loadstring"), i64, const char *);
#define LUA_MULTRET         (-1)
#define luaL_dostring(L, s) (luaL_loadstring (L, s) || lua_pcall (L, 0, LUA_MULTRET, 0))

FUNCTION_PTR (u64, RefTestModeMain, ASLR (0x1400337C0), u64);
FUNCTION_PTR (u64, RefPlayDataManager, ASLR (0x140024AC0), u64);
FUNCTION_PTR (i64, GetUserCount, ASLR (0x1403F1020), u64);

i64
lua_pushbool (const i64 a1, const bool val) {
    lua_settop (a1, 0);
    lua_pushboolean (a1, val);
    return 1;
}

u64 appAccessor       = 0;
u64 componentAccessor = 0;
FAST_HOOK (i64, DeviceCheck, ASLR (0x140464FC0), i64 a1, i64 a2, i64 a3) {
    LogMessage (LogLevel::HOOKS, "DeviceCheck was called");
    TestMode::SetupAccessor (a3, RefTestModeMain);
    componentAccessor = a2;
    appAccessor = a3;
    return originalDeviceCheck.fastcall<i64> (a1, a2, a3);
}

i64
GetUserStatus () {
    if (appAccessor) {
        if (const u64 playDataManager = RefPlayDataManager (appAccessor)) return GetUserCount (playDataManager);
    }
    return -1;
}

FAST_HOOK (bool, IsSongRelease, ASLR (0x1403F4510), i64 a1, i64 a2, int a3) {
    if (TestMode::ReadTestModeValue (L"ModUnlockSongs") == 1) return true;
    return originalIsSongRelease.fastcall<bool> (a1, a2, a3);
}
FAST_HOOK (bool, IsSongReleasePlayer, ASLR (0x1403F45F0), i64 a1, u64 a2, i32 a3) {
    if (TestMode::ReadTestModeValue (L"ModUnlockSongs") == 2) return true;
    return originalIsSongReleasePlayer.fastcall<bool> (a1, a2, a3);
}
MID_HOOK (DifficultyPanelCrown, ASLR (0x1403F2A25), SafetyHookContext &ctx) {
    if (TestMode::ReadTestModeValue (L"ModUnlockSongs") != 1) return;
    ctx.r15 |= 1;
}
FAST_HOOK (i64, AvailableMode_Collabo024, ASLR (0x1402DE710), i64 a1) {
    LogMessage (LogLevel::HOOKS, "AvailableMode_Collabo024 was called");
    const int tournamentMode = TestMode::ReadTestModeValue (L"TournamentMode");
    if (tournamentMode == 1) return originalAvailableMode_Collabo024.fastcall<i64> (a1);
    const int status = TestMode::ReadTestModeValue (L"ModModeCollabo024");
    if (status == 1 && GetUserStatus () == 1) return lua_pushbool (a1, true);
    return originalAvailableMode_Collabo024.fastcall<i64> (a1);
}
FAST_HOOK (i64, AvailableMode_Collabo025, ASLR (0x1402DE6B0), i64 a1) {
    LogMessage (LogLevel::HOOKS, "AvailableMode_Collabo025 was called");
    const int tournamentMode = TestMode::ReadTestModeValue (L"TournamentMode");
    if (tournamentMode == 1) return originalAvailableMode_Collabo025.fastcall<i64> (a1);
    const int status = TestMode::ReadTestModeValue (L"ModModeCollabo025");
    if (status == 1 && GetUserStatus () == 1) return lua_pushbool (a1, true);
    return originalAvailableMode_Collabo025.fastcall<i64> (a1);
}
FAST_HOOK (i64, AvailableMode_Collabo026, ASLR (0x1402DE670), i64 a1) {
    LogMessage (LogLevel::HOOKS, "AvailableMode_Collabo026 was called");
    const int tournamentMode = TestMode::ReadTestModeValue (L"TournamentMode");
    if (tournamentMode == 1) return originalAvailableMode_Collabo026.fastcall<i64> (a1);
    const int status = TestMode::ReadTestModeValue (L"ModModeCollabo026");
    if (status == 1 && GetUserStatus () == 1) return lua_pushbool (a1, true);
    return originalAvailableMode_Collabo026.fastcall<i64> (a1);
}
FAST_HOOK (i64, AvailableMode_AprilFool001, ASLR (0x1402DE5B0), i64 a1) {
    LogMessage (LogLevel::HOOKS, "AvailableMode_AprilFool001 was called");
    const int tournamentMode = TestMode::ReadTestModeValue (L"TournamentMode");
    if (tournamentMode == 1) return originalAvailableMode_AprilFool001.fastcall<i64> (a1);
    const int status = TestMode::ReadTestModeValue (L"ModModeAprilFool001");
    if (status == 1) return lua_pushbool (a1, true);
    return originalAvailableMode_AprilFool001.fastcall<i64> (a1);
}
i64 __fastcall lua_freeze_timer (const i64 a1) {
    LogMessage (LogLevel::HOOKS, "lua_freeze_timer was called");
    const int tournamentMode = TestMode::ReadTestModeValue (L"TournamentMode");
    if (tournamentMode == 1) return lua_pushbool (a1, true);
    const int status = TestMode::ReadTestModeValue (L"ModFreezeTimer");
    if (status == 1) return lua_pushbool (a1, true);
    return lua_pushbool (a1, false);
}
MID_HOOK (FreezeTimer, ASLR (0x14019FF51), SafetyHookContext &ctx) {
    LogMessage (LogLevel::HOOKS, "FreezeTimer was called");
    const auto a1 = ctx.rdi;
    const int v9  = static_cast<int> (ctx.rax + 1);
    lua_pushcclosure (a1, reinterpret_cast<i64> (&lua_freeze_timer), v9);
    ctx.rip = ASLR (0x14019FF65);
}

void
ExecuteSendResultData () {
    luaL_dostring(lua_State, R"(
    local currentGameMode = PlayDataManager.GetPlayMode()

    if currentGameMode == GameMode.kEnso then
		if g_entryType == 0 then
            NetAccess.SendResultData(0)
        elseif g_entryType == 1 then
            NetAccess.SendResultData(1)
        else
            NetAccess.SendResultData(0)
            NetAccess.SendResultData(1)
        end
	elseif currentGameMode == GameMode.kAI then
		if g_joinSide_ == 1 then
            NetAccess.SendAiResultData(0)
        elseif g_joinSide_ == 2 then
            NetAccess.SendAiResultData(1)
        end
	elseif currentGameMode == GameMode.kCollabo025 then
		if g_joinSide_ == 1 then
            NetAccess.SendCollabo025AiResultData(0)
        elseif g_joinSide_ == 2 then
            NetAccess.SendCollabo025AiResultData(1)
        end
	elseif currentGameMode == GameMode.kCollabo026 then
		if g_joinSide_ == 1 then
            NetAccess.SendCollabo026AiResultData(0)
        elseif g_joinSide_ == 2 then
            NetAccess.SendCollabo026AiResultData(1)
        end
	elseif currentGameMode == GameMode.kAprilFool001 then
		if g_entryType == 0 then
            NetAccess.SendAprilFoolResultData(0)
        elseif g_entryType == 1 then
            NetAccess.SendAprilFoolResultData(1)
        else
            NetAccess.SendAprilFoolResultData(0)
            NetAccess.SendAprilFoolResultData(1)
        end
    end

    QR.StartQRExecAll(SceneType.kResult)
    )"
    );
}

SCENE_RESULT_HOOK (SceneResultInitialize_Enso, ASLR (0x140411FD0));
SCENE_RESULT_HOOK (SceneResultInitialize_AI, ASLR (0x140411FD0));
SCENE_RESULT_HOOK (SceneResultInitialize_Collabo025, ASLR (0x140411FD0));
SCENE_RESULT_HOOK (SceneResultInitialize_Collabo026, ASLR (0x140411FD0));
SCENE_RESULT_HOOK (SceneResultInitialize_AprilFool, ASLR (0x140411FD0));
SEND_RESULT_HOOK (SendResultData_Enso, ASLR (0x1401817B0));
SEND_RESULT_HOOK (SendResultData_AI, ASLR (0x1401755E0));
SEND_RESULT_HOOK (SendResultData_Collabo025_026, ASLR (0x140179A00));
SEND_RESULT_HOOK (SendResultData_AprilFool, ASLR (0x140177800));
CHANGE_RESULT_SIZE_HOOK (ChangeResultDataSize_Enso, ASLR (0x140180074), rax);
CHANGE_RESULT_SIZE_HOOK (ChangeResultDataSize_AI, ASLR (0x140173774), rax);
CHANGE_RESULT_SIZE_HOOK (ChangeResultDataSize_Collabo025_026, ASLR (0x140178244), rax);
CHANGE_RESULT_SIZE_HOOK (ChangeResultDataSize_AprilFool, ASLR (0x140176044), rax);
CHANGE_RESULT_INDEX_HOOK (ChangeResultDataIndex_Enso, ASLR (0x14018074B), rax, 0x34, 0x07);
CHANGE_RESULT_INDEX_HOOK (ChangeResultDataIndex_AI, ASLR (0x140173EDD), r13, 0x24, 0x08);
CHANGE_RESULT_INDEX_HOOK (ChangeResultDataIndex_Collabo025_026, ASLR (0x1401789AD), r13, 0x24, 0x08);
CHANGE_RESULT_INDEX_HOOK (ChangeResultDataIndex_AprilFool, ASLR (0x140176716), rax, 0x34, 0x06);

MID_HOOK (CountLockedCrown, ASLR (0x1403F2A25), SafetyHookContext &ctx) {
    LogMessage (LogLevel::HOOKS, "CountLockedCrown was called");
    ctx.r15 |= 1;
}

int loaded_fail_count = 0;
FAST_HOOK (char, LoadedBankAll, ASLR (0x1404EC2F0), u64 a1) {
    LogMessage (LogLevel::HOOKS, "LoadedBankAll was called");
    char result = originalLoadedBankAll.fastcall<char> (a1);
    if (result) {
        loaded_fail_count = 0;
        return 1;
    } else if (loaded_fail_count > 60) {
        LogMessage (LogLevel::WARN, "LoadBankAll Guardian!!!");
        loaded_fail_count = 0;
        return 1;
    } else {
        loaded_fail_count += 1;
        return 0;
    }
}

float soundRate = 1.0F;
FAST_HOOK (i32, SetMasterVolumeSpeaker, ASLR (0x140160330), i32 a1) {
    LogMessage (LogLevel::HOOKS, "SetMasterVolumeSpeaker was called");
    patches::Audio::SetVolumeRate (a1 <= 100 ? 1.0f : a1 / 100.0f);
    return originalSetMasterVolumeSpeaker.fastcall<i32> (a1 > 100 ? 100 : a1);
}

MID_HOOK (AttractDemo, ASLR (0x14045A720), SafetyHookContext &ctx) {
    if (TestMode::ReadTestModeValue (L"AttractDemoItem") == 1) ctx.r14 = 0;
}

// FAST_HOOK (u64, EnsoGameManagerInitialize, ASLR (0x1400E2520), u64 a1, u64 a2, u64 a3) {
//     LogMessage (LogLevel::DEBUG, "Begin EnsoGameManagerInitialize");
//     u64 result = originalEnsoGameManagerInitialize.fastcall<u64> (a1, a2, a3);
//     LogMessage (LogLevel::DEBUG, "End EnsoGameManagerInitialize result={}", result);
//     return result;
// }

// FAST_HOOK (u64, EnsoGameManagerLoading, ASLR (0x1400E2750), u64 a1, u64 a2, u64 a3) {
//     LogMessage (LogLevel::DEBUG, "Begin EnsoGameManagerLoading");
//     u64 result = originalEnsoGameManagerLoading.fastcall<u64> (a1, a2, a3);
//     LogMessage (LogLevel::DEBUG, "End EnsoGameManagerLoading result={}", result);
//     return result;
// }

// FAST_HOOK (bool, EnsoGameManagerPreparing, ASLR (0x1400E2990), u64 a1, u64 a2, u64 a3) {
//     LogMessage (LogLevel::DEBUG, "Begin EnsoGameManagerPreparing");    
//     bool result = originalEnsoGameManagerPreparing.fastcall<u64> (a1, a2, a3);  // crashes here
//     LogMessage (LogLevel::DEBUG, "End EnsoGameManagerPreparing result={}", result);
//     return result;
// }

// FAST_HOOK (u64, EnsoGraphicManagerPreparing, ASLR (0x1400F0AB0), u64 a1, u64 a2, u64 a3) {
//     LogMessage (LogLevel::DEBUG, "Begin EnsoGraphicManagerPreparing");    
//     u64 result = originalEnsoGraphicManagerPreparing.fastcall<u64> (a1, a2, a3);
//     LogMessage (LogLevel::DEBUG, "End EnsoGraphicManagerPreparing result={}", result);
//     return result;
// }


// FAST_HOOK (u64, EnsoGameManagerStart, ASLR (0x1400E2A10), u64 a1, u64 a2, u64 a3) {
//     LogMessage (LogLevel::DEBUG, "Begin EnsoGameManagerStart");
//     u64 result = originalEnsoGameManagerStart.fastcall<u64> (a1, a2, a3);
//     LogMessage (LogLevel::DEBUG, "End EnsoGameManagerStart result={}", result);
//     return result;
// }

// FAST_HOOK (u64, EnsoGameManagerChechEnsoEnd, ASLR (0x1400E2A10), u64 a1, u64 a2, u64 a3) {
//     LogMessage (LogLevel::DEBUG, "Begin EnsoGameManagerChechEnsoEnd");
//     u64 result = originalEnsoGameManagerChechEnsoEnd.fastcall<u64> (a1, a2, a3);
//     LogMessage (LogLevel::DEBUG, "End EnsoGameManagerChechEnsoEnd result={}", result);
//     return result;
// }

// HOOK (DWORD*, AcquireMostCompatibleDisplayMode, ASLR (0x14064C870), i64 a1, DWORD *a2, DWORD *a3) {
//     LogMessage (LogLevel::DEBUG, "AcquireMostCompatibleDisplayMode {:d} {:d} {:f} {:f}", a3[0], a3[1], (float)(int)a3[2], (float)(int)a3[3]);
//     a3[2] = (DWORD)(int)120.0f;
//     LogMessage (LogLevel::DEBUG, "AcquireMostCompatibleDisplayMode {:d} {:d} {:f} {:f}", a3[0], a3[1], (float)(int)a3[2], (float)(int)a3[3]);
//     return originalAcquireMostCompatibleDisplayMode (a1, a2, a3);
// }

constexpr i32 datatableBufferSize = 1024 * 1024 * 12;
uint8_t *datatableBuffer[3] = { nullptr };
std::vector<SafetyHookMid> datatable_patch = {};
#define DATATABLE_PATCH_REGISTER(location, reg, value, skip) { datatable_patch.push_back(safetyhook::create_mid(location, [](SafetyHookContext &ctx) {ctx.reg = (uintptr_t)(value); ctx.rip = location + skip;})); }
void
ReplaceDatatableBufferAddresses () {
    LogMessage (LogLevel::INFO, "Set Datatable Size to 12MB");
    for (int i = 0; i < 3; i ++) datatableBuffer[i] = (uint8_t *)malloc (datatableBufferSize);
    DATATABLE_PATCH_REGISTER (ASLR (0x1400ABE26), r8, datatableBufferSize, 6);
    DATATABLE_PATCH_REGISTER (ASLR (0x1400ABE3A), r8, datatableBufferSize, 6);
    DATATABLE_PATCH_REGISTER (ASLR (0x1400ABF79), r8, datatableBufferSize, 6);
    DATATABLE_PATCH_REGISTER (ASLR (0x1400ABE40), rcx, datatableBuffer[0], 7);
    DATATABLE_PATCH_REGISTER (ASLR (0x1400ABEB1), rdi, datatableBuffer[0], 7);
    DATATABLE_PATCH_REGISTER (ASLR (0x1400ABEDB), rdi, datatableBuffer[0], 7);
    DATATABLE_PATCH_REGISTER (ASLR (0x1400ABF4C), rdi, datatableBuffer[0], 7);
    DATATABLE_PATCH_REGISTER (ASLR (0x1400ABE2C), rcx, datatableBuffer[1], 7);
    DATATABLE_PATCH_REGISTER (ASLR (0x1400ABF5B), rcx, datatableBuffer[1], 7);
    DATATABLE_PATCH_REGISTER (ASLR (0x1400ABF8E), r8 , datatableBuffer[1], 7);
    DATATABLE_PATCH_REGISTER (ASLR (0x1400ABF7F), rcx, datatableBuffer[2], 7);
    DATATABLE_PATCH_REGISTER (ASLR (0x1400ABF95), rcx, datatableBuffer[2], 7);
    DATATABLE_PATCH_REGISTER (ASLR (0x1400ABFBF), rdx, datatableBuffer[2], 7);
}

void
Init () {
    LogMessage (LogLevel::INFO, "Init JPN39 patches");
    i32 xRes          = 1920;
    i32 yRes          = 1080;
    bool vsync        = false;
    bool unlockSongs  = true;
    bool chsPatch     = false;
    bool useLayeredfs = false;

    auto configPath = std::filesystem::current_path () / "config.toml";
    std::unique_ptr<toml_table_t, void (*) (toml_table_t *)> config_ptr (openConfig (configPath), toml_free);
    if (config_ptr) {
        if (auto patches = openConfigSection (config_ptr.get (), "patches")) {
            unlockSongs = readConfigBool (patches, "unlock_songs", unlockSongs);
            if (auto jpn39 = openConfigSection (patches, "jpn39")) {
                chsPatch    = readConfigBool (jpn39, "chs_patch", chsPatch);
            }
        }

        if (auto graphics = openConfigSection (config_ptr.get (), "graphics")) {
            if (auto res = openConfigSection (graphics, "res")) {
                xRes = static_cast<i32> (readConfigInt (res, "x", xRes));
                yRes = static_cast<i32> (readConfigInt (res, "y", yRes));
            }
            vsync = readConfigBool (graphics, "vsync", vsync);
        }

        if (auto layeredfs = openConfigSection (config_ptr.get (), "layeredfs")) useLayeredfs = readConfigBool (layeredfs, "enabled", useLayeredfs);
    }

    // Hook to get AppAccessor and ComponentAccessor
    INSTALL_FAST_HOOK (DeviceCheck);
    INSTALL_FAST_HOOK (luaL_newstate);
    // INSTALL_HOOK (AcquireMostCompatibleDisplayMode);

    // Apply common config patch
    WRITE_MEMORY (ASLR (0x140494533), i32, xRes);
    WRITE_MEMORY (ASLR (0x14049453A), i32, yRes);
    if (!vsync) WRITE_MEMORY (ASLR (0x14064C7E9), u8, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x90);

    // Bypass errors
    WRITE_MEMORY (ASLR (0x140041A00), u8, 0xC3);

    // Use TLS v1.2
    WRITE_MEMORY (ASLR (0x140580459), u8, 0x10);

    // Disable F/G check
    WRITE_MEMORY (ASLR (0x140CD4858), char, "./");

    // Move various files to current directory
    WRITE_MEMORY (ASLR (0x140C8AB60), char, "./"); // F:/
    WRITE_MEMORY (ASLR (0x140C8AB5C), char, "./"); // G:/
    WRITE_MEMORY (ASLR (0x140CD4DC0), char, ".\\Setting1.bin");
    WRITE_MEMORY (ASLR (0x140CD4DB0), char, ".\\Setting2.bin");
    WRITE_MEMORY (ASLR (0x140C95B40), char, ".\\TournamentData\\PlayData\\TournamentPlayData.dat");
    WRITE_MEMORY (ASLR (0x140C95B78), char, ".\\TournamentData\\PlayData\\TournamentPlayData.dat");
    WRITE_MEMORY (ASLR (0x140C95BB0), char, ".\\TournamentData\\InfoData\\TournamentInfoData.dat");
    WRITE_MEMORY (ASLR (0x140C95BE8), char, ".\\TournamentData\\InfoData\\TournamentInfoData.dat");
    WRITE_MEMORY (ASLR (0x140CC0508), char, ".\\Garmc\\BillingData\\GarmcBillingData.dat");
    WRITE_MEMORY (ASLR (0x140CC0538), char, ".\\Garmc\\BillingData\\GarmcBillingData.dat");
    WRITE_MEMORY (ASLR (0x140CC0660), char, ".\\Garmc\\BillingData\\GarmcOBillingData.dat");
    WRITE_MEMORY (ASLR (0x140CC0690), char, ".\\Garmc\\BillingData\\GarmcOBillingData.dat");
    WRITE_MEMORY (ASLR (0x140CC05E0), char, ".\\Garmc\\BillingNetIdLocationId\\GarmcBillingNetIdLocationId.dat");
    WRITE_MEMORY (ASLR (0x140CC0620), char, ".\\Garmc\\BillingNetIdLocationId\\GarmcBillingNetIdLocationId.dat");
    WRITE_MEMORY (ASLR (0x140CC0830), char, ".\\Garmc\\BillingNetIdLocationId\\GarmcOBillingNetIdLocationId.dat");
    WRITE_MEMORY (ASLR (0x140CC0880), char, ".\\Garmc\\BillingNetIdLocationId\\GarmcOBillingNetIdLocationId.dat");
    WRITE_MEMORY (ASLR (0x140CC06C0), char, ".\\Garmc\\ErrorLogData\\GarmcErrorLogData.dat");
    WRITE_MEMORY (ASLR (0x140CC06F0), char, ".\\Garmc\\ErrorLogData\\GarmcErrorLogData.dat");
    WRITE_MEMORY (ASLR (0x140CC0580), char, ".\\Garmc\\ErrorLogData\\GarmcOErrorLogData.dat");
    WRITE_MEMORY (ASLR (0x140CC05B0), char, ".\\Garmc\\ErrorLogData\\GarmcOErrorLogData.dat");

    // Remove datatable size limit
    ReplaceDatatableBufferAddresses ();

    // Unlock Songs
    TestMode::RegisterItem (
        L"<select-item label=\"UNLOCK SONGS\" param-offset-x=\"35\" replace-text=\"0:OFF, 1:ON, "
        L"2:FORCE\" group=\"Setting\" id=\"ModUnlockSongs\" max=\"2\" min=\"0\" default=\"1\"/>",
        [&]() { 
            INSTALL_FAST_HOOK (IsSongRelease); 
            INSTALL_FAST_HOOK (IsSongReleasePlayer); 
            INSTALL_MID_HOOK (DifficultyPanelCrown); 
        }
    );
    // Freeze Timer
    TestMode::RegisterItem (
        L"<select-item label=\"FREEZE TIMER\" param-offset-x=\"35\" replace-text=\"0:OFF, 1:ON\" "
        L"group=\"Setting\" id=\"ModFreezeTimer\" max=\"1\" min=\"0\" default=\"0\"/>",
        [&] { INSTALL_MID_HOOK (FreezeTimer); }
    );
    // Mode Unlock
    // TestMode::Menu *modeUnlock = TestMode::CreateMenu (L"MODE UNLOCK", L"ModeUnlockMenu");
    TestMode::RegisterItem (
        L"<select-item label=\"KIMETSU MODE\" param-offset-x=\"35\" replace-text=\"0:DEFAULT, 1:ENABLE, "
        L"2:CARD ONLY\" group=\"Setting\" id=\"ModModeCollabo024\" max=\"1\" min=\"0\" default=\"0\"/>",
        [&] { INSTALL_FAST_HOOK (AvailableMode_Collabo024); }//, modeUnlock
    );
    TestMode::RegisterItem (
        L"<select-item label=\"ONE PIECE MODE\" param-offset-x=\"35\" replace-text=\"0:DEFAULT, "
        L"1:ENABLE, 2:CARD ONLY\" group=\"Setting\" id=\"ModModeCollabo025\" max=\"1\" min=\"0\" default=\"0\"/>",
        [&] { INSTALL_FAST_HOOK (AvailableMode_Collabo025); }//, modeUnlock
    );
    TestMode::RegisterItem (
        L"<select-item label=\"AI SOSHINA MODE\" param-offset-x=\"35\" replace-text=\"0:DEFAULT, "
        L"1:ENABLE, 2:CARD ONLY\" group=\"Setting\" id=\"ModModeCollabo026\" max=\"1\" min=\"0\" default=\"0\"/>",
        [&] { INSTALL_FAST_HOOK (AvailableMode_Collabo026); }//, modeUnlock
    );
    TestMode::RegisterItem (
        L"<select-item label=\"AOHARU MODE\" param-offset-x=\"35\" replace-text=\"0:DEFAULT, 1:ENABLE, "
        L"2:CARD ONLY\" group=\"Setting\" id=\"ModModeAprilFool001\" max=\"1\" min=\"0\" default=\"0\"/>",
        [&] { INSTALL_FAST_HOOK (AvailableMode_AprilFool001); }//, modeUnlock
    );
    // TestMode::RegisterItem (modeUnlock);
    // InstantResult
    TestMode::RegisterItem (
        L"<select-item label=\"INSTANT RESULT\" param-offset-x=\"35\" replace-text=\"0:OFF, 1:ON\" "
        L"group=\"Setting\" id=\"ModInstantResult\" max=\"1\" min=\"0\" default=\"0\"/>",
        [&] {
            INSTALL_FAST_HOOK (SceneResultInitialize_Enso);
            INSTALL_FAST_HOOK (SceneResultInitialize_AI);
            INSTALL_FAST_HOOK (SceneResultInitialize_Collabo025);
            INSTALL_FAST_HOOK (SceneResultInitialize_Collabo026);
            INSTALL_FAST_HOOK (SceneResultInitialize_AprilFool);
            INSTALL_FAST_HOOK (SendResultData_Enso);
            INSTALL_FAST_HOOK (SendResultData_AI);
            INSTALL_FAST_HOOK (SendResultData_Collabo025_026);
            INSTALL_FAST_HOOK (SendResultData_AprilFool);
            INSTALL_MID_HOOK (ChangeResultDataSize_Enso);
            INSTALL_MID_HOOK (ChangeResultDataSize_AI);
            INSTALL_MID_HOOK (ChangeResultDataSize_Collabo025_026);
            INSTALL_MID_HOOK (ChangeResultDataSize_AprilFool);
            INSTALL_MID_HOOK (ChangeResultDataIndex_Enso);
            INSTALL_MID_HOOK (ChangeResultDataIndex_AI);
            INSTALL_MID_HOOK (ChangeResultDataIndex_Collabo025_026);
            INSTALL_MID_HOOK (ChangeResultDataIndex_AprilFool);
        }
    );
    // Unlimit Volume
    TestMode::RegisterModify (
        L"/root/menu[@id='SoundTestMenu']/layout[@type='Center']/select-item[@id='OutputLevelSpeakerItem']",
        [&] (const pugi::xml_node &node) {
            TestMode::Append (node, L"label", L"*");
            node.attribute (L"max").set_value (L"300");
            node.attribute (L"delta").set_value (L"1");
        },
        [&] () { INSTALL_FAST_HOOK (SetMasterVolumeSpeaker); }
    );
    TestMode::RegisterItemAfter(
        L"/root/menu[@id='OthersMenu']/layout[@type='Center']/select-item[@id='AttractMovieItem']",
        L"<select-item label=\"ATTRACT DEMO\" disable=\"True/LanguageItem:0\" param-offset-x=\"35\" "
        L"replace-text=\"0:ON, 1:OFF\" group=\"Setting\" id=\"AttractDemoItem\" max=\"1\" min=\"0\" default=\"0\"/>",
        [&](){ INSTALL_MID_HOOK (AttractDemo); }
    );

    // Instant Result
    // TestMode::RegisterModify(
    //     L"/root/menu[@id='GameOptionsMenu']/layout[@type='Center']/select-item[@id='NumberOfStageItem']",
    //     [&](pugi::xml_node &node) { TestMode::Append(node, L"label", L"*"); node.attribute(L"max").set_value(L"99"); },
    //     [&](){}
    // );

    // Fix normal song play after passing through silent song
    // INSTALL_MID_HOOK (GenNus3bankId);
    INSTALL_FAST_HOOK (LoadedBankAll);

    // Disable live check
    auto amHandle = reinterpret_cast<u64> (GetModuleHandle ("AMFrameWork.dll"));
    INSTALL_FAST_HOOK_DYNAMIC (AMFWTerminate, reinterpret_cast<void *> (amHandle + 0x42DE0));

    // Move various files to current directory
    WRITE_MEMORY (amHandle + 0x15252, u8, 0xEB); // CreditLogPathA
    WRITE_MEMORY (amHandle + 0x15419, u8, 0xEB); // CreditLogPathB
    WRITE_MEMORY (amHandle + 0x416DA, u8, 0xEB); // ErrorLogPathA
    WRITE_MEMORY (amHandle + 0x41859, u8, 0xEB); // ErrorLogPathB
    WRITE_MEMORY (amHandle + 0x41C21, u8, 0xEB); // CommonLogPathA
    WRITE_MEMORY (amHandle + 0x41DA5, u8, 0xEB); // CommonLogPathB
    WRITE_NOP (amHandle + 0x420F1, 0x05);        // BackupDataPathA
    WRITE_NOP (amHandle + 0x42167, 0x05);        // BackupDataPathB

    // Redirect garmc requests
    auto garmcHandle = reinterpret_cast<u64> (GetModuleHandle ("garmc.dll"));
    INSTALL_FAST_HOOK_DYNAMIC (curl_easy_setopt, reinterpret_cast<void *> (garmcHandle + 0x1FBBB0));
    INSTALL_FAST_HOOK_DYNAMIC (garmc_logger_log, reinterpret_cast<void *> (garmcHandle + 0x13AB70));
}
} // namespace patches::JPN39
