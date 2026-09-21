#include "gdcode/Encoder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace gdcode {

std::string formatGdNumber(double value) {
    if (std::isnan(value)) return "0";
    if (std::isinf(value)) return value > 0 ? "999999" : "-999999";
    double rounded = std::round(value);
    if (value == rounded && std::fabs(value) < 1e15) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(rounded));
        return buf;
    }
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%.6f", value);
    // strip trailing zeros (keep at least one decimal digit)
    std::string s(buf);
    if (s.find('.') != std::string::npos) {
        while (s.size() > 2 && s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    return s;
}

namespace {

// Default channel colors of a level freshly created by the 2.2 editor
// (verified against a decrypted CCLocalLevels.dat: BG 40/125/255,
// Ground 0/102/255; the remaining channels default to white).
struct ChannelDefault {
    int channel;
    int r, g, b;
};
ChannelDefault const kChannelDefaults[] = {
    {1000, 40, 125, 255},  // BG
    {1001, 0, 102, 255},   // Ground
    {1002, 255, 255, 255}, // Line
    {1003, 255, 255, 255}, // 3DL
    {1004, 255, 255, 255}, // Obj
};

// One kS38 entry, written with the same keys and key order the 2.2 editor
// uses:  1_r_2_g_3_b_11_255_12_255_13_255_4_-1_6_<channel>_7_<opacity>_15_1_18_0_8_1
//   4  = player color (-1: none)      6  = channel id
//   7  = opacity                      8  = toggle opacity
//   11..13 = "to" color (unused for static colors)
//   15 = to-opacity                   18 = (always 0 in editor output)
std::string encodeColor(ColorIR const& c) {
    std::string s;
    s += "1_" + std::to_string(std::clamp(c.r, 0, 255));
    s += "_2_" + std::to_string(std::clamp(c.g, 0, 255));
    s += "_3_" + std::to_string(std::clamp(c.b, 0, 255));
    s += "_11_255_12_255_13_255";
    s += "_4_-1";
    s += "_6_" + std::to_string(c.channelId);
    s += "_7_" + formatGdNumber(std::clamp(c.opacity, 0.0, 1.0));
    s += "_15_1_18_0_8_1";
    if (c.blending) s += "_5_1";
    return s;
}

std::string encodeObject(ObjectIR const& o) {
    std::string s;
    s += "1," + std::to_string(o.id);
    s += ",2," + formatGdNumber(o.x);
    s += ",3," + formatGdNumber(o.y);
    if (o.flipX) s += ",4,1";
    if (o.flipY) s += ",5,1";
    if (o.rotation != 0.0) s += ",6," + formatGdNumber(o.rotation);
    if (o.specialChecked) s += ",13,1";
    if (o.editorLayer >= 0) s += ",20," + std::to_string(o.editorLayer);
    if (o.colorChannel >= 0) s += ",21," + std::to_string(o.colorChannel);
    if (o.zLayer != 0) s += ",24," + std::to_string(o.zLayer);
    if (o.zOrder != 0) s += ",25," + std::to_string(o.zOrder);
    if (o.scale != 1.0) s += ",32," + formatGdNumber(o.scale);
    if (o.groupId >= 0) s += ",33," + std::to_string(o.groupId);
    return s;
}

} // namespace

std::string encodeLevelString(LevelIR const& ir) {
    auto const& s = ir.settings;
    std::string out;

    // --- colors (kS38): script channels merged over the editor defaults ---
    std::map<int, ColorIR> channels;
    for (auto const& d : kChannelDefaults) {
        ColorIR c;
        c.channelId = d.channel;
        c.r = d.r;
        c.g = d.g;
        c.b = d.b;
        channels[d.channel] = c;
    }
    for (auto const& c : s.colors) channels[c.channelId] = c;

    out += "kS38,";
    bool first = true;
    for (auto const& [id, c] : channels) {
        if (!first) out += "|";
        first = false;
        out += encodeColor(c);
    }

    // --- level start object (kA* keys, per the 2.2 level-start spec) ---
    out += ",kA13,0";          // song offset
    out += ",kA15,0";          // fade in
    out += ",kA16,0";          // fade out
    out += ",kA14,";           // guidelines (none)
    out += ",kA6,0";           // background texture
    out += ",kA7,0";           // ground texture
    out += ",kA25,0";          // middleground texture
    out += ",kA17,0";          // ground line
    out += ",kA18,0";          // font
    out += ",kS39,0";          // color page
    out += ",kA2," + std::to_string(s.mode);
    out += ",kA3," + std::to_string(s.mini ? 1 : 0);
    out += ",kA8," + std::to_string(s.dual ? 1 : 0);
    out += ",kA4," + std::to_string(s.speed);
    out += ",kA9,0";           // this is the level start, not a start pos
    out += ",kA10," + std::to_string(s.twoPlayer ? 1 : 0);
    out += ",kA22," + std::to_string(s.platformer ? 1 : 0);
    // 2.2 compatibility settings, set the way the 2.2 editor initialises a
    // brand-new level (all "fix"/"modern behaviour" flags enabled). Users can
    // change any of them afterwards in the editor's level settings.
    out += ",kA27,1";          // allow multi-rotation
    out += ",kA40,1";          // enable 2.2 changes
    out += ",kA41,1";          // allow static rotate
    out += ",kA42,1";          // reverse sync
    out += ",kA43,0";          // no time penalty (platformer)
    out += ",kA28,0";          // mirror mode at start
    out += ",kA29,0";          // rotate gameplay at start
    out += ",kA31,1";          // enable player squeeze
    out += ",kA32,1";          // fix gravity bug
    out += ",kA36,0";          // spawn group
    out += ",kA37,1";          // dynamic level height
    out += ",kA38,1";          // sort groups
    out += ",kA39,1";          // fix radius collision
    out += ",kA45,1";          // decrease boost slide
    out += ",kA33,1";          // fix negative scale
    out += ",kA34,1";          // fix robot jump
    out += ",kA35,0";          // reset camera
    out += ",kA11," + std::to_string(s.flipGravity ? 1 : 0);
    out += ";";

    // --- objects ---
    for (auto const& o : ir.objects) {
        out += encodeObject(o);
        out += ";";
    }
    return out;
}

} // namespace gdcode
