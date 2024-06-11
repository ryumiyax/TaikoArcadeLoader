#include "bnusio.h"
#include "constants.h"
#include "helpers.h"
#include "patches/patches.h"
#include "poll.h"
#include <zlib.h>
#include <tomcrypt.h>
#define POLY 0x82f63b78

GameVersion gameVersion = GameVersion::UNKNOWN;
std::vector<HMODULE> plugins;
u64 song_data_size = 1024 * 1024 * 64;
void *song_data;

std::string server      = "127.0.0.1";
std::string port        = "54430";
std::string chassisId   = "284111080000";
std::string shopId      = "TAIKO ARCADE LOADER";
std::string gameVerNum  = "00.00";
std::string countryCode = "JPN";
char fullAddress[256]   = {'\0'};
char placeId[16]        = {'\0'};
char accessCode1[21]    = "00000000000000000001";
char accessCode2[21]    = "00000000000000000002";
char chipId1[33]        = "00000000000000000000000000000001";
char chipId2[33]        = "00000000000000000000000000000002";
bool autoIME            = false;
bool jpLayout           = false;
bool useLayeredFs       = false;
std::string datatableKey = "0000000000000000000000000000000000000000000000000000000000000000";
std::string fumenKey     = "0000000000000000000000000000000000000000000000000000000000000000";

uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len)
{
    int k;

    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (POLY & (0 - (crc & 1)));
    }
    return ~crc;
}

bool checkCRC(const std::string &path, uint32_t crc){
    if (std::filesystem::exists(path)){
        std::filesystem::path crc_path = path;
        crc_path.replace_extension(".crc");
        std::ifstream crc_file(crc_path, std::ios::binary);
        std::string crc_content((std::istreambuf_iterator<char>(crc_file)), std::istreambuf_iterator<char>());
        return std::stoul(crc_content) != crc;
    }
    return 1;
}

void create_directories(const std::string &path) {
    size_t pos = 0;
    std::string delimiter = "\\";
    std::string current_path;

    while ((pos = path.find(delimiter, pos)) != std::string::npos) {
        current_path = path.substr(0, pos++);
        if (!current_path.empty() && !CreateDirectory(current_path.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            throw std::runtime_error("Error creating directory: " + current_path);
        }
    }
    if (!path.empty() && !CreateDirectory(path.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        throw std::runtime_error("Error creating directory: " + path);
    }
}

void write_file(const std::string &filename, const std::vector<uint8_t> &data, uint32_t original_crc) {
    std::string::size_type pos = filename.find_last_of("\\");
    if (pos != std::string::npos) {
        std::string directory = filename.substr(0, pos);
        create_directories(directory);
    }

    std::filesystem::path crc_path = filename;
    crc_path.replace_extension(".crc");
    std::ofstream crc_file(crc_path.string());
    crc_file << std::to_string(original_crc);
    crc_file.close();

    std::ofstream file(filename, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

std::vector<unsigned char> gzip_compress(const std::vector<unsigned char>& data) {
    z_stream deflate_stream;
    deflate_stream.zalloc = Z_NULL;
    deflate_stream.zfree = Z_NULL;
    deflate_stream.opaque = Z_NULL;
    deflate_stream.avail_in = data.size();
    deflate_stream.next_in = const_cast<Bytef*>(data.data());

    deflateInit2(&deflate_stream, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);

    std::vector<unsigned char> compressed_data;
    compressed_data.resize(deflateBound(&deflate_stream, data.size()));

    deflate_stream.avail_out = compressed_data.size();
    deflate_stream.next_out = compressed_data.data();

    deflate(&deflate_stream, Z_FINISH);
    deflateEnd(&deflate_stream);

    compressed_data.resize(deflate_stream.total_out);
    return compressed_data;
}

// Function to pad data according to PKCS7
std::vector<uint8_t> pad_data(const std::vector<uint8_t> &data, size_t block_size) {
    size_t padding = block_size - (data.size() % block_size);
    std::vector<uint8_t> padded_data = data;
    padded_data.insert(padded_data.end(), padding, static_cast<uint8_t>(padding));
    return padded_data;
}

std::vector<uint8_t> hex_to_bytes(const std::string &hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        uint8_t byte = static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

std::vector<uint8_t> encrypt_file(const std::string &input_file, const std::string &hex_key) {
    // Convert the key from hex to bytes
    std::vector<uint8_t> key = hex_to_bytes(hex_key);

    // Generate the 128 bits IV 
    std::vector<uint8_t> iv(16);
    for (size_t i = 0; i < iv.size(); ++i) iv[i] = static_cast<uint8_t>(i);
    
    // Read the entire file into memory
    std::ifstream file(input_file, std::ios::binary);
    
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Compress the data
    std::vector<uint8_t> compressed_data = gzip_compress(data);

    // Pad the compressed data
    std::vector<uint8_t> padded_data = pad_data(compressed_data, 16);

    // Encrypt the data
    symmetric_CBC cbc;
    if (cbc_start(find_cipher("aes"), iv.data(), key.data(), key.size(), 0, &cbc) != CRYPT_OK) {
        throw std::runtime_error("Error initializing CBC");
    }

    std::vector<uint8_t> encrypted_data(padded_data.size());
    if (cbc_encrypt(padded_data.data(), encrypted_data.data(), padded_data.size(), &cbc) != CRYPT_OK) {
        throw std::runtime_error("Error during encryption");
    }

    cbc_done(&cbc);

    // Return IV concatenated with the encrypted data
    encrypted_data.insert(encrypted_data.begin(), iv.begin(), iv.end());
    return encrypted_data;
}

bool is_fumen_encrypted(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    file.seekg(0x210, std::ios::beg);
    std::vector<unsigned char> buffer(32);
    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());

    // Check if the read bytes match the expected pattern
    std::vector<unsigned char> expected_bytes = {
        0x00, 0x01, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00
    };

    return buffer != expected_bytes;
}

HOOK (i32, ShowMouse, PROC_ADDRESS ("user32.dll", "ShowCursor"), bool) { return originalShowMouse (true); }
HOOK (i32, ExitWindows, PROC_ADDRESS ("user32.dll", "ExitWindowsEx")) { ExitProcess (0); }

HOOK (i32, XinputGetState, PROC_ADDRESS ("xinput9_1_0.dll", "XInputGetState")) { return ERROR_DEVICE_NOT_CONNECTED; }
HOOK (i32, XinputSetState, PROC_ADDRESS ("xinput9_1_0.dll", "XInputSetState")) { return ERROR_DEVICE_NOT_CONNECTED; }
HOOK (i32, XinputGetCapabilites, PROC_ADDRESS ("xinput9_1_0.dll", "XInputGetCapabilities")) { return ERROR_DEVICE_NOT_CONNECTED; }

HOOK (i32, ssleay_Shutdown, PROC_ADDRESS ("ssleay32.dll", "SSL_shutdown")) { return 1; }

HOOK (i64, UsbFinderInitialize, PROC_ADDRESS ("nbamUsbFinder.dll", "nbamUsbFinderInitialize")) { return 0; }
HOOK (i64, UsbFinderRelease, PROC_ADDRESS ("nbamUsbFinder.dll", "nbamUsbFinderRelease")) { return 0; }
HOOK (i64, UsbFinderGetSerialNumber, PROC_ADDRESS ("nbamUsbFinder.dll", "nbamUsbFinderGetSerialNumber"), i32 a1, char *a2) {
    strcpy (a2, chassisId.c_str ());
    return 0;
}

HOOK (i32, ws2_getaddrinfo, PROC_ADDRESS ("ws2_32.dll", "getaddrinfo"), const char *node, char *service, void *hints, void *out) {
    return originalws2_getaddrinfo (server.c_str (), service, hints, out);
}

HOOK (HANDLE, CreateFileAHook, PROC_ADDRESS ("kernel32.dll", "CreateFileA"), LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
      LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    std::filesystem::path path (lpFileName);
    if (!path.is_absolute ()) path = std::filesystem::absolute (path);
    auto originalDataFolder      = std::filesystem::current_path ().parent_path ().parent_path () / "Data" / "x64";
    auto originalLayeredFsFolder = std::filesystem::current_path ().parent_path ().parent_path () / "Data_mods" / "x64";
    auto encryptedLayeredFsFolder = std::filesystem::current_path ().parent_path ().parent_path () / "Data_mods" / "x64_enc";

    if (path.string ().find (originalDataFolder.string ()) == 0) {
        auto newPath = path.string ();
        auto encPath = path.string ();
        newPath.replace (0, originalDataFolder.string ().length (), originalLayeredFsFolder.string ());
        encPath.replace (0, originalDataFolder.string ().length (), encryptedLayeredFsFolder.string ());

        //The following code handles file redirection and if need be, file encryption.
        //It's a bit of a mess but it works well ! -Kit

        if (std::filesystem::exists (newPath)) { //If a file exists in the datamod folder
            if(is_fumen_encrypted(newPath)){ //And if it's an encrypted fumen or a different type of file, use it.
                std::cout << "Redirecting " << std::filesystem::relative (path).string() << std::endl;
                return originalCreateFileAHook (newPath.c_str (), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
                                                dwFlagsAndAttributes, hTemplateFile);
            }
            else{ //Otherwise if it's an unencrypted fumen.
                if (!std::filesystem::exists(encPath)) { //We check if we don't already have a cached file.
                    if(fumenKey.length() == 64){
                        std::cout << "Encrypting " << std::filesystem::relative (newPath) << std::endl; // If we don't we encrypt the file
                        std::ifstream crc_file(newPath, std::ios::binary);
                        std::vector<uint8_t> crc_vector((std::istreambuf_iterator<char>(crc_file)), std::istreambuf_iterator<char>());
                        uint32_t crc = crc32c(0, crc_vector.data(), crc_vector.size());
                        write_file(encPath, encrypt_file(newPath, fumenKey), crc); // And we save it
                    } else { 
                        std::cout << "Missing or invalid fumen key : " << std::filesystem::relative (newPath) << " couldn't be encrypted." << std::endl;
                        encPath = path.string();
                    }
                } else std::cout << "Using cached file for " << std::filesystem::relative (newPath) << std::endl;
                return originalCreateFileAHook (encPath.c_str (), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
                                                dwFlagsAndAttributes, hTemplateFile);
            }
        }

        //We check separately for unencrypted json files.
        std::filesystem::path json_path = newPath;
        json_path.replace_extension(".json");
        if (std::filesystem::exists(json_path)) { //If a json file exists in the folder
            bool crcBool = false;

            if (std::filesystem::exists(encPath)){
                std::ifstream crc_file(json_path, std::ios::binary);
                std::vector<uint8_t> crc_vector((std::istreambuf_iterator<char>(crc_file)), std::istreambuf_iterator<char>());
                uint32_t crc = crc32c(0, crc_vector.data(), crc_vector.size());
                crcBool = checkCRC(encPath, crc);
            }
                        
            if (!std::filesystem::exists(encPath) || crcBool) { //And if it hasn't been encrypted before
                if(datatableKey.length() == 64){
                    std::cout << "Encrypting " << std::filesystem::relative (json_path) << std::endl; //Encrypt the file
                    std::ifstream crc_file(json_path.string(), std::ios::binary);
                    std::vector<uint8_t> crc_vector((std::istreambuf_iterator<char>(crc_file)), std::istreambuf_iterator<char>());
                    uint32_t crc = crc32c(0, crc_vector.data(), crc_vector.size());
                    write_file(encPath, encrypt_file(json_path.string(), datatableKey), crc); //And save it
                } else {
                    std::cout << "Missing or invalid datatable key : " << std::filesystem::relative (newPath) << " couldn't be encrypted." << std::endl;
                    encPath = path.string();
                }
            } 
            else std::cout << "Using cached file for " << std::filesystem::relative (json_path) << std::endl; //Otherwise use the already encrypted file.
            return originalCreateFileAHook (encPath.c_str (), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
                                            dwFlagsAndAttributes, hTemplateFile);
        }
    }

    return originalCreateFileAHook (lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes,
                                    hTemplateFile);
}

// HOOK (HANDLE, CreateFileWHook, PROC_ADDRESS ("kernel32.dll", "CreateFileW"), LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
//       LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
//     std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
//     std::string strFileName = converter.to_bytes (lpFileName);

//     std::filesystem::path path (strFileName);
//     if (!path.is_absolute ()) path = std::filesystem::absolute (path);

//     auto originalDataFolder      = std::filesystem::current_path ().parent_path ().parent_path () / "Data" / "x64";
//     auto originalLayeredFsFolder = std::filesystem::current_path ().parent_path ().parent_path () / "Data_mods" / "x64";

//     if (path.string ().find (originalDataFolder.string ()) != std::string::npos) {
//         auto newPath = path.string ();
//         newPath.replace (0, originalDataFolder.string ().length (), originalLayeredFsFolder.string ());

//         if (std::filesystem::exists (newPath)) {
//             std::wstring wNewPath = converter.from_bytes (newPath);
//             std::wcout << L"Redirecting " << lpFileName << L" to " << wNewPath << std::endl;
//             return originalCreateFileWHook (wNewPath.c_str (), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
//                                             dwFlagsAndAttributes, hTemplateFile);
//         }
//     }

//     return originalCreateFileWHook (lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes,
//                                     hTemplateFile);
// }

void
GetGameVersion () {
    wchar_t w_path[MAX_PATH];
    GetModuleFileNameW (nullptr, w_path, MAX_PATH);
    std::filesystem::path path (w_path);

    if (!std::filesystem::exists (path) || !path.has_filename ()) {
        MessageBoxA (nullptr, "Failed to find executable", nullptr, MB_OK);
        ExitProcess (0);
    }

    std::ifstream stream (path, std::ios::binary);
    if (!stream.is_open ()) {
        MessageBoxA (nullptr, "Failed to read executable", nullptr, MB_OK);
        ExitProcess (0);
    }

    stream.seekg (0, std::ifstream::end);
    size_t length = stream.tellg ();
    stream.seekg (0, std::ifstream::beg);

    char *buf = (char *)calloc (length + 1, sizeof (char));
    stream.read (buf, length);

    gameVersion = (GameVersion)XXH64 (buf, length, 0);

    stream.close ();
    free (buf);

    switch (gameVersion) {
    case GameVersion::JPN00:
    case GameVersion::JPN08:
    case GameVersion::JPN39:
    case GameVersion::CHN00: break;
    default: MessageBoxA (nullptr, "Unknown game version", nullptr, MB_OK); ExitProcess (0);
    }
}

void
createCard () {
    const char hexCharacterTable[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    char buf[64]                   = {0};
    srand (time (nullptr));

    std::generate (buf, buf + 20, [&] () { return hexCharacterTable[rand () % 10]; });
    WritePrivateProfileStringA ("card", "accessCode1", buf, ".\\card.ini");
    std::generate (buf, buf + 32, [&] () { return hexCharacterTable[rand () % 16]; });
    WritePrivateProfileStringA ("card", "chipId1", buf, ".\\card.ini");
    std::generate (buf, buf + 20, [&] () { return hexCharacterTable[rand () % 10]; });
    WritePrivateProfileStringA ("card", "accessCode2", buf, ".\\card.ini");
    std::generate (buf, buf + 32, [&] () { return hexCharacterTable[rand () % 16]; });
    WritePrivateProfileStringA ("card", "chipId2", buf, ".\\card.ini");
}

BOOL
DllMain (HMODULE module, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        // This is bad, dont do this
        // I/O in DllMain can easily cause a deadlock

        std::string version = "auto";
        auto configPath     = std::filesystem::current_path () / "config.toml";
        std::unique_ptr<toml_table_t, void (*) (toml_table_t *)> config_ptr (openConfig (configPath), toml_free);
        if (config_ptr) {
            toml_table_t *config = config_ptr.get ();
            auto amauth          = openConfigSection (config, "amauth");
            if (amauth) {
                server      = readConfigString (amauth, "server", server);
                port        = readConfigString (amauth, "port", port);
                chassisId   = readConfigString (amauth, "chassis_id", chassisId);
                shopId      = readConfigString (amauth, "shop_id", shopId);
                gameVerNum  = readConfigString (amauth, "game_ver", gameVerNum);
                countryCode = readConfigString (amauth, "country_code", countryCode);

                std::strcat (fullAddress, server.c_str ());
                std::strcat (fullAddress, ":");
                std::strcat (fullAddress, port.c_str ());

                std::strcat (placeId, countryCode.c_str ());
                std::strcat (placeId, "0FF0");
            }
            auto patches = openConfigSection (config, "patches");
            if (patches) version = readConfigString (patches, "version", version);
            auto keyboard = openConfigSection (config, "keyboard");
            if (keyboard) {
                autoIME  = readConfigBool (keyboard, "auto_ime", autoIME);
                jpLayout = readConfigBool (keyboard, "jp_layout", jpLayout);
            }

            auto layeredFs = openConfigSection (config, "layeredfs");
            if (layeredFs) {
                useLayeredFs = readConfigBool (layeredFs, "enabled", useLayeredFs);
                datatableKey = readConfigString (layeredFs, "datatable_key", datatableKey);
                fumenKey     = readConfigString (layeredFs, "fumen_key", fumenKey);
            }
        }

        if (version == "auto") {
            GetGameVersion ();
        } else if (version == "JPN00") {
            gameVersion = GameVersion::JPN00;
        } else if (version == "JPN08") {
            gameVersion = GameVersion::JPN08;
        } else if (version == "JPN39") {
            gameVersion = GameVersion::JPN39;
        } else if (version == "CHN00") {
            gameVersion = GameVersion::CHN00;
        } else {
            MessageBoxA (nullptr, "Unknown patch version", nullptr, MB_OK);
            ExitProcess (0);
        }

        auto pluginPath = std::filesystem::current_path () / "plugins";

        if (std::filesystem::exists (pluginPath)) {
            for (const auto &entry : std::filesystem::directory_iterator (pluginPath)) {
                if (entry.path ().extension () == ".dll") {
                    auto name       = entry.path ().wstring ();
                    HMODULE hModule = LoadLibraryW (name.c_str ());
                    if (!hModule) {
                        wchar_t buf[128];
                        wsprintfW (buf, L"Failed to load plugin %ls", name.c_str ());
                        MessageBoxW (0, buf, name.c_str (), MB_ICONERROR);
                    } else {
                        plugins.push_back (hModule);
                    }
                }
            }
        }

        if (!std::filesystem::exists (".\\card.ini")) createCard ();
        GetPrivateProfileStringA ("card", "accessCode1", accessCode1, accessCode1, 21, ".\\card.ini");
        GetPrivateProfileStringA ("card", "chipId1", chipId1, chipId1, 33, ".\\card.ini");
        GetPrivateProfileStringA ("card", "accessCode2", accessCode2, accessCode2, 21, ".\\card.ini");
        GetPrivateProfileStringA ("card", "chipId2", chipId2, chipId2, 33, ".\\card.ini");

        INSTALL_HOOK (ShowMouse);
        INSTALL_HOOK (ExitWindows);

        INSTALL_HOOK (XinputGetState);
        INSTALL_HOOK (XinputSetState);
        INSTALL_HOOK (XinputGetCapabilites);

        INSTALL_HOOK (ssleay_Shutdown);

        INSTALL_HOOK (UsbFinderInitialize);
        INSTALL_HOOK (UsbFinderRelease);
        INSTALL_HOOK (UsbFinderGetSerialNumber);

        INSTALL_HOOK (ws2_getaddrinfo);

        if (useLayeredFs) {
            std::wcout << "Using LayeredFs!" << std::endl;
            register_cipher(&aes_desc);
            INSTALL_HOOK (CreateFileAHook);
            // INSTALL_HOOK (CreateFileWHook);
        }

        bnusio::Init ();

        switch (gameVersion) {
        case GameVersion::UNKNOWN: break;
        case GameVersion::JPN00: patches::JPN00::Init (); break;
        case GameVersion::JPN08: patches::JPN08::Init (); break;
        case GameVersion::JPN39: patches::JPN39::Init (); break;
        case GameVersion::CHN00: patches::CHN00::Init (); break;
        }
    }
    return true;
}
