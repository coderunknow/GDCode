#include "AiPromptPopup.hpp"

#include "AiCodingPrompt.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextArea.hpp>

using namespace geode::prelude;

namespace gdcode::ui {

AiPromptPopup* AiPromptPopup::create() {
    auto* ret = new AiPromptPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool AiPromptPopup::init() {
    if (!Popup::init(460.f, 290.f, "GJ_square01.png")) return false;
    this->setID("ai-prompt-popup"_spr);
    this->setTitle("GDCode AI coding prompt", "goldFont.fnt", 0.7f, 18.f);

    auto* scroll = ScrollLayer::create({436.f, 206.f});
    scroll->setID("prompt-scroll");
    scroll->setTouchEnabled(true);
    scroll->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(0.f));

    auto* area = SimpleTextArea::create(content::kAiCodingPrompt, "chatFont.fnt", 0.5f, 424.f);
    area->setWrappingMode(WrappingMode::WORD_WRAP);
    auto* holder = CCNode::create();
    holder->setContentSize({436.f, area->getHeight() + 8.f});
    holder->addChildAtPosition(area, Anchor::Center);
    scroll->m_contentLayer->addChild(holder);
    scroll->m_contentLayer->updateLayout();

    auto* bg = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
    bg->setColor({0, 0, 0});
    bg->setOpacity(110);
    bg->setContentSize({444.f, 214.f});
    m_mainLayer->addChildAtPosition(bg, Anchor::Center, {0.f, 0.f});
    bg->addChildAtPosition(scroll, Anchor::BottomLeft, {4.f, 4.f});

    auto* sprite = ButtonSprite::create("Copy Prompt", 140, 0, 0.55f, true,
                                         "bigFont.fnt", "GJ_button_01.png", 26.f);
    auto* copy = CCMenuItemExt::createSpriteExtra(sprite, [](auto) {
        // Copy the complete canonical text, not wrapped labels or a viewport slice.
        if (utils::clipboard::write(content::kAiCodingPrompt)) {
            Notification::create("Full prompt copied - paste into your AI", NotificationIcon::Success)->show();
        } else {
            Notification::create("Clipboard unavailable - prompt is still readable here",
                                 NotificationIcon::Error)->show();
        }
    });
    copy->setID("copy-prompt");
    m_buttonMenu->addChildAtPosition(copy, Anchor::Bottom, {0.f, 20.f});
    scroll->scrollToTop();
    handleTouchPriority(this);
    return true;
}

} // namespace gdcode::ui
