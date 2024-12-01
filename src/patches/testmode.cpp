#include "constants.h"
#include "helpers.h"
#include "patches.h"
#include <exception>
#include <format>

extern GameVersion gameVersion;

bool chsPatch = false;
#define MIN(a, b) ((a)<(b)?(a):(b))

namespace patches::TestMode {
std::wstring
mergeCondition (std::wstring original, std::wstring addition, std::wstring value) {
    size_t pos = original.find (L"/");
    if (pos < 0) return original;
    std::wstring b = original.substr (0, pos - 1);
    if (b == L"False") {
        if (value.starts_with (L"!")) value = value.substr (1, value.size () - 1);
        else value = L"!" + value;
    }

    if (original.ends_with(L")")) return replace (original, L")", std::format (L" or {}:{})", addition, value));
    else return std::format (L"{}/({} or {}:{})", b, original.substr (pos + 1, original.size () - pos - 1), addition, value);
}
void
addConditionValue (pugi::xml_node *node, std::wstring key, std::wstring addition, std::wstring value) {
    pugi::xml_attribute attr = node->attribute (key.c_str ());
    if (attr != nullptr) {
        attr.set_value (mergeCondition(attr.value (), addition, value).c_str ());
    } else node->append_attribute (key.c_str ()) = std::format(L"True/{}:{}", addition, value).c_str ();
}

class RegisteredItem : public Applicable {
public:
    std::wstring selectItem;
    std::function<void ()> registerInit;
    pugi::xml_node temp;
    RegisteredItem (const std::wstring &selectItem, const std::function<void ()> &initMethod) {
        this->selectItem   = selectItem;
        this->registerInit = initMethod;
    }
    pugi::xml_node *
    Apply (pugi::xml_node *doc, pugi::xml_node *node) override {
        pugi::xml_document itemDoc;
        std::wstring itemLine = L"<root>" + this->selectItem + L"</root>";
        bool success = itemDoc.load_string (itemLine.c_str ());
        if (!success) {
            LogMessage (LogLevel::ERROR, L"Failed to parse item: {}\n", this->selectItem);
            itemDoc.load_string (L"<root><text-item label=\"@Color/Red;!!!ERROR APPLYING ITEM!!!@Color/Default;\"/></root>");
        }
        if (success) this->registerInit ();
        std::wstring layoutName = L"layout";
        if (layoutName == node->name ()) {
            temp = node->append_copy (itemDoc.first_child ().first_child ());
            return &temp;
        } else {
            pugi::xml_node breakItem = node->parent ().insert_child_after (L"break-item", *node);
            temp = node->parent ().insert_copy_after (itemDoc.first_child ().first_child (), breakItem);
            return &temp;
        }
    }
};
class RegisteredMenu : public Menu {
public:
    std::wstring menuName;
    std::wstring menuId;
    std::vector<Applicable *> items;
    pugi::xml_node temp;
    RegisteredMenu (const std::wstring &menuName, const std::wstring &menuId, std::vector<Applicable *> items = {}) {
        this->menuName = menuName;
        this->menuId = menuId;
        this->items = items;
    }
    void
    RegisterItem (Applicable *item) override {
        this->items.push_back (item);
    }
    pugi::xml_node *
    Apply (pugi::xml_node *doc, pugi::xml_node *node) override {
        size_t size = items.size ();
        if (size == 0) return node;
        size_t maxPage = size <= 14 ? 1 : (size / 13 + (size % 13 > 0));
        LogMessage (LogLevel::DEBUG, "Render testmode menu with items(size={}, page={})", size, maxPage);
        std::wstring parentId = node->parent ().parent ().attribute (L"id").value ();

        pugi::xml_document menuItem;
        menuItem.load_string (std::format (L"<root><menu-item label=\"{}\" menu=\"{}\"/></root>", this->menuName, this->menuId).c_str ());
        std::wstring layoutName = L"layout";
        if (layoutName == node->name ()) {
            temp = node->append_copy (menuItem.first_child ().first_child ());
        } else {
            pugi::xml_node menuBreakItem = node->parent ().insert_child_after (L"break-item", *node);
            temp = node->parent ().insert_copy_after (menuItem.first_child ().first_child (), menuBreakItem);
        }

        pugi::xml_document menu;
        menu.load_string (std::format (
            L"<root><menu id=\"{}\">"
            L"<layout type=\"Header\"><break-item/><text-item label=\"{}\"/><break-item/>"
            L"</layout><layout type=\"Center\" padding-x=\"23\"></layout>"
            L"<layout type=\"Footer\"><data-item data=\"FooterTextData\" param-offset-x=\"-20\"/></layout>"
            L"</menu></root>",
            this->menuId, this->menuName
        ).c_str ());
        pugi::xpath_query query = pugi::xpath_query (L"/root/menu/layout[@type='Center']");
        pugi::xml_node passing = menu.select_node (query).node ();
        std::vector<pugi::xml_node> refs = {};

        for (size_t page = 0; page < maxPage; page ++) {
            LogMessage (LogLevel::DEBUG, "Render testmode menu page ({}/{})", page + 1, maxPage);

            pugi::xml_node ref = passing;
            size_t maxIndex = MIN(size - page * 13, 13);
            for (size_t index = 0; index < maxIndex; index ++) {
                LogMessage (LogLevel::DEBUG, "Render testmode menu page ({}/{}) item ({}/{})", page + 1, maxPage, index + 1, maxIndex);
                if (index >= refs.size ()) {
                    passing = *(items[page * 13 + index]->Apply (doc, &passing));
                    if (size > 14) {
                        addConditionValue (&passing, L"invisible", std::format (L"{}Pagenation", this->menuId), std::format(L"!{}", page + 1));
                        addConditionValue (&passing, L"disable", std::format (L"{}Pagenation", this->menuId), std::format(L"!{}", page + 1));
                    }
                    refs.push_back (passing);
                } else {
                    pugi::xml_node ref = refs[index];
                    ref = *(items[page * 13 + index]->Apply (doc, &ref));
                    addConditionValue (&ref, L"invisible", std::format (L"{}Pagenation", this->menuId), std::format(L"!{}", page + 1));
                    addConditionValue (&ref, L"disable", std::format (L"{}Pagenation", this->menuId), std::format(L"!{}", page + 1));
                    ref.parent ().remove_child (ref.previous_sibling ());
                    refs[index] = ref;
                }
            }
            if (size == 14) passing = *(items[13]->Apply (doc, &passing));
        }
        passing = passing.parent ().last_child ();
        LogMessage (LogLevel::DEBUG, "Begin render testmode footer");
        size_t maxIndex = size == 14 ? 14 : MIN(size, 13);
        for (size_t i = 0; i < 15 - maxIndex; i++) passing = passing.parent ().insert_child_after (L"break-item", passing);
        if (maxPage > 1) {
            pugi::xml_document pagenation;
            pagenation.load_string (std::format (L"<root><select-item label=\"PAGE\" param-offset-x=\"23\" prefix=\"(\" suffix=\"/{})\" param-color=\"Color/Yellow\" id=\"{}Pagenation\" min=\"1\" max=\"{}\" default=\"1\"/></root>", maxPage, this->menuId, maxPage).c_str ());
            passing = passing.parent ().insert_copy_after (pagenation.first_child ().first_child (), passing);
        }

        passing = passing.parent ().insert_child_after (L"break-item", passing);
        pugi::xml_document exit;
        exit.load_string (std::format (L"<root><menu-item label=\"EXIT\" menu=\"{}\"/></root>", parentId).c_str ());
        passing = passing.parent ().insert_copy_after (exit.first_child ().first_child (), passing);

        LogMessage (LogLevel::DEBUG, L"Insert testmode menu menuId: {}, fromId: {}", this->menuId, parentId);
        pugi::xml_node root = doc->first_child ();
        root.insert_copy_after (menu.first_child ().first_child (), root.first_child ());
        return &temp;
    }
};
class RegisteredSingleItem : Applicable {
public:
    std::wstring query;
    Applicable *item;
    RegisteredSingleItem (const std::wstring query, Applicable *item) {
        this->query = query;
        this->item = item;
    }
    pugi::xml_node *
    Apply (pugi::xml_node *doc, pugi::xml_node *node) override {
        pugi::xpath_query insertQuery = pugi::xpath_query (this->query.c_str ());
        try {
            if (pugi::xml_node insertNode = doc->select_node (insertQuery).node ()) return this->item->Apply (doc, &insertNode);
        } catch ([[maybe_unused]] std::exception &e) { LogMessage (LogLevel::ERROR, L"Failed to find node by xpath: {}\n", this->query); }
        return nullptr;
    }
};
class RegisteredModify : Applicable {
public:
    std::wstring query;
    std::function<void (pugi::xml_node &)> nodeModify;
    std::function<void ()> registerInit;
    RegisteredModify (const std::wstring &query, const std::function<void (pugi::xml_node &)> &nodeModify, const std::function<void ()> &initMethod) {
        this->query        = query;
        this->nodeModify   = nodeModify;
        this->registerInit = initMethod;
    }
    pugi::xml_node *
    Apply (pugi::xml_node *doc, pugi::xml_node *node) override {
        pugi::xpath_query modifyQuery = pugi::xpath_query (this->query.c_str ());
        try {
            if (pugi::xml_node modifyNode = node->select_node (modifyQuery).node ()) {
                this->nodeModify (modifyNode);
                this->registerInit ();
            }
        } catch ([[maybe_unused]] std::exception &e) { LogMessage (LogLevel::ERROR, L"Failed to find node by xpath: {}\n", this->query); }
        return nullptr;
    }
};

Menu *modManager = new RegisteredMenu (L"MOD MANAGER", L"ModManagerMenu");
// std::vector<RegisteredItem *> registeredItems             = {};
std::vector<RegisteredSingleItem *> registeredSingleItems = {};
std::vector<RegisteredModify *> registeredModifies        = {};
std::wstring moddedInitial                                = L"";
std::wstring modded                                       = L"";

u64 appAccessor             = 0;
RefTestModeMain refTestMode = nullptr;

std::wstring
ReadXMLFileSwitcher (std::wstring &fileName) {
    const std::size_t pos   = fileName.rfind (L"/");
    const std::wstring file = fileName.substr (pos + 1, fileName.size ());
    std::wstring base       = fileName.substr (0, pos + 1);

    if (gameVersion == GameVersion::JPN39 && chsPatch) {
        if (file.starts_with (L"DeviceInitialize")) base.append (L"DeviceInitialize_china.xml");
        if (file.starts_with (L"TestMode")) base.append (L"TestMode_china.xml");
        if (std::filesystem::exists (base)) return base;
    }

    return fileName;
}

HOOK_DYNAMIC (void, TestModeSetMenuHook, u64 testModeLibrary, const wchar_t *lFileName) {
    const auto originalFileName = std::wstring (lFileName);
    std::wstring fileName       = originalFileName;
    if (fileName.ends_with (L"DeviceInitialize.xml") || fileName.ends_with (L"DeviceInitialize_asia.xml")
        || fileName.ends_with (L"DeviceInitialize_china.xml")) {
        if (moddedInitial == L"") {
            fileName = ReadXMLFileSwitcher (fileName);
            if (pugi::xml_document doc; !doc.load_file (fileName.c_str ())) {
                LogMessage (LogLevel::ERROR, L"Loading DeviceInitialize structure failed! path: " + fileName);
                moddedInitial = fileName;
            } else {
                const std::wstring modFileName
                    = replace (replace (replace (fileName, L"lize_asia.xml", L"lize_mod.xml"), L"lize_china.xml", L"lize_mod.xml"), L"lize.xml",
                               L"lize_mod.xml");
                const pugi::xpath_query dongleQuery = pugi::xpath_query (L"/root/menu[@id='TopMenu']/layout[@type='Center']/select-item[@id='DongleItem']");
                const pugi::xml_node dongleItem     = doc.select_node (dongleQuery).node ();
                pugi::xml_node talItem        = dongleItem.parent ().append_copy (dongleItem);
                talItem.attribute (L"label").set_value (L"TAIKOARCADELOADER");
                talItem.attribute (L"id").set_value (L"TaikoArcadeLoader");
                talItem.append_attribute (L"default") = L"1";
                dongleItem.parent ().append_child (L"break-item");

                [[maybe_unused]] auto ignored = doc.save_file (modFileName.c_str ());
                moddedInitial = modFileName;
                fileName      = modFileName;
            }
        } else fileName = moddedInitial;
    } else if (fileName.ends_with (L"TestMode.xml") || fileName.ends_with (L"TestMode_asia.xml") || fileName.ends_with (L"TestMode_china.xml")) {
        if (modded == L"") {
            if (!((RegisteredMenu*)modManager)->items.empty () || registeredSingleItems.empty () || !registeredModifies.empty ()) {
                fileName = ReadXMLFileSwitcher (fileName);
                if (pugi::xml_document doc; !doc.load_file (fileName.c_str ())) {
                    LogMessage (LogLevel::ERROR, L"Loading TestMode structure failed! path: " + fileName);
                    modded = fileName;
                } else {
                    const std::wstring modFileName
                        = replace (replace (replace (fileName, L"Mode_asia.xml", L"Mode_mod.xml"), L"Mode_china.xml", L"Mode_mod.xml"), L"Mode.xml",
                                   L"Mode_mod.xml");
                    if (!((RegisteredMenu*)modManager)->items.empty ()) {
                        LogMessage (LogLevel::DEBUG, "Begin generate Mod Manager menu");
                        pugi::xpath_query menuQuery
                            = pugi::xpath_query (L"/root/menu[@id='TopMenu']/layout[@type='Center']/menu-item[@menu='GameOptionsMenu']");
                        pugi::xml_node menuItem = doc.select_node (menuQuery).node ();
                        modManager->Apply (&doc, &menuItem);
                        LogMessage (LogLevel::DEBUG, "End generate Mod Manager menu");
                    }

                    if (!registeredSingleItems.empty()) {
                        LogMessage (LogLevel::DEBUG, "Begin Apply Single Items");
                        for (RegisteredSingleItem *item : registeredSingleItems) item->Apply (&doc, &doc);
                        LogMessage (LogLevel::DEBUG, "End Apply Single Items");
                    }

                    if (!registeredModifies.empty ()) {
                        LogMessage (LogLevel::DEBUG, "Begin Apply Modify Items");
                        for (RegisteredModify *modify : registeredModifies) modify->Apply (&doc, &doc);
                        LogMessage (LogLevel::DEBUG, "End Apply Modify Items");
                    }

                    [[maybe_unused]] auto _ = doc.save_file (modFileName.c_str ());
                    modded   = modFileName;
                    fileName = modFileName;
                }
            } else modded = fileName;
        } else fileName = modded;
    }

    LogMessage (LogLevel::DEBUG, L"TestModeLibrary load: " + fileName);
    originalTestModeSetMenuHook (testModeLibrary, fileName.c_str ());
}

void
CommonModify () {
    // Default off Close time
    RegisterModify (
        L"/root/menu[@id='CloseTimeSettingMenu']/layout[@type='Center']/select-item[@id='ScheduleTypeItem']",
        [&] (const pugi::xml_node &node) { node.attribute (L"default").set_value (L"0"); }, [&] {});
}

void
LocalizationCHT () {
    TestMode::RegisterModify(
        L"/root/menu[@id='TopMenu']/layout[@type='Center']/menu-item[@menu='ModManagerMenu']",
        [&](pugi::xml_node &node) { node.attribute(L"label").set_value(L"模組管理"); }, [](){}
    );
    TestMode::RegisterModify(
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Header']/text-item",
        [&](pugi::xml_node &node) { node.attribute(L"label").set_value(L"模組管理"); }, [](){}
    );
    TestMode::RegisterModify(
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Center']/select-item[@id='ModFixLanguage']",
        [&](pugi::xml_node &node) { 
            node.attribute(L"label").set_value(L"修復語言"); 
            node.attribute(L"replace-text").set_value(L"0:關閉, 1:開啓"); 
        }, [](){}
    );
    TestMode::RegisterModify(
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Center']/select-item[@id='ModUnlockSongs']",
        [&](pugi::xml_node &node) { 
            node.attribute(L"label").set_value(L"解鎖歌曲"); 
            node.attribute(L"replace-text").set_value(L"0:關閉, 1:開啓, 2:強制"); 
        }, [](){}
    );
    TestMode::RegisterModify(
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Center']/select-item[@id='ModFreezeTimer']",
        [&](pugi::xml_node &node) { 
            node.attribute(L"label").set_value(L"凍結計時"); 
            node.attribute(L"replace-text").set_value(L"0:關閉, 1:開啓"); 
        }, [](){}
    );
    TestMode::RegisterModify(
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Center']/select-item[@id='ModModeCollabo024']",
        [&](pugi::xml_node &node) { 
            node.attribute(L"label").set_value(L"鬼滅之刃模式"); 
            node.attribute(L"replace-text").set_value(L"0:黙認, 1:啓用, 2:僅登入"); 
        }, [](){}
    );
    TestMode::RegisterModify(
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Center']/select-item[@id='ModModeCollabo025']",
        [&](pugi::xml_node &node) { 
            node.attribute(L"label").set_value(L"航海王模式"); 
            node.attribute(L"replace-text").set_value(L"0:黙認, 1:啓用, 2:僅登入"); 
        }, [](){}
    );
    TestMode::RegisterModify(
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Center']/select-item[@id='ModModeCollabo026']",
        [&](pugi::xml_node &node) { 
            node.attribute(L"label").set_value(L"ＡＩ粗品模式"); 
            node.attribute(L"replace-text").set_value(L"0:黙認, 1:啓用, 2:僅登入"); 
        }, [](){}
    );
    TestMode::RegisterModify(
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Center']/select-item[@id='ModModeAprilFool001']",
        [&](pugi::xml_node &node) { 
            node.attribute(L"label").set_value(L"青春之達人模式"); 
            node.attribute(L"replace-text").set_value(L"0:黙認, 1:啓用, 2:僅登入"); 
        }, [](){}
    );
    TestMode::RegisterModify(
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Center']/select-item[@id='ModInstantResult']",
        [&](pugi::xml_node &node) { 
            node.attribute(L"label").set_value(L"即時保存"); 
            node.attribute(L"replace-text").set_value(L"0:關閉, 1:開啓"); 
        }, [](){}
    );
    TestMode::RegisterModify(
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Center']/menu-item[@menu='TopMenu']",
        [&](pugi::xml_node &node) { node.attribute(L"label").set_value(L"離開"); }, [](){}
    );
    TestMode::RegisterModify(
        L"/root/menu[@id='OthersMenu']/layout[@type='Center']/select-item[@id='AttractDemoItem']",
        [&](pugi::xml_node &node) { 
            node.attribute(L"label").set_value(L"演示遊玩影片"); 
            node.attribute(L"replace-text").set_value(L"0:關閉, 1:開啓"); 
        }, [](){}
    );
}

void
LocalizationCHS () {
    RegisterModify (
        L"/root/menu[@id='TopMenu']/layout[@type='Center']/menu-item[@menu='ModManagerMenu']",
        [&] (const pugi::xml_node &node) { node.attribute (L"label").set_value (L"模组管理"); }, [] {});
    RegisterModify (
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Header']/text-item",
        [&] (const pugi::xml_node &node) { node.attribute (L"label").set_value (L"模组管理"); }, [] {});
    RegisterModify (
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Center']/select-item[@id='ModFreezeTimer']",
        [&] (const pugi::xml_node &node) {
            node.attribute (L"label").set_value (L"冻结计时");
            node.attribute (L"replace-text").set_value (L"0:禁用, 1:启用");
        },
        [] {});
    RegisterModify (
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Center']/select-item[@id='ModModeCollabo024']",
        [&] (const pugi::xml_node &node) {
            node.attribute (L"label").set_value (L"鬼灭之刃模式");
            node.attribute (L"replace-text").set_value (L"0:默认, 1:启用, 2:仅刷卡");
        },
        [] {});
    RegisterModify (
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Center']/select-item[@id='ModModeCollabo025']",
        [&] (const pugi::xml_node &node) {
            node.attribute (L"label").set_value (L"航海王模式");
            node.attribute (L"replace-text").set_value (L"0:默认, 1:启用, 2:仅刷卡");
        },
        [] {});
    RegisterModify (
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Center']/select-item[@id='ModModeCollabo026']",
        [&] (const pugi::xml_node &node) {
            node.attribute (L"label").set_value (L"ＡＩ粗品模式");
            node.attribute (L"replace-text").set_value (L"0:默认, 1:启用, 2:仅刷卡");
        },
        [] {});
    RegisterModify (
        L"/root/menu[@id='ModManagerMenu']/layout[@type='Center']/menu-item[@menu='TopMenu']",
        [&] (const pugi::xml_node &node) { node.attribute (L"label").set_value (L"离开"); }, [] {});
}

void
Init () {
    LogMessage (LogLevel::INFO, "Init TestMode patches");

    const auto configPath = std::filesystem::current_path () / "config.toml";
    const std::unique_ptr<toml_table_t, void (*) (toml_table_t *)> config_ptr (openConfig (configPath), toml_free);

    const u64 testModeSetMenuAddress = PROC_ADDRESS_OFFSET ("TestModeLibrary.dll", 0x99D0);
    switch (gameVersion) {
    case GameVersion::UNKNOWN: break;
    case GameVersion::JPN00: break;
    case GameVersion::JPN08: break;
    case GameVersion::JPN39: {
        if (config_ptr) {
            if (const auto patches = openConfigSection (config_ptr.get (), "patches")) {
                if (const auto jpn39 = openConfigSection (patches, "jpn39")) chsPatch = readConfigBool (jpn39, "chs_patch", chsPatch);
            }
        }
        if (chsPatch) LocalizationCHT ();
    } break;
    case GameVersion::CHN00: break;
    }

    CommonModify ();

    INSTALL_HOOK_DYNAMIC (TestModeSetMenuHook, testModeSetMenuAddress);
}

void
SetupAccessor (const u64 appAccessor, const RefTestModeMain refTestMode) {
    TestMode::appAccessor = appAccessor;
    TestMode::refTestMode = refTestMode;
}

int
ReadTestModeValue (const wchar_t *itemId) {
    if (appAccessor) {
        if (const u64 testModeMain = refTestMode (appAccessor)) {
            int value   = 0;
            u64 *reader = *reinterpret_cast<u64 **> (testModeMain + 16);
            (*reinterpret_cast<void (__fastcall **) (u64 *, const wchar_t *, int *)> (*reader + 256)) (reader, itemId, &value);
            return value;
        }
    }
    LogMessage (LogLevel::ERROR, (std::wstring (L"Read TestMode(") + itemId + L") failed!").c_str ());
    return -1;
}

Menu *
CreateMenu (const std::wstring &menuName, const std::wstring &menuId) {
    LogMessage (LogLevel::DEBUG, L"Create MenuName: {} MenuId: {}", menuName, menuId);
    return new RegisteredMenu (menuName, menuId);
}

void
RegisterItem (const std::wstring &item, const std::function<void ()> &initMethod, Menu *menu) {
    LogMessage (LogLevel::DEBUG, L"Register Item: {}", item);
    menu->RegisterItem (new RegisteredItem (item, initMethod));
}

void
RegisterItem (const std::wstring &item, const std::function<void ()> &initMethod) {
    LogMessage (LogLevel::DEBUG, L"Register Item: {}", item);
    modManager->RegisterItem (new RegisteredItem (item, initMethod));
}

void
RegisterItem (const std::wstring &item, Menu *menu) {
    LogMessage (LogLevel::DEBUG, L"Register Item: {}", item);
    menu->RegisterItem (new RegisteredItem (item, [](){}));
}

void
RegisterItem (const std::wstring &item) {
    LogMessage (LogLevel::DEBUG, L"Register Item: {}", item);
    modManager->RegisterItem (new RegisteredItem (item, [](){}));
}

void
RegisterItem (Applicable *item, Menu *menu) {
    LogMessage (LogLevel::DEBUG, L"Register Item");
    menu->RegisterItem (item);
}

void
RegisterItem (Applicable *item) {
    LogMessage (LogLevel::DEBUG, L"Register Item");
    modManager->RegisterItem (item);
}

void
RegisterItemAfter (const std::wstring &query, const std::wstring &item, const std::function<void()> &initMethod) {
    LogMessage (LogLevel::DEBUG, L"Register Query: {} Item: {}", query, item);
    registeredSingleItems.push_back (new RegisteredSingleItem (query, new RegisteredItem (item, initMethod)));
}

void
RegisterItemAfter (const std::wstring &query, const std::wstring &item) {
    LogMessage (LogLevel::DEBUG, L"Register Query: {} Item: {}", query, item);
    registeredSingleItems.push_back (new RegisteredSingleItem (query, new RegisteredItem (item, [](){})));
}

void
RegisterItemAfter (const std::wstring &query, Applicable *item) {
    LogMessage (LogLevel::DEBUG, L"Register Query: {} Item: ptr", query);
    registeredSingleItems.push_back (new RegisteredSingleItem (query, item));
}

void
RegisterModify (const std::wstring &query, const std::function<void (pugi::xml_node &)> &nodeModify, const std::function<void ()> &initMethod) {
    LogMessage (LogLevel::DEBUG, L"Register Modify: {}", query);
    registeredModifies.push_back (new RegisteredModify (query, nodeModify, initMethod));
}

void
RegisterModify (const std::wstring &query, const std::function<void (pugi::xml_node &)> &nodeModify) {
    LogMessage (LogLevel::DEBUG, L"Register Modify: {}", query);
    registeredModifies.push_back (new RegisteredModify (query, nodeModify, [](){}));
}

void
Append (const pugi::xml_node &node, const wchar_t *attr, const std::wstring &append) {
    pugi::xml_attribute attribute = node.attribute (attr);
    const std::wstring attrValue        = std::wstring (attribute.value ()) + append;
    attribute.set_value (attrValue.c_str ());
}
}