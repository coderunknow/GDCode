#include "gdcode/Ir.hpp"

#include <sstream>

namespace gdcode {

namespace {

void escape(std::ostringstream& out, std::string const& s) {
    for (char c : s) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\t': out << "\\t"; break;
            case '\r': out << "\\r"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out << buf;
                } else {
                    out << c;
                }
        }
    }
}

void jsonString(std::ostringstream& out, std::string const& s) {
    out << '"';
    escape(out, s);
    out << '"';
}

/// Deterministic number rendering for snapshots.
void jsonNumber(std::ostringstream& out, double v) {
    if (v == static_cast<long long>(v) && v >= -1e15 && v <= 1e15) {
        out << static_cast<long long>(v);
    } else {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%.6f", v);
        out << buf;
    }
}

} // namespace

std::string irToJson(LevelIR const& ir) {
    std::ostringstream out;
    auto const& s = ir.settings;

    out << "{\n  \"settings\": {\n";
    out << "    \"name\": ";
    jsonString(out, s.name);
    out << ",\n    \"description\": ";
    jsonString(out, s.description);
    out << ",\n    \"audioTrack\": " << s.audioTrack;
    out << ",\n    \"customSongId\": " << s.customSongId;
    out << ",\n    \"speed\": " << s.speed;
    out << ",\n    \"mode\": " << s.mode;
    out << ",\n    \"mini\": " << (s.mini ? "true" : "false");
    out << ",\n    \"dual\": " << (s.dual ? "true" : "false");
    out << ",\n    \"flipGravity\": " << (s.flipGravity ? "true" : "false");
    out << ",\n    \"twoPlayer\": " << (s.twoPlayer ? "true" : "false");
    out << ",\n    \"platformer\": " << (s.platformer ? "true" : "false");
    if (s.seed) {
        out << ",\n    \"seed\": " << *s.seed;
    }
    out << ",\n    \"colors\": [";
    for (std::size_t i = 0; i < s.colors.size(); ++i) {
        auto const& c = s.colors[i];
        if (i) out << ", ";
        out << "{\"channel\": " << c.channelId << ", \"r\": " << c.r
            << ", \"g\": " << c.g << ", \"b\": " << c.b
            << ", \"blending\": " << (c.blending ? "true" : "false")
            << ", \"opacity\": ";
        jsonNumber(out, c.opacity);
        out << "}";
    }
    out << "]\n  },\n  \"objects\": [\n";
    for (std::size_t i = 0; i < ir.objects.size(); ++i) {
        auto const& o = ir.objects[i];
        out << "    {\"id\": " << o.id << ", \"x\": ";
        jsonNumber(out, o.x);
        out << ", \"y\": ";
        jsonNumber(out, o.y);
        if (o.rotation != 0.0) out << ", \"rot\": " << o.rotation;
        if (o.scale != 1.0) out << ", \"scale\": " << o.scale;
        if (o.flipX) out << ", \"flipX\": true";
        if (o.flipY) out << ", \"flipY\": true";
        if (o.groupId >= 0) out << ", \"group\": " << o.groupId;
        if (o.colorChannel >= 0) out << ", \"color\": " << o.colorChannel;
        if (o.editorLayer >= 0) out << ", \"layer\": " << o.editorLayer;
        if (o.zOrder != 0) out << ", \"zOrder\": " << o.zOrder;
        if (o.specialChecked) out << ", \"checked\": true";
        out << ", \"dsl\": ";
        jsonString(out, o.dslType + (o.dslVariant.empty() ? "" : " " + o.dslVariant));
        out << "}";
        if (i + 1 < ir.objects.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
    return out.str();
}

} // namespace gdcode
