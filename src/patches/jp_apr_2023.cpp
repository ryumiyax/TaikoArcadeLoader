#include "helpers.h"
#include "patches.h"

namespace patches::JP_APR_2023 {

HOOK_DYNAMIC (char, __fastcall, AMFWTerminate, i64) { return 0; }

HOOK_DYNAMIC (i64, __fastcall, curl_easy_setopt, i64 a1, i64 a2, i64 a3, i64 a4, i64 a5) {
	// printf ("garmc curl_easy_setopt\n");
	originalcurl_easy_setopt (a1, 64, 0, 0, 0);
	originalcurl_easy_setopt (a1, 81, 0, 0, 0);
	return originalcurl_easy_setopt (a1, a2, a3, a4, a5);
}

void
Init () {
	i32 xRes         = 1920;
	i32 yRes         = 1080;
	bool vsync       = false;
	bool sharedAudio = true;
	bool unlockSongs = true;

	auto configPath      = std::filesystem::current_path () / "config.toml";
	toml_table_t *config = openConfig (configPath);
	if (config) {
		auto patches = openConfigSection (config, "patches");
		if (patches) {
			auto res = openConfigSection (patches, "res");
			if (res) {
				xRes = readConfigInt (res, "x", xRes);
				yRes = readConfigInt (res, "y", yRes);
			}
			vsync       = readConfigBool (patches, "vsync", vsync);
			sharedAudio = readConfigBool (patches, "shared_audio", sharedAudio);
			unlockSongs = readConfigBool (patches, "unlock_songs", unlockSongs);
		}
		toml_free (config);
	}

	// Apply common config patch
	WRITE_MEMORY (ASLR (0x140494533), i32, xRes);
	WRITE_MEMORY (ASLR (0x14049453A), i32, yRes);
	if (!vsync) WRITE_MEMORY (ASLR (0x14064C7E9), u8, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x90);
	if (sharedAudio) WRITE_MEMORY (ASLR (0x1407C8637), u8, 0xEB);
	if (unlockSongs) WRITE_MEMORY (ASLR (0x1403F45CF), u8, 0xB0, 0x01);

	// Bypass Errors
	WRITE_MEMORY (ASLR (0x140041A00), u8, 0xC3);

	// Use TLS v1.2
	WRITE_MEMORY (ASLR (0x140580459), u8, 0x10);

	// Disable F/G Check
	WRITE_MEMORY (ASLR (0x140CD4858), char, "./");

	// Move various files to current dir
	WRITE_MEMORY (ASLR (0x140C8AB5C), char, "./");
	WRITE_MEMORY (ASLR (0x140C8AB60), char, "./");
	WRITE_MEMORY (ASLR (0x140CD4DC0), char, ".\\Setting1.bin");
	WRITE_MEMORY (ASLR (0x140CD4DB0), char, ".\\Setting2.bin");
	WRITE_MEMORY (ASLR (0x140C95B40), char, ".\\TournamentData\\PlayData\\TournamentPlayData.dat");
	WRITE_MEMORY (ASLR (0x140C95BB0), char, ".\\TournamentData\\InfoData\\TournamentInfoData.dat");
	WRITE_MEMORY (ASLR (0x140CC0508), char, ".\\Garmc\\BillingData\\GarmcBillingData.dat");
	WRITE_MEMORY (ASLR (0x140CC0580), char, ".\\Garmc\\ErrorLogData\\GarmcOErrorLogData.dat");
	WRITE_MEMORY (ASLR (0x140CC05E0), char, ".\\Garmc\\BillingNetIdLocationId\\GarmcBillingNetIdLocationId.dat");
	WRITE_MEMORY (ASLR (0x140CC0660), char, ".\\Garmc\\BillingData\\GarmcOBillingData.dat");
	WRITE_MEMORY (ASLR (0x140CC06C0), char, ".\\Garmc\\ErrorLogData\\GarmcErrorLogData.dat");
	WRITE_MEMORY (ASLR (0x140CC0830), char, ".\\Garmc\\BillingNetIdLocationId\\GarmcOBillingNetIdLocationId.dat");
	WRITE_MEMORY (ASLR (0x140C95B78), char, ".\\TournamentData\\PlayData\\TournamentPlayData.dat");
	WRITE_MEMORY (ASLR (0x140C95BE8), char, ".\\TournamentData\\InfoData\\TournamentInfoData.dat");
	WRITE_MEMORY (ASLR (0x140CC0538), char, ".\\Garmc\\BillingData\\GarmcBillingData.dat");
	WRITE_MEMORY (ASLR (0x140CC05B0), char, ".\\Garmc\\ErrorLogData\\GarmcOErrorLogData.dat");
	WRITE_MEMORY (ASLR (0x140CC0620), char, ".\\Garmc\\BillingNetIdLocationId\\GarmcBillingNetIdLocationId.dat");
	WRITE_MEMORY (ASLR (0x140CC0690), char, ".\\Garmc\\BillingData\\GarmcOBillingData.dat");
	WRITE_MEMORY (ASLR (0x140CC06F0), char, ".\\Garmc\\ErrorLogData\\GarmcErrorLogData.dat");
	WRITE_MEMORY (ASLR (0x140CC0880), char, ".\\Garmc\\BillingNetIdLocationId\\GarmcOBillingNetIdLocationId.dat");

	// Disable live check
	auto amHandle = (u64)GetModuleHandle ("AMFrameWork.dll");
	INSTALL_HOOK_DYNAMIC (AMFWTerminate, (void *)(amHandle + 0x42DE0));

	// Redirect garmc requests
	auto garmcHandle = (u64)GetModuleHandle ("garmc.dll");
	INSTALL_HOOK_DYNAMIC (curl_easy_setopt, (void *)(garmcHandle + 0x1FBBB0));

	patches::Qr::Init ();
	patches::AmAuth::Init ();
}
} // namespace patches::JP_APR_2023