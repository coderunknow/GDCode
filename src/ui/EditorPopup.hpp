#pragma once

#include "../storage/ProjectStore.hpp"
#include "CodeEditor.hpp"

#include "gdcode/Compiler.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>

#include <functional>

namespace gdcode::ui {

/// The main GDCode window: code editor + live diagnostics + generate.
class EditorPopup : public geode::Popup {
public:
    static EditorPopup* create(storage::Project project);

    /// Called after the popup closes (the projects list refreshes itself).
    void setOnClosed(std::function<void()> cb) { m_onClosed = std::move(cb); }

protected:
    bool init(storage::Project project);
    void onClose(cocos2d::CCObject* sender) override;
    void keyDown(cocos2d::enumKeyCodes key, double timestamp) override;

private:
    // actions
    void onCheck(cocos2d::CCObject*);
    void onGenerate(bool forceNewLevel);
    void onCopyAll(cocos2d::CCObject*);
    void onPaste(cocos2d::CCObject*);
    void onUndo(cocos2d::CCObject*);
    void onHelp(cocos2d::CCObject*);

    // pipeline
    gdcode::CompileResult compileCurrent();
    void writeAndOpen(gdcode::LevelIR const& ir, GJGameLevel* target);
    void scheduleLiveCheck();
    void runLiveCheck(float);
    void scheduleAutosave();
    void runAutosave(float);
    bool saveProject(bool announce);

    // ui
    void rebuildProblems(gdcode::CompileResult const& result);
    void updateStatus();
    void setStatus(std::string const& text, cocos2d::ccColor3B color);
    CCMenuItemSpriteExtra* makeButton(char const* text, char const* bg, float width,
                                      std::function<void()> cb);

    storage::Project m_project;
    CodeEditor* m_editor = nullptr;
    geode::ScrollLayer* m_problems = nullptr;
    cocos2d::CCLabelBMFont* m_problemsTitle = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    cocos2d::CCLabelBMFont* m_position = nullptr;
    std::function<void()> m_onClosed;
    bool m_dirty = false;
    bool m_generating = false;
    std::size_t m_lastErrorCount = 0;
    std::size_t m_lastObjectCount = 0;
};

} // namespace gdcode::ui
