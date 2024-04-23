#pragma once
#include <xxhash.h>

enum class GameVersion : XXH64_hash_t {
	UNKNOWN     = 0,
	JPN08 = 0x67C0F3042746D488,
	JPN39 = 0x49F643ADB6B18705,
	CHN00 = 0xA7EE39F2CC2C57C8,
};