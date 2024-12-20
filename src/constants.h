#pragma once
#include <xxhash.h>

enum class GameVersion : XXH64_hash_t {
    UNKNOWN = 0,
    JPN00   = 0x4C07355966D815FB,
    JPN08   = 0x67C0F3042746D488,
    JPN39   = 0x49F643ADB6B18705,
    CHN00   = 0xA7EE39F2CC2C57C8,
};

enum StatusType {
    CardStatus = 1,
    QrStatus   = 2
};