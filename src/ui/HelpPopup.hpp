#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

namespace gdcode::ui {

/// In-game language reference (short version of docs/DSL.md).
class HelpPopup : public geode::Popup {
public:
    static HelpPopup* create();

protected:
    bool init();
};

} // namespace gdcode::ui
