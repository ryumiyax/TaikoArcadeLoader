#include "constants.h"
#include "helpers.h"
#include "patches.h"

extern GameVersion gameVersion;

namespace patches::Audio {

typedef struct nusc_init_config {
    u32 sample_rate;
    u32 buffer_size;
    u32 device_mode;
    u32 channel_count;
    const char *asio_driver_name;
    bool wasapi_disable_com;
    bool wasapi_exclusive;
    u32 wasapi_exclusive_buffer_size;
    void *wasapi_audioSes;
} nusc_init_config_t;

bool wasapiShared      = true;
bool asio              = false;
std::string asioDriver;

float volumeRate       = 0.0f;

FAST_HOOK_DYNAMIC (i64, NUSCDeviceInit, void *a1, nusc_init_config_t *a2, nusc_init_config_t *a3, void *a4) {
    LogMessage (LogLevel::INFO, std::string ("Device mode is ") + (asio ? "ASIO" : wasapiShared ? "wasapi shared" : "wasapi exclusive"));
    if (asio) LogMessage (LogLevel::INFO, (std::string ("ASIO driver is ") + asioDriver).c_str ());
    a2->device_mode      = asio;
    a2->asio_driver_name = asio ? asioDriver.c_str () : "";
    a2->wasapi_exclusive = asio ? 1 : wasapiShared ? 0 : 1;
    return originalNUSCDeviceInit.fastcall<i64> (a1, a2, a3, a4);
}
FAST_HOOK_DYNAMIC (bool, LoadASIODriver, void *a1, const char *a2) {
    const auto result = originalLoadASIODriver.fastcall<bool> (a1, a2);
    if (!result) {
        LogMessage (LogLevel::ERROR, (std::string ("Failed to load ASIO driver ") + asioDriver).c_str ());
        MessageBoxA (nullptr, "Failed to load ASIO driver", nullptr, MB_OK);
        ExitProcess (0);
    }
    return result;
}
FAST_HOOK_DYNAMIC (u64, NuscBusVolume, u64 a1, u64 a2, float a3) {
    if (volumeRate == 0.0f) {
        int value = patches::TestMode::ReadTestModeValue (L"OutputLevelSpeakerItem");
        if (value == -1) return originalNuscBusVolume.fastcall<u64> (a1, a2, a3);
        volumeRate = value <= 100 ? 1.0f : value / 100.0f;
    }
    return originalNuscBusVolume.fastcall<u64> (a1, a2, a3 * volumeRate);
}

void
SetVolumeRate (float rate) {
    volumeRate = rate;
}

void
Init () {
    LogMessage (LogLevel::INFO, "Init Audio patches");

    const auto configPath = std::filesystem::current_path () / "config.toml";
    const std::unique_ptr<toml_table_t, void (*) (toml_table_t *)> config_ptr (openConfig (configPath), toml_free);
    if (config_ptr) {
        if (const auto audio = openConfigSection (config_ptr.get (), "audio")) {
            wasapiShared = readConfigBool (audio, "wasapi_shared", wasapiShared);
            asio         = readConfigBool (audio, "asio", asio);
            asioDriver   = readConfigString (audio, "asio_driver", asioDriver);
        }
    }

    switch (gameVersion) {
    case GameVersion::JPN00: {
        INSTALL_FAST_HOOK_DYNAMIC (NUSCDeviceInit, ASLR (0x140552160));
        INSTALL_FAST_HOOK_DYNAMIC (LoadASIODriver, ASLR (0x14055A950));
        break;
    }
    case GameVersion::JPN08: {
        INSTALL_FAST_HOOK_DYNAMIC (NUSCDeviceInit, ASLR (0x140692E00));
        INSTALL_FAST_HOOK_DYNAMIC (LoadASIODriver, ASLR (0x14069B750));
        break;
    }
    case GameVersion::JPN39: {
        INSTALL_FAST_HOOK_DYNAMIC (NUSCDeviceInit, ASLR (0x1407C8620));
        INSTALL_FAST_HOOK_DYNAMIC (LoadASIODriver, ASLR (0x1407D0F70));
        INSTALL_FAST_HOOK_DYNAMIC (NuscBusVolume,  ASLR (0x1407B1C30));
        break;
    }
    case GameVersion::CHN00: {
        INSTALL_FAST_HOOK_DYNAMIC (NUSCDeviceInit, ASLR (0x140777F70));
        INSTALL_FAST_HOOK_DYNAMIC (LoadASIODriver, ASLR (0x1407808C0));
        break;
    }
    default: {
        break;
    }
    }
}
} // namespace patches::Audio
