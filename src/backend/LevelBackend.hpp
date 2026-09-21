#pragma once

// The only place in the mod that touches Geometry Dash level objects.
// Everything above this layer works on gdcode::LevelIR; everything below it
// is GD's own API (GJGameLevel, GameLevelManager, LocalLevelManager,
// ZipUtils, LevelEditorLayer).

#include <Geode/Geode.hpp>

#include "gdcode/Ir.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gdcode::backend {

/// How a project remembers the level it generated last time.
struct LevelLink {
    std::string levelName;      ///< GJGameLevel::m_levelName we wrote
    std::string fingerprint;    ///< hex FNV-1a of the m_levelString we wrote
};

/// Result of looking up a project's linked level among the local levels.
struct LinkedLevelLookup {
    GJGameLevel* level = nullptr; ///< null: nothing to update, create a new level
    bool modifiedSinceGeneration = false; ///< level string differs from what we wrote
    bool ambiguous = false;       ///< several same-named, non-matching levels
};

/// Find the level a project generated earlier.
LinkedLevelLookup findLinkedLevel(std::optional<LevelLink> const& link);

struct WriteResult {
    GJGameLevel* level = nullptr;
    bool createdNew = false;
    LevelLink link;
    std::size_t rawBytes = 0;
    std::size_t compressedBytes = 0;
    std::string error; ///< non-empty => nothing was written
};

/// Encode the IR and write it into `target` (or into a freshly created local
/// level when `target` is null). The compressed level string is decompressed
/// again and compared with the raw encoding before anything is touched, so a
/// broken round-trip can never produce a corrupt level.
WriteResult writeLevel(LevelIR const& ir, GJGameLevel* target);

/// Hex FNV-1a 64 of a string (used for the level fingerprint).
std::string fingerprint(std::string const& data);

enum class AfterGenerate { OpenEditor, OpenLevelPage, Stay };
AfterGenerate afterGenerateSetting();

/// Perform the configured post-generation navigation.
void openLevel(GJGameLevel* level, AfterGenerate how);

} // namespace gdcode::backend
