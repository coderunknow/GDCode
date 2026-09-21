#include "LevelNavigation.hpp"

using namespace geode::prelude;

namespace gdcode::backend {

AfterGenerate afterGenerateSetting() {
    auto value = Mod::get()->getSettingValue<std::string>("after-generate");
    if (value == "level-page") return AfterGenerate::OpenLevelPage;
    if (value == "stay") return AfterGenerate::Stay;
    return AfterGenerate::OpenEditor;
}

bool openLevel(GJGameLevel* level, AfterGenerate how,
               std::function<void()> const& beforeSwitch) {
    auto* director = CCDirector::sharedDirector();
    if (!level || how == AfterGenerate::Stay || director->getIsTransitioning()) return false;

    auto* manager = GameManager::sharedState();
    int previousScene = manager->m_sceneEnum;
    CCScene* scene = nullptr;
    switch (how) {
        case AfterGenerate::OpenEditor:
            // GD's returnToLastScene uses 3 for the local level page. Calling
            // the factory alone (v0.1.0) leaves the main menu's stale context,
            // so native editor/pause exit cannot route back correctly.
            manager->m_sceneEnum = 3;
            scene = LevelEditorLayer::scene(level, false);
            break;
        case AfterGenerate::OpenLevelPage:
            // Local level page -> My Levels, as in GD's new/clone flow.
            manager->m_sceneEnum = 2;
            scene = EditLevelLayer::scene(level);
            break;
        case AfterGenerate::Stay:
            return false;
    }
    if (!scene) {
        manager->m_sceneEnum = previousScene;
        log::error("GDCode: failed to create the target scene");
        return false;
    }
    if (beforeSwitch) beforeSwitch();
    director->replaceScene(CCTransitionFade::create(0.5f, scene));
    return true;
}

} // namespace gdcode::backend
