#include "gdcode/Catalog.hpp"

#include <algorithm>

namespace gdcode {

// ---------------------------------------------------------------------------
// Object ID provenance (cross-checked by tools/check_gd_api.py):
//   * FlowVix/gd-info-explorer `objects.csv` (dataset the community gddocs
//     project cites for 2.2 object properties), names quoted per entry;
//   * g-js-api/G.js v7 `obj_ids` constants (portals/special objects);
//   * the decoded official RobTop level "Stereo Madness" (grid positions of
//     ids 1/8/39 confirm the block/spike/half-spike mappings + coordinate
//     convention).
// ---------------------------------------------------------------------------

std::vector<ObjectSpec> const& objectCatalog() {
    static std::vector<ObjectSpec> const catalog = {
        // "Black Gradient Square" -- the default 1x1 block.
        {"block", 1, {}},

        // Spikes. Default is id 8 "Black Gradient Spike"; size/color variants
        // from the dataset.
        {"spike", 8,
         {
             {"half", 39},       // "Half Black Gradient Spike"
             {"small", 103},     // "Small Black Gradient Spike"
             {"tiny", 392},      // "Black Gradient Tiny Spike"
             {"ice", 177},       // "Ice Spike"
             {"fake", 191},      // "Fake Black Spike"
             {"invisible", 144}, // "Invisible Spike"
         }},

        // Colorable 2.2 sawblades.
        {"saw", 1705,
         {
             {"large", 1705},  // "Large Saw Blade"
             {"medium", 1706}, // "Medium Saw Blade"
             {"small", 1707},  // "Small Saw Blade"
         }},

        // Orbs (variant required).
        {"orb", -1,
         {
             {"yellow", 36},    // "Yellow Jump Orb"
             {"pink", 141},     // "Pink Jump Orb"
             {"red", 1333},     // "Red Jump Orb"
             {"blue", 84},      // "Blue Gravity Orb"
             {"green", 1022},   // "Green Gravity Orb"
             {"black", 1330},   // "Black Drop Orb"
             {"dash", 1704},    // "Green Dash Orb"
             {"spider", 3004},  // "Spider Orb"
             {"teleport", 3027},// "Teleport Orb"
             {"toggle", 1594},  // "Toggle Orb"
         }},

        // Pads (variant required).
        {"pad", -1,
         {
             {"yellow", 35},   // "Yellow Jump Pad"
             {"pink", 140},    // "Pink Jump Pad"
             {"red", 1332},    // "Red Jump Pad"
             {"blue", 67},     // "Blue Gravity Pad"
             {"spider", 3005}, // "Spider Pad"
         }},

        // Portals (variant required). Gamemode/gravity/size/speed/dual/mirror.
        // Speed semantics verified against G.js obj_ids + gddocs Speed enum.
        {"portal", -1,
         {
             {"cube", 12},
             {"ship", 13},
             {"ball", 47},
             {"ufo", 111},
             {"wave", 660},
             {"robot", 745},
             {"spider", 1331},
             {"swing", 1933},
             {"gravDown", 10},   // "Blue Gravity Portal"
             {"gravUp", 11},     // "Yellow Gravity Portal"
             {"mirrorOn", 45},   // "Orange Mirror Portal"
             {"mirrorOff", 46},  // "Blue Mirror Portal"
             {"mini", 101},      // "Pink Size Portal" (G.js SIZE_MINI)
             {"sizeNormal", 99}, // "Green Size Portal" (G.js SIZE_NORMAL)
             {"speedHalf", 200}, // 0.5x
             {"speed1x", 201},   // 1x
             {"speed2x", 202},   // 2x
             {"speed3x", 203},   // 3x
             {"speed4x", 1334},  // 4x
             {"dualOn", 286},
             {"dualOff", 287},
         },
         /*checkedByDefault=*/true},

        // Start position ("Start Position", kA9=1 semantics handled by GD
        // when placed as an object).
        {"startpos", 31, {}},

        // Secret/user coin ("User Coin"; plain coins need server-side
        // verification and are out of scope).
        {"coin", 1329, {}},
    };
    return catalog;
}

ObjectSpec const* findObjectType(std::string_view type) {
    for (auto const& spec : objectCatalog()) {
        if (spec.type == type) return &spec;
    }
    return nullptr;
}

std::optional<int> resolveObjectId(std::string_view type, std::string_view variant) {
    ObjectSpec const* spec = findObjectType(type);
    if (!spec) return std::nullopt;
    if (variant.empty()) {
        if (spec->defaultId >= 0) return spec->defaultId;
        return std::nullopt; // variant required
    }
    for (auto const& v : spec->variants) {
        if (v.name == variant) return v.gdId;
    }
    return std::nullopt;
}

std::size_t editDistance(std::string_view a, std::string_view b) {
    std::size_t const n = a.size(), m = b.size();
    std::vector<std::size_t> prev(m + 1), cur(m + 1);
    for (std::size_t j = 0; j <= m; ++j) prev[j] = j;
    for (std::size_t i = 1; i <= n; ++i) {
        cur[0] = i;
        for (std::size_t j = 1; j <= m; ++j) {
            std::size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
        }
        std::swap(prev, cur);
    }
    return prev[m];
}

static std::vector<std::string> suggestFrom(std::string_view typo,
                                            std::vector<std::string_view> const& names) {
    std::vector<std::pair<std::size_t, std::string>> scored;
    for (auto name : names) {
        std::size_t d = editDistance(typo, name);
        std::size_t threshold = typo.size() <= 3 ? 1 : 2;
        if (d <= threshold) scored.emplace_back(d, std::string(name));
    }
    std::sort(scored.begin(), scored.end());
    std::vector<std::string> out;
    for (auto& [d, n] : scored) {
        if (out.size() >= 3) break;
        out.push_back(std::move(n));
    }
    return out;
}

std::vector<std::string> suggestObjectTypes(std::string_view typo) {
    std::vector<std::string_view> names;
    for (auto const& spec : objectCatalog()) names.push_back(spec.type);
    names.push_back("obj"); // escape hatch is also a valid "type"
    return suggestFrom(typo, names);
}

std::vector<std::string> suggestVariants(std::string_view type, std::string_view typo) {
    ObjectSpec const* spec = findObjectType(type);
    if (!spec) return {};
    std::vector<std::string_view> names;
    for (auto const& v : spec->variants) names.push_back(v.name);
    return suggestFrom(typo, names);
}

} // namespace gdcode
