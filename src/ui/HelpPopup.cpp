#include "HelpPopup.hpp"
#include "AiPromptPopup.hpp"

#include "gdcode/Catalog.hpp"

#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextArea.hpp>

using namespace geode::prelude;

namespace gdcode::ui {

HelpPopup* HelpPopup::create() {
    auto* ret = new HelpPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool HelpPopup::init() {
    if (!Popup::init(420.f, 280.f, "GJ_square01.png")) return false;
    this->setID("help-popup"_spr);
    this->setTitle("GDCode quick reference", "goldFont.fnt", 0.7f, 18.f);

    std::string text =
        "COORDINATES\n"
        "  x y are in blocks (1 block = 30 units). y = 0 sits on the ground.\n"
        "\n"
        "OBJECTS  <type> [variant] <x> <y> [prop:value ...]\n";
    for (auto const& spec : objectCatalog()) {
        text += "  " + std::string(spec.type);
        if (!spec.variants.empty()) {
            text += "  variants: ";
            for (std::size_t i = 0; i < spec.variants.size(); ++i) {
                if (i) text += ", ";
                text += std::string(spec.variants[i].name);
            }
            if (spec.defaultId < 0) text += "  (variant required)";
        }
        text += "\n";
    }
    text +=
        "  obj <id> <x> <y>        any raw GD object id (1-4539)\n"
        "  props: rot scale flipX flipY group color layer zOrder\n"
        "\n"
        "LEVEL SETTINGS\n"
        "  level \"Name\" { desc:\"...\" song:0 customSong:0 speed:normal mode:cube\n"
        "    mini:false dual:false flip:false twoplayer:false platformer:false\n"
        "    bg:#287DC8 ground:#0F5FA8 line:#FFFFFF object:#FFFFFF seed:1 }\n"
        "  speed: slow normal fast faster fastest\n"
        "  mode: cube ship ball ufo wave robot spider swing\n"
        "\n"
        "PROGRAMMING\n"
        "  var name = expr             define once per scope\n"
        "  repeat N as i { ... }       i runs 0..N-1\n"
        "  define name(a, b) { ... }   reusable pattern, call: name(1, 2)\n"
        "  random(lo, hi)              whole number, seeded by level seed\n"
        "  expressions: + - * / ( )    comments: # ... or // ...\n"
        "  write `block 5 -1` for negative arguments (space before '-', none after)\n"
        "\n"
        "EDITOR KEYS\n"
        "  Arrows/Home/End/PgUp/PgDn move, Tab indents, Enter auto-indents,\n"
        "  Ctrl+V pastes, Ctrl+Z / Ctrl+Y undo / redo. Tap a problem to jump to it.\n";

    auto* scroll = ScrollLayer::create({396.f, 190.f});
    scroll->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(0.f));
    scroll->setTouchEnabled(true);

    auto* area = SimpleTextArea::create(text, "chatFont.fnt", 0.5f, 388.f);
    area->setWrappingMode(WrappingMode::WORD_WRAP);
    area->setAnchorPoint({0.f, 0.5f});
    auto* holder = CCNode::create();
    holder->setContentSize({396.f, area->getHeight() + 8.f});
    holder->addChildAtPosition(area, Anchor::Left, {4.f, 0.f});
    scroll->m_contentLayer->addChild(holder);
    scroll->m_contentLayer->updateLayout();

    auto* bg = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
    bg->setColor({0, 0, 0});
    bg->setOpacity(110);
    bg->setContentSize({404.f, 198.f});
    m_mainLayer->addChildAtPosition(bg, Anchor::Center, {0.f, 6.f});
    bg->addChildAtPosition(scroll, Anchor::BottomLeft, {4.f, 4.f});
    auto* sprite = ButtonSprite::create("AI Prompt", 100, 0, 0.55f, true,
                                         "bigFont.fnt", "GJ_button_04.png", 26.f);
    auto* prompt = CCMenuItemExt::createSpriteExtra(sprite, [](auto) {
        if (auto* popup = AiPromptPopup::create()) popup->show();
    });
    prompt->setID("ai-prompt-button"_spr);
    m_buttonMenu->addChildAtPosition(prompt, Anchor::Bottom, {0.f, 22.f});
    scroll->scrollToTop();
    handleTouchPriority(this);
    return true;
}

} // namespace gdcode::ui
