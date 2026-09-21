#pragma once

#include <Geode/Geode.hpp>

#include <functional>

namespace gdcode::backend {

enum class AfterGenerate { OpenEditor, OpenLevelPage, Stay };
AfterGenerate afterGenerateSetting();

/// Hand off to native GD navigation. beforeSwitch runs synchronously, exactly
/// once on success, to dismiss the owning UI before replacing its scene.
/// Failure / Stay leaves the current scene and UI intact. No callback is kept.
bool openLevel(GJGameLevel* level, AfterGenerate how,
               std::function<void()> const& beforeSwitch);

} // namespace gdcode::backend
