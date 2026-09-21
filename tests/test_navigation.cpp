#include "framework.hpp"
#include "../src/backend/LevelNavigation.hpp"

using namespace gdcode::backend;
using namespace navigation_test;
using namespace cocos2d;

static void reset() {
    events.clear();
    failScene = false;
    GameManager::sharedState()->m_sceneEnum = 99; // stale / unrelated route
    *CCDirector::sharedDirector() = {};
}

TEST(editor_handoff_sets_return_route_and_closes_before_replace) {
    reset();
    GJGameLevel level;
    CHECK(openLevel(&level, AfterGenerate::OpenEditor, [] { events.push_back("close"); }));
    CHECK(events == std::vector<std::string>({"editor:3", "close", "replace"}));
    CHECK_EQ(GameManager::sharedState()->m_sceneEnum, 3);
}

TEST(level_page_handoff_sets_my_levels_route) {
    reset();
    GJGameLevel level;
    CHECK(openLevel(&level, AfterGenerate::OpenLevelPage, [] { events.push_back("close"); }));
    CHECK(events == std::vector<std::string>({"page:2", "close", "replace"}));
}

TEST(stay_null_and_transition_leave_ui_and_context_intact) {
    reset();
    GJGameLevel level;
    auto close = [] { events.push_back("close"); };
    CHECK(!openLevel(&level, AfterGenerate::Stay, close));
    CHECK(!openLevel(nullptr, AfterGenerate::OpenEditor, close));
    CCDirector::sharedDirector()->transitioning = true;
    CHECK(!openLevel(&level, AfterGenerate::OpenEditor, close));
    CHECK(events.empty());
    CHECK_EQ(GameManager::sharedState()->m_sceneEnum, 99);
}

TEST(factory_failure_restores_context_and_does_not_close) {
    for (auto how : {AfterGenerate::OpenEditor, AfterGenerate::OpenLevelPage}) {
        reset();
        failScene = true;
        GJGameLevel level;
        CHECK(!openLevel(&level, how, [] { events.push_back("close"); }));
        CHECK_EQ(events.size(), std::size_t{1});
        CHECK_EQ(GameManager::sharedState()->m_sceneEnum, 99);
        CHECK(CCDirector::sharedDirector()->current == nullptr);
    }
}

TEST(repeated_entry_and_duplicate_handoff) {
    GJGameLevel level;
    for (int i = 0; i < 10; ++i) {
        reset(); // stand-in for returning to the menu, not a simulated GD exit
        int closed = 0;
        CHECK(openLevel(&level, AfterGenerate::OpenEditor, [&] { ++closed; }));
        CHECK(!openLevel(&level, AfterGenerate::OpenEditor, [&] { ++closed; }));
        CHECK_EQ(closed, 1);
        CHECK_EQ(events.size(), std::size_t{2});
    }
}

TEST(settings_preserve_existing_choices_and_default) {
    for (auto const& value : {"editor", "level-page", "stay", "unknown"}) {
        setting = value;
        auto expected = setting == "stay" ? AfterGenerate::Stay :
                        setting == "level-page" ? AfterGenerate::OpenLevelPage : AfterGenerate::OpenEditor;
        CHECK(afterGenerateSetting() == expected);
    }
}
