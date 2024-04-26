#include "../patches.h"
#include "helpers.h"
#include <safetyhook.hpp>

namespace patches::JPN39 {

HOOK_DYNAMIC (char, __fastcall, AMFWTerminate, i64) { return 0; }

HOOK_DYNAMIC (i64, __fastcall, curl_easy_setopt, i64 a1, i64 a2, i64 a3, i64 a4, i64 a5) {
    // printf ("garmc curl_easy_setopt\n");
    originalcurl_easy_setopt (a1, 64, 0, 0, 0);
    originalcurl_easy_setopt (a1, 81, 0, 0, 0);
    return originalcurl_easy_setopt (a1, a2, a3, a4, a5);
}

const i32 datatableBufferSize = 1024 * 1024 * 12;
safetyhook::Allocation datatableBuffer1;
safetyhook::Allocation datatableBuffer2;
safetyhook::Allocation datatableBuffer3;
const std::vector<uintptr_t> datatableBuffer1Addresses = {0x1400ABE40, 0x1400ABEB1, 0x1400ABEDB, 0x1400ABF4C};
const std::vector<uintptr_t> datatableBuffer2Addresses = {0x1400ABE2C, 0x1400ABF5B, 0x1400ABF8E};
const std::vector<uintptr_t> datatableBuffer3Addresses = {0x1400ABF7F, 0x1400ABF95, 0x1400ABFBF};
const std::vector<uintptr_t> memsetSizeAddresses       = {0x1400ABE26, 0x1400ABE3A, 0x1400ABF79};

void
AllocateStaticBufferNear (void *target_address, size_t size, safetyhook::Allocation *newBuffer) {
    auto allocator                = safetyhook::Allocator::global ();
    std::vector desired_addresses = {(uint8_t *)target_address};
    auto allocation_result        = allocator->allocate_near (desired_addresses, size);
    if (allocation_result.has_value ()) *newBuffer = std::move (*allocation_result);
}

void
ReplaceLeaBufferAddress (const std::vector<uintptr_t> &bufferAddresses, void *newBufferAddress) {
    for (auto bufferAddress : bufferAddresses) {
        uintptr_t lea_instruction_dst = ASLR (bufferAddress) + 3;
        uintptr_t lea_instruction_end = ASLR (bufferAddress) + 7;
        intptr_t offset               = (intptr_t)newBufferAddress - lea_instruction_end;
        WRITE_MEMORY (lea_instruction_dst, i32, (i32)offset);
    }
}

void
Init () {
    i32 xRes         = 1920;
    i32 yRes         = 1080;
    bool vsync       = false;
    bool unlockSongs = true;

    auto configPath = std::filesystem::current_path () / "config.toml";
    std::unique_ptr<toml_table_t, void (*) (toml_table_t *)> config_ptr (openConfig (configPath), toml_free);
    if (config_ptr) {
        auto patches = openConfigSection (config_ptr.get (), "patches");
        if (patches) {
            auto res = openConfigSection (patches, "res");
            if (res) {
                xRes = readConfigInt (res, "x", xRes);
                yRes = readConfigInt (res, "y", yRes);
            }
            vsync       = readConfigBool (patches, "vsync", vsync);
            unlockSongs = readConfigBool (patches, "unlock_songs", unlockSongs);
        }
    }

    // Apply common config patch
    WRITE_MEMORY (ASLR (0x140494533), i32, xRes);
    WRITE_MEMORY (ASLR (0x14049453A), i32, yRes);
    if (!vsync) WRITE_MEMORY (ASLR (0x14064C7E9), u8, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x90);
    if (unlockSongs) WRITE_MEMORY (ASLR (0x1403F45CF), u8, 0xB0, 0x01);

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
    {
        for (auto address : memsetSizeAddresses)
            WRITE_MEMORY (ASLR (address) + 2, i32, datatableBufferSize);

        auto bufferBase = MODULE_HANDLE - 0x03000000;
        AllocateStaticBufferNear ((void *)bufferBase, datatableBufferSize, &datatableBuffer1);
        bufferBase += datatableBufferSize;
        AllocateStaticBufferNear ((void *)bufferBase, datatableBufferSize, &datatableBuffer2);
        bufferBase += datatableBufferSize;
        AllocateStaticBufferNear ((void *)bufferBase, datatableBufferSize, &datatableBuffer3);

        ReplaceLeaBufferAddress (datatableBuffer1Addresses, datatableBuffer1.data ());
        ReplaceLeaBufferAddress (datatableBuffer2Addresses, datatableBuffer2.data ());
        ReplaceLeaBufferAddress (datatableBuffer3Addresses, datatableBuffer3.data ());
    }

    // Disable live check
    auto amHandle = (u64)GetModuleHandle ("AMFrameWork.dll");
    INSTALL_HOOK_DYNAMIC (AMFWTerminate, (void *)(amHandle + 0x42DE0));

    // Move various files to current directory
    WRITE_MEMORY (amHandle + 0x15252, u8, 0xEB);                         // CreditLogPathA
    WRITE_MEMORY (amHandle + 0x15419, u8, 0xEB);                         // CreditLogPathB
    WRITE_MEMORY (amHandle + 0x416DA, u8, 0xEB);                         // ErrorLogPathA
    WRITE_MEMORY (amHandle + 0x41859, u8, 0xEB);                         // ErrorLogPathB
    WRITE_MEMORY (amHandle + 0x41C21, u8, 0xEB);                         // CommonLogPathA
    WRITE_MEMORY (amHandle + 0x41DA5, u8, 0xEB);                         // CommonLogPathB
    WRITE_MEMORY (amHandle + 0x420F1, u8, 0x90, 0x90, 0x90, 0x90, 0x90); // BackupDataPathA
    WRITE_MEMORY (amHandle + 0x42167, u8, 0x90, 0x90, 0x90, 0x90, 0x90); // BackupDataPathB

    // Redirect garmc requests
    auto garmcHandle = (u64)GetModuleHandle ("garmc.dll");
    INSTALL_HOOK_DYNAMIC (curl_easy_setopt, (void *)(garmcHandle + 0x1FBBB0));

    patches::Audio::Init ();
    patches::Qr::Init ();
    patches::AmAuth::Init ();
}
} // namespace patches::JPN39