#include "constants.h"
#include "helpers.h"
#include "patches.h"

extern GameVersion gameVersion;

namespace patches::Wordlist {
u64 GetWordInfoAddress;
u64 AddWordInfoAddress;

u64 appAccessor;
RefDataTableManager refDataTableManager;

void
Init () {
    switch (gameVersion) {
    case GameVersion::JPN00: break;
    case GameVersion::JPN08: break;
    case GameVersion::JPN39: {
        GetWordInfoAddress = ASLR (0x1400AA640);
        AddWordInfoAddress = ASLR (0x1400AAA20);
    } break;
    case GameVersion::CHN00: break;
    }
}
void
SetupAccessor (u64 appAccessor, RefDataTableManager refDataTableManager) {
    patches::Wordlist::refDataTableManager = refDataTableManager;
    patches::Wordlist::appAccessor = appAccessor;
}
WordInfo *
GetWordInfo (std::string &key) {
    WordInfo *wordInfo = (WordInfo *)malloc (sizeof(WordInfo));
    wordInfo->key = "";
    wordInfo->text = "";
    wordInfo->fontType = 0;
    if (refDataTableManager != nullptr && appAccessor != 0) {
        u64 dataTableManager = refDataTableManager (appAccessor);
        if (dataTableManager) {
            u64 wordInfoAccessor = *(u64*)(dataTableManager + 32);
            if (wordInfoAccessor) {
                ((char (__fastcall *)(u64, std::string *, WordInfo *))(GetWordInfoAddress))(wordInfoAccessor, &key, wordInfo);
            }
        }
    }
    return wordInfo;
}
void
SetWordInfo (WordInfo *wordInfo) {
    if (wordInfo != nullptr && refDataTableManager != nullptr && appAccessor != 0) {
        u64 dataTableManager = refDataTableManager (appAccessor);
        if (dataTableManager) {
            u64 wordInfoAccessor = *(u64*)(dataTableManager + 32);
            if (wordInfoAccessor) {
                ((u64 (__fastcall *)(u64, WordInfo *))(AddWordInfoAddress))(wordInfoAccessor, wordInfo);
            }
        }
    }
}
}