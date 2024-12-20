#include <winsock2.h>
#include "helpers.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <ws2tcpip.h>

/*
 * Reference: https://gitea.tendokyu.moe/Hay1tsme/bananatools/src/branch/master/amcus/iauth.c
 * https://github.com/BroGamer4256/TaikoArcadeLoader/blob/master/plugins/amauth/dllmain.cpp
 */

extern std::string server;
extern std::string chassisId;
extern std::string shopId;
extern std::string gameVerNum;
extern std::string countryCode;
extern char fullAddress[256];
extern char placeId[16];

namespace patches::AmAuth {

char server_ip[16];

constexpr GUID IID_CAuth{0x045A5150, 0xD2B3, 0x4590, {0xA3, 0x8B, 0xC1, 0x15, 0x86, 0x78, 0xE1, 0xAC}};

constexpr GUID IID_CAuthFactory{0x4603BB03, 0x058D, 0x43D9, {0xB9, 0x6F, 0x63, 0x9B, 0xE9, 0x08, 0xC1, 0xED}};

typedef struct amcus_network_state {
    char mode[16];
    char pcbid[16];
    char dongle_serial[16];
    char shop_router_ip[16];
    char auth_server_ip[16];
    char local_ip[16];
    char subnet_mask[16];
    char gateway[16];
    char primary_dns[16];
    int hop_count;
    u32 line_type;
    u32 line_status;
    u32 content_router_status;
    u32 shop_router_status;
    u32 hop_status;
} amcus_network_state_t;

typedef struct amcus_auth_server_resp {
    char uri[257];
    char host[257];
    char shop_name[256];
    char shop_nickname[256];
    char region0[16];
    char region_name0[256];
    char region_name1[256];
    char region_name2[256];
    char region_name3[256];
    char place_id[16];
    char setting[16];
    char country[16];
    char timezone[32];
    char res_class[64];
} amcus_auth_server_resp_t;

typedef struct amcus_version_info {
    char game_rev[4];
    char auth_type[16];
    char game_id[8];
    char game_ver[8];
    char game_cd[8];
    char cacfg_game_ver[8];
    char game_board_type[4];
    char game_board_id[4];
    char auth_url[256];
} amcus_version_info_t;

struct allnet_state {};

/* Memory Size: 144 */
struct mucha_state {
    /* Offset: 0 */ /* ENUM32 */ u32 state;
    /* Offset: 4 */ /* ENUM32 */ u32 error;
    /* Offset: 8  */ i32 auth_state;
    /* Offset: 12 */ i32 auth_count;
    /* Offset: 16 */ i32 state_dlexec;
    /* Offset: 20 */ i32 state_dlstep;
    /* Offset: 24 */ i32 state_dllan;
    /* Offset: 28 */ i32 state_dlwan;
    /* Offset: 32 */ i32 state_io;
    /* Offset: 36 */ i16 cacfg_ver_major;
    /* Offset: 38 */ i16 cacfg_ver_minor;
    /* Offset: 40 */ i16 app_ver_major;
    /* Offset: 42 */ i16 app_ver_minor;
    /* Offset: 44 */ i16 dl_ver_major;
    /* Offset: 46 */ i16 dl_ver_minor;
    /* Offset: 48 */ i32 dl_ver_total;
    /* Offset: 52 */ i32 dl_ver_done;
    /* Offset: 56 */ i64 dl_total;
    /* Offset: 64 */ i64 dl_done;
    /* Offset: 72 */ i64 dl_pc_done;
    /* Offset: 80 */ i64 dl_io_total;
    /* Offset: 88 */ i64 dl_io_done;
    /* Offset: 96 */ i32 dl_check_complete;
    /* Offset: 100 */ i32 token_consumed;
    /* Offset: 104 */ i32 token_charged;
    /* Offset: 108 */ i32 token_unit;
    /* Offset: 112 */ i32 token_lower;
    /* Offset: 116 */ i32 token_upper;
    /* Offset: 120 */ i32 token_added;
    /* Offset: 124 */ i32 token_month_lower;
    /* Offset: 128 */ i32 token_month_upper;
    /* Offset: 132 */ i32 is_forced_boot;
    /* Offset: 136 */ i32 Member88;
    /* Offset: 140 */ i32 unknown_a;
    /* Offset: 144 */ i32 unknown_b;
};

/* Memory Size: 208 */
typedef struct amcus_state {
    /* Offset: 0 */ /* ENUM32 */ u32 allnet_state;
    /* Offset: 4 */ /* ENUM32 */ u32 allnet_error;
    /* Offset: 8 */ i32 allnet_auth_state;
    /* Offset: 12 */ i32 allnet_auth_count;
    /* Offset: 16 */ i32 allnet_last_error;
    /* Offset: 24 */
    mucha_state mucha_state;
    /* Offset: 176 */ i64 clock_status;
    /* Offset: 184 */ i64 name_resolution_timeout;
    /* Offset: 192 */ /* ENUM32 */ u32 auth_type;
    /* Offset: 196 */ /* ENUM32 */ u32 cab_mode;
    /* Offset: 200 */ /* ENUM32 */ u32 state;
    /* Offset: 204 */ /* ENUM32 */ u32 err;
} amcus_state_t;

typedef struct mucha_boardauth_resp {
    /* Offset: 0 */ char url_charge[256];
    /* Offset: 256 */ char url_file[256];
    /* Offset: 512 */ char url_url1[256];
    /* Offset: 768 */ char url_url2[256];
    /* Offset: 1024 */ char url_url3[256];

    /* Offset: 1280 */ char place_id[16];
    /* Offset: 1296 */ char country_cd[16];
    /* Offset: 1312 */ char shop_name[256];
    /* Offset: 1568 */ char shop_nickname[128];
    /* Offset: 1696 */ char area0[64];
    /* Offset: 1760 */ char area1[64];
    /* Offset: 1824 */ char area2[64];
    /* Offset: 1888 */ char area3[64];
    /* Offset: 1952 */ char area_full0[256];
    /* Offset: 2208 */ char area_full1[256];
    /* Offset: 2464 */ char area_full2[256];
    /* Offset: 2720 */ char area_full3[256];

    /* Offset: 2976 */ char shop_name_en[64];
    /* Offset: 3040 */ char shop_nickname_en[32];
    /* Offset: 3072 */ char area0_en[32];
    /* Offset: 3104 */ char area1_en[32];
    /* Offset: 3136 */ char area2_en[32];
    /* Offset: 3168 */ char area3_en[32];
    /* Offset: 3200 */ char area_full0_en[128];
    /* Offset: 3328 */ char area_full1_en[128];
    /* Offset: 3456 */ char area_full2_en[128];
    /* Offset: 3584 */ char area_full3_en[128];

    /* Offset: 3712 */ char prefecture_id[16];
    /* Offset: 3728 */ char expiration_date[16];
    /* Offset: 3744 */ char use_token[16];
    /* Offset: 3760 */ char consume_token[32];
    /* Offset: 3792 */ char dongle_flag[8];
    /* Offset: 3800 */ char force_boot[8];
    /* Offset: 3808 */ char auth_token[384];
    /* Offset: 4192 */ char division_code[16];
} mucha_boardauth_resp_t;

enum daemon_state {
    DAEMON_UNKNOWN,
    DAEMON_GET_HOP,
    DAEMON_GET_TIP,
    DAEMON_GET_AIP,
    DAEMON_GET_NET_INFO,
    DAEMON_INIT,
    DAEMON_AUTH_START,
    DAEMON_AUTH_BUSY,
    DAEMON_AUTH_RETRY,
    DAEMON_DL,
    DAEMON_EXPORT,
    DAEMON_IMPORT,
    DAEMON_CONSUME_CHARGE,
    DAEMON_NOTIFY_CHARGE,
    DAEMON_CHECK_LAN,
    DAEMON_IDLE,
    DAEMON_FINALIZE,
    DAEMON_BUSY
};

enum dllan_state {
    DLLAN_UNKNOWN = 40,
    DLLAN_NONE,
    DLLAN_REQ,
    DLLAN_DISABLE,
    DLLAN_SERVER_NOTHING,
    DLLAN_SERVER_VER_NOTHING,
    DLLAN_SERVER_CHECKCODE_NOTHING,
    DLLAN_SERVER_IMGCHUNK_NOTHING,
    DLLAN_COMPLETE
};

enum dlwan_state {
    DLWAN_UNKNOWN = 49,
    DLWAN_NONE,
    DLWAN_REQ,
    DLWAN_COMPLETE,
    DLWAN_DISABLE,
    DLWAN_WANENV_INVALID,
    DLWAN_BASICAUTHKEY_INVALID,
    DLWAN_SERVER_VERINFO_NOTHING,
    DLWAN_SERVER_VERINFO_VERSION_NOTHING,
};

enum daemon_io_info {
    DAEMON_IO_UNKNOWN = 58,
    DAEMON_IO_NONE,
    DAEMON_IO_IDLE,
    DAEMON_IO_EXPORT,
    DAEMON_IO_IMPORT,
    DAEMON_IO_FAIL,
    DAEMON_IO_SUCCESS,
};

enum dlexec_state {
    DLEXEC_UNKNOWN = 25,
    DLEXEC_NONE,
    DLEXEC_PROC,
    DLEXEC_STOP,
    DLEXEC_STOPPING,
};

enum dlstep_state {
    DLSTEP_UNKNOWN = 30,
    DLSTEP_NONE,
    DLSTEP_IDLE,
    DLSTEP_PARTIMG_CHECK,
    DLSTEP_STOP_REQ,
    DLSTEP_LAN_VERINFO,
    DLSTEP_LAN_CHECKCODE,
    DLSTEP_LAN_IMGCHUNK,
    DLSTEP_WAN_CHECKCODE,
    DLSTEP_WAN_IMGCHUNK,
};

enum auth_type { AUTH_TYPE_OFFLINE, AUTH_TYPE_ALLNET, AUTH_TYPE_NBLINE, AUTH_TYPE_CHARGE_NORMAL, AUTH_TYPE_CHARGE_MONTHLY };

enum daemon_mode {
    DAEMON_MODE_UNKNOWN,
    DAEMON_MODE_SERVER,
    DAEMON_MODE_CLIENT,
    DAEMON_MODE_STANDALONE,
};

// ReSharper disable once CppClassCanBeFinal
class CAuth : public IUnknown {
public:
    STDMETHODIMP
    QueryInterface (REFIID riid, LPVOID *ppvObj) override {
        wchar_t *iid_str;
        [[maybe_unused]] auto _ = StringFromCLSID (riid, &iid_str);

        if (riid == IID_IUnknown || riid == IID_CAuth) {
            *ppvObj = this;
            this->AddRef ();
            return 0;
        }
        *ppvObj = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_ (ULONG) AddRef () override { return this->refCount++; }
    STDMETHODIMP_ (ULONG) Release () override {
        this->refCount--;
        if (this->refCount <= 0) {
            // delete this;
            return 0;
        }
        return this->refCount;
    }

    virtual i64 Unk3 (u32 a1) { return 1; }

    virtual i64 Unk4 () { return 1; }

    virtual i32 Unk5 () { return 0; }

    virtual i64 Unk6 () { return 1; }

    virtual i32 Unk7 () { return 0; }

    virtual i32 Unk8 () { return 0; }

    virtual i32 IAuth_GetUpdaterState (amcus_state_t *arr) {
        memset (arr, 0, sizeof (*arr));
        // Convert gameVerNum from string to double
        const double ver_d = std::stod (gameVerNum);

        const int ver_top = static_cast<int> (ver_d);
        int ver_btm       = static_cast<int> (ver_d * 100);

        if (ver_top != 0) ver_btm %= ver_top * 100;

        arr->allnet_state      = DAEMON_IDLE;
        arr->allnet_auth_state = 2;
        arr->allnet_auth_count = 1;

        arr->mucha_state.state             = DAEMON_DL;
        arr->mucha_state.auth_state        = 2;
        arr->mucha_state.auth_count        = 1;
        arr->mucha_state.state_dlexec      = DLEXEC_PROC;
        arr->mucha_state.state_dlstep      = DLSTEP_IDLE;
        arr->mucha_state.state_dllan       = DLLAN_DISABLE;
        arr->mucha_state.state_dlwan       = DLWAN_COMPLETE;
        arr->mucha_state.state_io          = DAEMON_IO_NONE;
        arr->mucha_state.cacfg_ver_major   = ver_top;
        arr->mucha_state.cacfg_ver_minor   = ver_btm;
        arr->mucha_state.app_ver_major     = ver_top;
        arr->mucha_state.app_ver_minor     = ver_btm;
        arr->mucha_state.dl_check_complete = 1;
        arr->mucha_state.token_added       = 100;
        arr->mucha_state.token_charged     = 100;
        arr->mucha_state.token_unit        = 1;

        arr->clock_status = 1;
        arr->auth_type    = AUTH_TYPE_ALLNET;
        arr->cab_mode     = DAEMON_MODE_STANDALONE;
        arr->state        = DAEMON_IDLE;

        /*memset(a1, 0, sizeof(i32) * 0x31);
        a1[0] = 15;
        a1[2] = 2;
        a1[3] = 1;
        a1[6] = 9;
        a1[8] = 2;
        a1[9] = 1;
        a1[10] = 27;
        a1[11] = 33;
        a1[12] = 41;
        a1[13] = 50;
        a1[14] = 59;
        a1[15] = 1179656;
        a1[30] = 1;
        a1[46] = 1;
        a1[47] = 3;
        a1[48] = 9;*/
        return 0;
    }

    virtual i32 IAuth_GetCabinetConfig (amcus_network_state_t *state) {
        memset (state, 0, sizeof (*state));
        strcpy_s (state->mode, "STANDALONE");
        strcpy_s (state->pcbid, "ABLN1080001");
        strcpy_s (state->dongle_serial, chassisId.c_str ());
        strcpy_s (state->auth_server_ip, server_ip);
        strcpy_s (state->local_ip, "127.0.0.1");
        strcpy_s (state->shop_router_ip, "127.0.0.1");
        strcpy_s (state->subnet_mask, "***.***.***.***");
        strcpy_s (state->gateway, "***.***.***.***");
        strcpy_s (state->primary_dns, "***.***.***.***");

        state->hop_count             = 1;
        state->line_type             = 1;
        state->line_status           = 1;
        state->content_router_status = 1;
        state->shop_router_status    = 1;
        state->hop_status            = 1;
        return 0;
    }

    virtual i32 IAuth_GetVersionInfo (amcus_version_info_t *version) {
        memset (version, 0, sizeof (*version));
        strcpy_s (version->game_rev, "1");
        strcpy_s (version->auth_type, "ALL.NET");
        strcpy_s (version->game_id, "SBWY");
        strcpy_s (version->game_ver, "12.20");
        strcpy_s (version->game_cd, "S121");
        strcpy_s (version->cacfg_game_ver, gameVerNum.c_str ());
        strcpy_s (version->game_board_type, "0");
        strcpy_s (version->game_board_id, "PCB");
        strcpy_s (version->auth_url, fullAddress);
        return 0;
    }

    virtual i32 Unk12 () { return 1; }

    virtual i32 Unk13 () { return 1; }

    virtual i32 IAuth_GetAuthServerResp (amcus_auth_server_resp_t *resp) {
        memset (resp, 0, sizeof (*resp));
        strcpy_s (resp->uri, fullAddress);
        strcpy_s (resp->host, fullAddress);

        strcpy_s (resp->shop_name, shopId.c_str ());
        strcpy_s (resp->shop_nickname, shopId.c_str ());

        strcpy_s (resp->region0, "01035");

        strcpy_s (resp->region_name0, "NAMCO");
        strcpy_s (resp->region_name1, "X");
        strcpy_s (resp->region_name2, "Y");
        strcpy_s (resp->region_name3, "Z");
        strcpy_s (resp->place_id, placeId);
        strcpy_s (resp->setting, "");
        strcpy_s (resp->country, countryCode.c_str ());
        strcpy_s (resp->timezone, "+0900");
        strcpy_s (resp->res_class, "PowerOnResponseVer3");
        return 0;
    }

    virtual i32 Unk15 () { return 0; }

    virtual i32 Unk16 () { return 0; }

    virtual i32 Unk17 () { return 0; }

    virtual i32 IAuth_GetMuchaAuthResponse (mucha_boardauth_resp_t *arr) {
        memset (arr, 0, sizeof (*arr));
        strcpy_s (arr->shop_name, sizeof (arr->shop_name), shopId.c_str ());
        strcpy_s (arr->shop_name_en, sizeof (arr->shop_name_en), shopId.c_str ());
        strcpy_s (arr->shop_nickname, sizeof (arr->shop_nickname), shopId.c_str ());
        strcpy_s (arr->shop_nickname_en, sizeof (arr->shop_nickname_en), shopId.c_str ());
        strcpy_s (arr->place_id, sizeof (arr->place_id), placeId);
        strcpy_s (arr->country_cd, sizeof (arr->country_cd), countryCode.c_str ());

        strcpy_s (arr->area0, sizeof (arr->area0), "008");
        strcpy_s (arr->area0_en, sizeof (arr->area0_en), "008");
        strcpy_s (arr->area1, sizeof (arr->area1), "009");
        strcpy_s (arr->area1_en, sizeof (arr->area1_en), "009");
        strcpy_s (arr->area2, sizeof (arr->area2), "010");
        strcpy_s (arr->area2_en, sizeof (arr->area2_en), "010");
        strcpy_s (arr->area3, sizeof (arr->area3), "011");
        strcpy_s (arr->area3_en, sizeof (arr->area3_en), "011");

        strcpy_s (arr->prefecture_id, sizeof (arr->prefecture_id), "1");
        strcpy_s (arr->expiration_date, sizeof (arr->expiration_date), "");
        strcpy_s (arr->consume_token, sizeof (arr->consume_token), "10");
        strcpy_s (arr->force_boot, sizeof (arr->force_boot), "0");
        strcpy_s (arr->use_token, sizeof (arr->use_token), "11");
        strcpy_s (arr->dongle_flag, sizeof (arr->dongle_flag), "1");
        strcpy_s (arr->auth_token, sizeof (arr->auth_token), "1");
        strcpy_s (arr->division_code, sizeof (arr->division_code), "1");

        strcpy_s (arr->url_charge, sizeof (arr->url_charge), "http://127.0.0.1/charge/");
        strcpy_s (arr->url_file, sizeof (arr->url_file), "http://127.0.0.1/file/");
        strcpy_s (arr->url_url1, sizeof (arr->url_url1), fullAddress);
        strcpy_s (arr->url_url2, sizeof (arr->url_url2), fullAddress);
        strcpy_s (arr->url_url3, sizeof (arr->url_url3), "http://127.0.0.1/url3/");
        return 0;
    }

    virtual i32 Unk19 (u8 *a1) {
        memset (a1, 0, 0x38);
        a1[0] = 1;
        return 1;
    }

    virtual i32 Unk20 () { return 0; }

    virtual i32 Unk21 () { return 1; }

    virtual i32 Unk22 () { return 0; }

    virtual i32 Unk23 () { return 0; }

    virtual i32 Unk24 () { return 0; }

    virtual i32 Unk25 () { return 1; }

    virtual i32 Unk26 () { return 0; }

    virtual i32 Unk27 () { return 1; }

    virtual i32 Unk28 () { return 0; }

    virtual i32 Unk29 () { return 0; }

    virtual i32 Unk30 () { return 0; }

    virtual i32 PrintDebugInfo () { return 0; }

    virtual i32 Unk32 (void *a1) { return 0; }

    virtual void Unk33 () {}

    CAuth () = default;

    virtual ~CAuth () {}

private:
    i32 refCount = 0;
};

class CAuthFactory final : public IClassFactory {
public:
    virtual ~CAuthFactory () = default;
    STDMETHODIMP
    QueryInterface (REFIID riid, LPVOID *ppvObj) override {
        wchar_t *iid_str;
        [[maybe_unused]] auto _ = StringFromCLSID (riid, &iid_str);

        if (riid == IID_IUnknown || riid == IID_IClassFactory || riid == IID_CAuthFactory) {
            *ppvObj = this;
            return 0;
        }
        *ppvObj = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_ (ULONG) AddRef () override { return 2; }
    STDMETHODIMP_ (ULONG) Release () override { return 1; }

    HRESULT CreateInstance (IUnknown *outer, REFIID riid, void **object) override {
        if (outer != nullptr) return CLASS_E_NOAGGREGATION;

        const auto auth = new CAuth ();
        return auth->QueryInterface (riid, object);
    }

    HRESULT LockServer (i32 lock) override { return 0; }
};

SafetyHookInline originalCoCreateInstanceHook;

static HRESULT STDAPICALLTYPE
CoCreateInstanceHook (const IID *const rclsid, const LPUNKNOWN pUnkOuter, const DWORD dwClsContext, const IID *const riid, LPVOID *ppv) {
    HRESULT result;

    LPOLESTR clsidStr       = nullptr;
    // const LPOLESTR iidStr         = nullptr;
    [[maybe_unused]] auto _ = StringFromIID (*rclsid, &clsidStr);
    // _                       = StringFromIID (*riid, &iidStr);

    if (IsEqualGUID (*rclsid, IID_CAuthFactory) && IsEqualGUID (*riid, IID_CAuth)) {
        const auto cauth = new CAuth ();
        result           = cauth->QueryInterface (*riid, ppv);
    } else {
        result = originalCoCreateInstanceHook.stdcall<HRESULT> (rclsid, pUnkOuter, dwClsContext, riid, ppv);
    }

    CoTaskMemFree (clsidStr);
    // CoTaskMemFree (iidStr);
    return result;
}

void
Init () {
    LogMessage (LogLevel::INFO, "Init AmAuth patches");

    auto whereCoCreateInstance = GetProcAddress (LoadLibraryW (L"ole32.dll"), "CoCreateInstance");
    originalCoCreateInstanceHook = safetyhook::create_inline ((void *)whereCoCreateInstance, CoCreateInstanceHook);

    addrinfo *res = nullptr;
    getaddrinfo (server.c_str (), "", nullptr, &res);
    for (const addrinfo *i = res; i != nullptr; i = i->ai_next) {
        if (res->ai_addr->sa_family != AF_INET) continue;
        const sockaddr_in *p = reinterpret_cast<struct sockaddr_in *> (res->ai_addr);
        inet_ntop (AF_INET, &p->sin_addr, server_ip, 0x10);
        break;
    }
}
} // namespace patches::AmAuth
