#pragma once

#include "../storage/ProjectStore.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>

#include <functional>

namespace gdcode::ui {

/// Small "enter a name" prompt used when creating a project.
class NamePopup : public geode::Popup {
public:
    static NamePopup* create(std::string const& title, std::string const& placeholder,
                             std::function<void(std::string)> onDone);

protected:
    bool init(std::string const& title, std::string const& placeholder,
              std::function<void(std::string)> onDone);
    void submit();

    geode::TextInput* m_input = nullptr;
    std::function<void(std::string)> m_onDone;
};

/// Entry point UI: the list of GDCode projects.
class ProjectsPopup : public geode::Popup {
public:
    static ProjectsPopup* create();

protected:
    bool init();
    void reload();
    void openProject(std::string const& id);
    void onNew(cocos2d::CCObject*);
    void onImportClipboard(cocos2d::CCObject*);
    void onDelete(storage::ProjectMeta const& meta);
    void onOpenFolder(cocos2d::CCObject*);

    cocos2d::CCNode* makeRow(storage::ProjectMeta const& meta, float width);

    geode::ScrollLayer* m_list = nullptr;
    cocos2d::CCLabelBMFont* m_empty = nullptr;
};

} // namespace gdcode::ui
