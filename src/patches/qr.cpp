#include "constants.h"
#include "helpers.h"
#include "poll.h"
#include <ReadBarcode.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include "stb_image.h"

extern GameVersion gameVersion;
extern Keybindings CARD_INSERT_1;
extern Keybindings CARD_INSERT_2;
extern Keybindings QR_DATA_READ;
extern Keybindings QR_IMAGE_READ;
extern char accessCode1[21];
extern char accessCode2[21];

namespace patches::Qr {

enum class State { Ready, CopyWait };
enum class Mode { Card, Data, Image };
State gState = State::Ready;
Mode gMode   = Mode::Card;
std::string accessCode;
bool qrEnabled = true;

HOOK_DYNAMIC (char, __fastcall, qrInit, i64) { return 1; }
HOOK_DYNAMIC (char, __fastcall, qrRead, i64 a1) {
	*(DWORD *)(a1 + 40) = 1;
	*(DWORD *)(a1 + 16) = 1;
	*(BYTE *)(a1 + 112) = 0;
	return 1;
}
HOOK_DYNAMIC (char, __fastcall, qrClose, i64) { return 1; }
HOOK_DYNAMIC (i64, __fastcall, callQrUnknown, i64) { return 1; }
HOOK_DYNAMIC (bool, __fastcall, Send1, i64, const void *, i64) { return true; }
HOOK_DYNAMIC (bool, __fastcall, Send2, i64, char) { return true; }
HOOK_DYNAMIC (bool, __fastcall, Send3, i64 a1) {
	*(WORD *)(a1 + 88) = 0;
	*(BYTE *)(a1 + 90) = 0;
	return true;
}
HOOK_DYNAMIC (bool, __fastcall, Send4, i64 a1) {
	*(BYTE *)(a1 + 88) = 1;
	*(i64 *)(a1 + 32)  = *(i64 *)(a1 + 24);
	*(WORD *)(a1 + 89) = 0;
	return true;
}
HOOK_DYNAMIC (i64, __fastcall, copy_data, i64, void *dest, int length) {
	if (gState == State::CopyWait) {
		std::cout << "Copy data, length: " << length << std::endl;

		auto configPath = std::filesystem::current_path () / "config.toml";
		std::unique_ptr<toml_table_t, void (*) (toml_table_t *)> config_ptr (openConfig (configPath), toml_free);

		if (gMode == Mode::Card) {
			memcpy (dest, accessCode.c_str (), accessCode.size () + 1);
			gState = State::Ready;
			return accessCode.size () + 1;
		} else if (gMode == Mode::Data) {
			std::string serial = "";
			u16 type           = 0;
			std::vector<i64> songNoes;

			if (config_ptr) {
				auto qr = openConfigSection (config_ptr.get (), "qr");
				if (qr) {
					auto data = openConfigSection (qr, "data");
					if (data) {
						serial   = readConfigString (data, "serial", "");
						type     = readConfigInt (data, "type", 0);
						songNoes = readConfigIntArray (data, "song_no", songNoes);
					}
				}
			}

			BYTE serial_length           = (BYTE)serial.size ();
			std::vector<BYTE> byteBuffer = {0x53, 0x31, 0x32, 0x00, 0x00, 0xFF, 0xFF, serial_length, 0x01, 0x00};

			for (char c : serial)
				byteBuffer.push_back ((BYTE)c);

			if (type == 5) {
				std::vector<BYTE> folderData = {0xFF, 0xFF};

				folderData.push_back (songNoes.size () * 2);

				folderData.push_back ((u8)(type & 0xFF));
				folderData.push_back ((u8)((type >> 8) & 0xFF));

				for (u16 songNo : songNoes) {
					folderData.push_back ((u8)(songNo & 0xFF));
					folderData.push_back ((u8)((songNo >> 8) & 0xFF));
				}

				for (auto c : folderData)
					byteBuffer.push_back (c);
			}

			byteBuffer.push_back (0xEE);
			byteBuffer.push_back (0xFF);

			memcpy (dest, byteBuffer.data (), byteBuffer.size ());
			gState = State::Ready;
			return byteBuffer.size ();
		} else {
			std::string imagePath = "";

			if (config_ptr) {
				auto qr = openConfigSection (config_ptr.get (), "qr");
				if (qr) imagePath = readConfigString (qr, "image_path", "");
			}

			std::u8string u8PathStr (imagePath.begin (), imagePath.end ());
			std::filesystem::path u8Path (u8PathStr);
			if (!std::filesystem::is_regular_file (u8Path)) {
				std::cerr << "Failed to open image: " << u8Path.string () << " (file not found)"
				          << "\n";
				gState = State::Ready;
				return 0;
			}

			int width, height, channels;
			std::unique_ptr<stbi_uc, void (*) (void *)> buffer (stbi_load (u8Path.string ().c_str (), &width, &height, &channels, 3), stbi_image_free);
			if (!buffer) {
				std::cerr << "Failed to read image: " << u8Path << " (" << stbi_failure_reason () << ")"
				          << "\n";
				gState = State::Ready;
				return 0;
			}

			ZXing::ImageView image{buffer.get (), width, height, ZXing::ImageFormat::RGB};
			auto result = ReadBarcode (image);
			if (!result.isValid ()) {
				std::cerr << "Failed to read qr: " << imagePath << " (" << ToString (result.error ()) << ")"
				          << "\n";
				gState = State::Ready;
				return 0;
			}

			std::cout << "Valid" << std::endl;
			auto byteData = result.bytes ();
			std::cout << ZXing::ToHex (byteData) << std::endl;
			auto dataSize = byteData.size ();

			memcpy (dest, byteData.data (), dataSize);
			gState = State::Ready;
			return dataSize;
		}
	}
	return 0;
}

void
Update () {
	if (!qrEnabled) return;
	if (gState == State::Ready) {
		if (IsButtonTapped (CARD_INSERT_1)) {
			if (gameVersion != GameVersion::CN_JUN_2023) return;

			std::cout << "Insert" << std::endl;
			accessCode = "BNTTCNID";
			accessCode += accessCode1;
			gState = State::CopyWait;
			gMode  = Mode::Card;
		} else if (IsButtonTapped (CARD_INSERT_2)) {
			if (gameVersion != GameVersion::CN_JUN_2023) return;

			std::cout << "Insert" << std::endl;
			accessCode = "BNTTCNID";
			accessCode += accessCode2;
			gState = State::CopyWait;
			gMode  = Mode::Card;
		} else if (IsButtonTapped (QR_DATA_READ)) {
			std::cout << "Insert" << std::endl;
			gState = State::CopyWait;
			gMode  = Mode::Data;
		} else if (IsButtonTapped (QR_IMAGE_READ)) {
			std::cout << "Insert" << std::endl;
			gState = State::CopyWait;
			gMode  = Mode::Image;
		}
	}
}

void
Init () {
	auto configPath = std::filesystem::current_path () / "config.toml";
	std::unique_ptr<toml_table_t, void (*) (toml_table_t *)> config_ptr (openConfig (configPath), toml_free);
	if (!config_ptr) {
		std::cerr << "[Init] Config file not found" << std::endl;
		return;
	}
	auto qr = openConfigSection (config_ptr.get (), "qr");
	if (!qr) {
		std::cerr << "[Init] QR config section not found! QR emulation disabled" << std::endl;
		qrEnabled = false;
		return;
	}
	qrEnabled = readConfigBool (qr, "enabled", true);
	if (!qrEnabled) {
		std::cout << "[Init] QR emulation disabled" << std::endl;
		return;
	}
	SetConsoleOutputCP (CP_UTF8);
	auto amHandle = (u64)GetModuleHandle ("AMFrameWork.dll");
	switch (gameVersion) {
	case GameVersion::JP_NOV_2020: {
		INSTALL_HOOK_DYNAMIC (qrInit, (LPVOID)(amHandle + 0x1BA00));
		INSTALL_HOOK_DYNAMIC (qrRead, (LPVOID)(amHandle + 0x1BC20));
		INSTALL_HOOK_DYNAMIC (qrClose, (LPVOID)(amHandle + 0x1BBD0));
		INSTALL_HOOK_DYNAMIC (callQrUnknown, (LPVOID)(amHandle + 0xFD40));
		// 08.18 has no Send1
		INSTALL_HOOK_DYNAMIC (Send2, (LPVOID)(amHandle + 0x1C2D0));
		INSTALL_HOOK_DYNAMIC (Send3, (LPVOID)(amHandle + 0x1C260));
		INSTALL_HOOK_DYNAMIC (Send4, (LPVOID)(amHandle + 0x1C220));
		INSTALL_HOOK_DYNAMIC (copy_data, (LPVOID)(amHandle + 0x1C2A0));
		break;
	}
	case GameVersion::JP_APR_2023: {
		INSTALL_HOOK_DYNAMIC (qrInit, (LPVOID)(amHandle + 0x1EDC0));
		INSTALL_HOOK_DYNAMIC (qrRead, (LPVOID)(amHandle + 0x1EFB0));
		INSTALL_HOOK_DYNAMIC (qrClose, (LPVOID)(amHandle + 0x1EF60));
		INSTALL_HOOK_DYNAMIC (callQrUnknown, (LPVOID)(amHandle + 0x11A70));
		INSTALL_HOOK_DYNAMIC (Send1, (LPVOID)(amHandle + 0x1F690));
		INSTALL_HOOK_DYNAMIC (Send2, (LPVOID)(amHandle + 0x1F660));
		INSTALL_HOOK_DYNAMIC (Send3, (LPVOID)(amHandle + 0x1F5F0));
		INSTALL_HOOK_DYNAMIC (Send4, (LPVOID)(amHandle + 0x1F5B0));
		INSTALL_HOOK_DYNAMIC (copy_data, (LPVOID)(amHandle + 0x1F630));
		break;
	}
	case GameVersion::CN_JUN_2023: {
		INSTALL_HOOK_DYNAMIC (qrInit, (LPVOID)(amHandle + 0x161B0));
		INSTALL_HOOK_DYNAMIC (qrRead, (LPVOID)(amHandle + 0x163A0));
		INSTALL_HOOK_DYNAMIC (qrClose, (LPVOID)(amHandle + 0x16350));
		INSTALL_HOOK_DYNAMIC (callQrUnknown, (LPVOID)(amHandle + 0x8F60));
		INSTALL_HOOK_DYNAMIC (Send1, (LPVOID)(amHandle + 0x16A30));
		INSTALL_HOOK_DYNAMIC (Send2, (LPVOID)(amHandle + 0x16A00));
		INSTALL_HOOK_DYNAMIC (Send3, (LPVOID)(amHandle + 0x16990));
		INSTALL_HOOK_DYNAMIC (Send4, (LPVOID)(amHandle + 0x16940));
		INSTALL_HOOK_DYNAMIC (copy_data, (LPVOID)(amHandle + 0x169D0));
		break;
	}
	default: {
		break;
	}
	}
}
} // namespace patches::Qr
