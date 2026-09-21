#pragma once

#include "Span.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gdcode {

/// One Geometry Dash object, positions already converted to GD units
/// (1 grid cell == 30 units; cell (gx, gy) centre == (gx*30+15, gy*30+15)).
struct ObjectIR {
    int id = 0;
    double x = 0.0;
    double y = 0.0;

    double rotation = 0.0;
    double scale = 1.0;
    bool flipX = false;
    bool flipY = false;

    int groupId = -1;      ///< key 33 (single group id); -1 = unset
    int colorChannel = -1; ///< key 21 (main color channel); -1 = unset
    int editorLayer = -1;  ///< key 20; -1 = unset
    int zLayer = 0;        ///< key 24
    int zOrder = 0;        ///< key 25
    /// key 13 "special object checked": the editor sets this on freshly
    /// placed portals; it makes speed portals count for song sync/length.
    bool specialChecked = false;

    /// DSL source that produced this object (for diagnostics / tooling).
    std::string dslType;
    std::string dslVariant;
    SourceSpan span{};
};

/// One initialized color channel (kS38 entry).
struct ColorIR {
    int channelId = 0; ///< 1000 BG, 1001 G1, 1002 Line, 1003 3DL, 1004 Obj, 1..999 custom
    int r = 255, g = 255, b = 255;
    bool blending = false;
    double opacity = 1.0;
};

/// Level-wide settings (the `kA*` header object of the level string).
struct LevelSettingsIR {
    std::string name = "Untitled";
    std::string description;

    int audioTrack = 0;  ///< built-in soundtrack index (GJGameLevel::m_audioTrack)
    int customSongId = 0;///< custom (Newgrounds) song id, 0 = none (m_songID)
    int speed = 0;       ///< kA4: 0=1x 1=0.5x 2=2x 3=3x 4=4x
    int mode = 0;        ///< kA2 gamemode: 0 cube ... 7 swing
    bool mini = false;   ///< kA3
    bool dual = false;   ///< kA8
    bool flipGravity = false; ///< kA11
    bool twoPlayer = false;   ///< kA10
    bool platformer = false;  ///< kA22

    /// Color channels to initialize. The encoder always emits the five
    /// standard channels (BG/G1/Line/3DL/Obj) with defaults when absent.
    std::vector<ColorIR> colors;

    /// RNG seed for `random()`; deterministic generation requirement.
    std::optional<std::uint64_t> seed;
};

/// The complete intermediate representation handed to the GD backend.
struct LevelIR {
    LevelSettingsIR settings;
    std::vector<ObjectIR> objects;

    bool empty() const { return objects.empty(); }
};

/// Serialize the IR to a stable, deterministic JSON document.
/// Used by golden/snapshot tests and debugging tooling.
std::string irToJson(LevelIR const& ir);

} // namespace gdcode
