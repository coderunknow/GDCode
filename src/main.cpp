// GDCode - write Geometry Dash levels as code.
//
// Entry point: a "</>" button in the main menu's bottom row opens the project
// list. Everything else lives in ui/ (editor + popups), storage/ (projects on
// disk) and backend/ (turning compiler output into real GJGameLevel objects).

#include "ui/ProjectsPopup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>

using namespace geode::prelude;

class $modify(GDCodeMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto* menu = this->getChildByID("bottom-menu");
        if (!menu) {
            log::warn("GDCode: MenuLayer has no 'bottom-menu' node; button not added");
            return true;
        }

        auto* label = CCLabelBMFont::create("</>", "bigFont.fnt");
        auto* sprite = CircleButtonSprite::create(label, CircleBaseColor::Green, CircleBaseSize::MediumAlt);
        auto* button = CCMenuItemExt::createSpriteExtra(sprite, [](auto) {
            if (auto* popup = gdcode::ui::ProjectsPopup::create()) popup->show();
        });
        button->setID("gdcode-button"_spr);
        menu->addChild(button);
        menu->updateLayout();
        return true;
    }
};

$execute {
    log::info("GDCode {} loaded", Mod::get()->getVersion().toVString());
}
