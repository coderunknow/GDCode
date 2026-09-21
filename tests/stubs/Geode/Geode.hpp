#pragma once
// Deliberately small test double for LevelNavigation.cpp, NOT a GD runtime.
// It records factory/context/transition order; cannot prove native pause UI.
#include <string>
#include <vector>

namespace navigation_test {
inline std::vector<std::string> events;
inline bool failScene = false;
inline std::string setting = "editor";
}

struct GJGameLevel {};
struct GameManager {
    int m_sceneEnum = 0;
    static GameManager* sharedState() { static GameManager m; return &m; }
};
namespace cocos2d {
struct CCScene {};
struct CCDirector {
    bool transitioning = false;
    CCScene* current = nullptr;
    static CCDirector* sharedDirector() { static CCDirector d; return &d; }
    bool getIsTransitioning() { return transitioning; }
    void replaceScene(CCScene* scene) {
        navigation_test::events.push_back("replace");
        current = scene;
        transitioning = true;
    }
};
struct CCTransitionFade {
    static CCScene* create(float, CCScene* scene) { return scene; }
};
}
struct LevelEditorLayer {
    static cocos2d::CCScene* scene(GJGameLevel*, bool) {
        navigation_test::events.push_back("editor:" + std::to_string(GameManager::sharedState()->m_sceneEnum));
        static cocos2d::CCScene s;
        return navigation_test::failScene ? nullptr : &s;
    }
};
struct EditLevelLayer {
    static cocos2d::CCScene* scene(GJGameLevel*) {
        navigation_test::events.push_back("page:" + std::to_string(GameManager::sharedState()->m_sceneEnum));
        static cocos2d::CCScene s;
        return navigation_test::failScene ? nullptr : &s;
    }
};
namespace geode {
struct Mod {
    static Mod* get() { static Mod m; return &m; }
    template<class T> T getSettingValue(char const*) { return navigation_test::setting; }
};
namespace log { inline void error(char const*) {} }
namespace prelude { using namespace cocos2d; using geode::Mod; namespace log = geode::log; }
}
