#include "ProjectsPopup.hpp"

#include "EditorPopup.hpp"
#include "HelpPopup.hpp"

#include <Geode/ui/Notification.hpp>
#include <Geode/utils/file.hpp>

#include <ctime>

using namespace geode::prelude;

namespace gdcode::ui {

// ---------------------------------------------------------------------------
// NamePopup
// ---------------------------------------------------------------------------

NamePopup* NamePopup::create(std::string const& title, std::string const& placeholder,
                             std::function<void(std::string)> onDone) {
    auto* ret = new NamePopup();
    if (ret->init(title, placeholder, std::move(onDone))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool NamePopup::init(std::string const& title, std::string const& placeholder,
                     std::function<void(std::string)> onDone) {
    if (!Popup::init(280.f, 130.f, "GJ_square01.png")) return false;
    m_onDone = std::move(onDone);
    this->setTitle(title, "goldFont.fnt", 0.7f, 18.f);

    m_input = TextInput::create(220.f, placeholder, "bigFont.fnt");
    m_input->setCommonFilter(CommonFilter::Any);
    m_input->setMaxCharCount(40);
    m_mainLayer->addChildAtPosition(m_input, Anchor::Center, {0.f, 8.f});

    auto* spr = ButtonSprite::create("Create", 80, 0, 0.6f, true, "bigFont.fnt", "GJ_button_01.png", 28.f);
    auto* btn = CCMenuItemExt::createSpriteExtra(spr, [this](auto) { submit(); });
    m_buttonMenu->addChildAtPosition(btn, Anchor::Bottom, {0.f, 24.f});
    m_input->focus();
    return true;
}

void NamePopup::submit() {
    std::string name = m_input->getString();
    // trim
    while (!name.empty() && name.back() == ' ') name.pop_back();
    while (!name.empty() && name.front() == ' ') name.erase(name.begin());
    if (name.empty()) {
        Notification::create("Please enter a name", NotificationIcon::Warning)->show();
        return;
    }
    auto cb = m_onDone;
    this->onClose(nullptr);
    if (cb) cb(name);
}

// ---------------------------------------------------------------------------
// ProjectsPopup
// ---------------------------------------------------------------------------

namespace {

constexpr float kW = 420.f;
constexpr float kH = 280.f;
constexpr float kListW = 396.f;
constexpr float kListH = 190.f;

std::string formatDate(std::int64_t unixSeconds) {
    if (unixSeconds <= 0) return "-";
    std::time_t t = static_cast<std::time_t>(unixSeconds);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
}

} // namespace

ProjectsPopup* ProjectsPopup::create() {
    auto* ret = new ProjectsPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool ProjectsPopup::init() {
    if (!Popup::init(kW, kH, "GJ_square01.png")) return false;
    this->setID("projects-popup"_spr);
    this->setTitle("GDCode projects", "goldFont.fnt", 0.75f, 18.f);

    auto* bg = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
    bg->setColor({0, 0, 0});
    bg->setOpacity(110);
    bg->setContentSize({kListW + 8.f, kListH + 8.f});
    m_mainLayer->addChildAtPosition(bg, Anchor::Top, {0.f, -(34.f + (kListH + 8.f) / 2)});

    m_list = ScrollLayer::create({kListW, kListH});
    m_list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(3.f));
    m_list->setTouchEnabled(true);
    bg->addChildAtPosition(m_list, Anchor::BottomLeft, {4.f, 4.f});

    m_empty = CCLabelBMFont::create("No projects yet.\nPress New to write your first level.", "bigFont.fnt");
    m_empty->setScale(0.45f);
    m_empty->setOpacity(140);
    m_empty->setAlignment(kCCTextAlignmentCenter);
    bg->addChildAtPosition(m_empty, Anchor::Center);

    auto* row = CCMenu::create();
    row->setContentSize({kW - 24.f, 34.f});
    row->setLayout(RowLayout::create()->setGap(8.f)->setAxisAlignment(AxisAlignment::Center));
    m_mainLayer->addChildAtPosition(row, Anchor::Bottom, {0.f, 24.f});

    auto button = [&](char const* text, char const* bgName, float width, std::function<void()> cb) {
        auto* spr = ButtonSprite::create(text, static_cast<int>(width), 0, 0.55f, true, "bigFont.fnt", bgName, 26.f);
        return CCMenuItemExt::createSpriteExtra(spr, [cb = std::move(cb)](auto) { cb(); });
    };
    row->addChild(button("New", "GJ_button_01.png", 70.f, [this] { onNew(nullptr); }));
    row->addChild(button("Import clipboard", "GJ_button_04.png", 130.f, [this] { onImportClipboard(nullptr); }));
    row->addChild(button("Folder", "GJ_button_04.png", 70.f, [this] { onOpenFolder(nullptr); }));
    row->addChild(button("?", "GJ_button_05.png", 30.f, [] { HelpPopup::create()->show(); }));
    row->updateLayout();

    reload();
    return true;
}

void ProjectsPopup::reload() {
    auto* content = m_list->m_contentLayer;
    content->removeAllChildrenWithCleanup(true);
    auto projects = storage::ProjectStore::get().list();
    m_empty->setVisible(projects.empty());
    for (auto const& meta : projects) {
        content->addChild(makeRow(meta, kListW));
    }
    content->updateLayout();
    m_list->scrollToTop();
    handleTouchPriority(this);
}

CCNode* ProjectsPopup::makeRow(storage::ProjectMeta const& meta, float width) {
    constexpr float height = 36.f;
    auto* row = CCNode::create();
    row->setContentSize({width, height});
    row->setAnchorPoint({0.5f, 0.5f});

    auto* bg = CCLayerColor::create({255, 255, 255, 18}, width, height);
    row->addChild(bg);

    auto* name = CCLabelBMFont::create(meta.name.c_str(), "bigFont.fnt");
    name->setScale(0.5f);
    name->limitLabelWidth(width - 130.f, 0.5f, 0.2f);
    name->setAnchorPoint({0.f, 0.5f});
    row->addChildAtPosition(name, Anchor::Left, {10.f, 7.f});

    std::string info = fmt::format("{}  |  {} objects{}", formatDate(meta.updatedAt), meta.lastObjectCount,
                                   meta.linkedLevel ? "  |  level: " + meta.linkedLevel->levelName : "");
    auto* sub = CCLabelBMFont::create(info.c_str(), "chatFont.fnt");
    sub->setScale(0.5f);
    sub->setOpacity(150);
    sub->limitLabelWidth(width - 130.f, 0.5f, 0.2f);
    sub->setAnchorPoint({0.f, 0.5f});
    row->addChildAtPosition(sub, Anchor::Left, {10.f, -8.f});

    auto* menu = CCMenu::create();
    menu->setContentSize({width, height});
    menu->ignoreAnchorPointForPosition(false);
    menu->setAnchorPoint({0.5f, 0.5f});

    std::string id = meta.id;
    auto* openSpr = ButtonSprite::create("Open", 56, 0, 0.5f, true, "bigFont.fnt", "GJ_button_01.png", 24.f);
    auto* openBtn = CCMenuItemExt::createSpriteExtra(openSpr, [this, id](auto) { openProject(id); });
    menu->addChildAtPosition(openBtn, Anchor::Right, {-62.f, 0.f});

    auto* delSpr = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
    delSpr->setScale(0.7f);
    auto metaCopy = meta;
    auto* delBtn = CCMenuItemExt::createSpriteExtra(delSpr, [this, metaCopy](auto) { onDelete(metaCopy); });
    menu->addChildAtPosition(delBtn, Anchor::Right, {-20.f, 0.f});

    row->addChildAtPosition(menu, Anchor::Center);
    return row;
}

void ProjectsPopup::openProject(std::string const& id) {
    auto loaded = storage::ProjectStore::get().load(id);
    if (!loaded) {
        FLAlertLayer::create("Cannot open project", "<cr>" + loaded.unwrapErr() + "</c>", "OK")->show();
        return;
    }
    auto* popup = EditorPopup::create(loaded.unwrap());
    if (!popup) return;
    popup->setOnClosed([this] { reload(); });
    popup->show();
}

void ProjectsPopup::onNew(CCObject*) {
    NamePopup::create("New project", "Project name", [this](std::string name) {
        auto created = storage::ProjectStore::get().create(name, storage::ProjectStore::starterSource(name));
        if (!created) {
            FLAlertLayer::create("Cannot create project", "<cr>" + created.unwrapErr() + "</c>", "OK")->show();
            return;
        }
        reload();
        openProject(created.unwrap().meta.id);
    })->show();
}

void ProjectsPopup::onImportClipboard(CCObject*) {
    std::string clip = utils::clipboard::read();
    if (clip.empty()) {
        Notification::create("Clipboard is empty", NotificationIcon::Warning)->show();
        return;
    }
    NamePopup::create("Import from clipboard", "Project name", [this, clip](std::string name) {
        auto created = storage::ProjectStore::get().create(name, clip);
        if (!created) {
            FLAlertLayer::create("Cannot create project", "<cr>" + created.unwrapErr() + "</c>", "OK")->show();
            return;
        }
        reload();
        openProject(created.unwrap().meta.id);
    })->show();
}

void ProjectsPopup::onDelete(storage::ProjectMeta const& meta) {
    createQuickPopup(
        "Delete project",
        fmt::format("Delete <cy>{}</c>?\nThe script file is removed; levels already generated in "
                    "Geometry Dash are <cg>kept</c>.",
                    meta.name),
        "Cancel", "Delete",
        [this, id = meta.id](FLAlertLayer*, bool confirmed) {
            if (!confirmed) return;
            auto res = storage::ProjectStore::get().remove(id);
            if (!res) {
                Notification::create("Delete failed: " + res.unwrapErr(), NotificationIcon::Error)->show();
            }
            reload();
        });
}

void ProjectsPopup::onOpenFolder(CCObject*) {
    auto root = storage::ProjectStore::get().root();
    (void)file::createDirectoryAll(root);
    if (!file::openFolder(root)) {
        Notification::create("Projects folder: " + root.string(), NotificationIcon::Info, 4.f)->show();
    }
}

} // namespace gdcode::ui
