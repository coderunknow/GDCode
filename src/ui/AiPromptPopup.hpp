#pragma once

#include <Geode/ui/Popup.hpp>

namespace gdcode::ui {

/// Offline, read-only view of the full coding-agent prompt, with clipboard export.
class AiPromptPopup : public geode::Popup {
public:
    static AiPromptPopup* create();

protected:
    bool init();
};

} // namespace gdcode::ui
