#include "framework.hpp"

#include "gdcode/Compiler.hpp"
#include "gdcode/Encoder.hpp"

#include <fstream>
#include <map>
#include <sstream>

using namespace gdcode;

namespace {

// Split "k,v,k,v" into a map (used to compare records key-by-key).
std::map<std::string, std::string> kv(std::string const& record) {
    std::map<std::string, std::string> out;
    std::vector<std::string> parts;
    std::string cur;
    for (char c : record) {
        if (c == ',') {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    parts.push_back(cur);
    for (std::size_t i = 0; i + 1 < parts.size(); i += 2) out[parts[i]] = parts[i + 1];
    return out;
}

std::vector<std::string> records(std::string const& levelString) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : levelString) {
        if (c == ';') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::string readFixture(char const* name) {
    std::ifstream in(std::string(GDCODE_TEST_DIR) + "/fixtures/" + name);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

TEST(number_formatting_matches_gd_conventions) {
    CHECK_EQ(formatGdNumber(15), std::string("15"));
    CHECK_EQ(formatGdNumber(-45), std::string("-45"));
    CHECK_EQ(formatGdNumber(0.5), std::string("0.5"));
    CHECK_EQ(formatGdNumber(1.25), std::string("1.25"));
    CHECK_EQ(formatGdNumber(1.0 / 3.0), std::string("0.333333"));
    CHECK_EQ(formatGdNumber(1e20), std::string("100000000000000000000"));
}

TEST(level_string_layout_header_then_objects) {
    auto r = compile("block 0 0\nspike 1 0\n");
    CHECK(r.ok());
    auto ls = encodeLevelString(r.ir);
    auto recs = records(ls);
    CHECK_EQ(recs.size(), std::size_t{3});
    CHECK(ls.back() == ';');
    CHECK(recs[0].rfind("kS38,", 0) == 0);
    CHECK_EQ(recs[1], std::string("1,1,2,15,3,15"));
    CHECK_EQ(recs[2], std::string("1,8,2,45,3,15"));
}

TEST(header_contains_required_start_keys) {
    auto r = compile("level \"H\" { mode: ship speed: fast mini: true dual: true twoplayer: true platformer: true flip: true }\n");
    CHECK(r.ok());
    auto header = records(encodeLevelString(r.ir))[0];
    auto keys = kv(header);
    CHECK_EQ(keys["kA2"], std::string("1"));  // ship
    CHECK_EQ(keys["kA4"], std::string("2"));  // fast
    CHECK_EQ(keys["kA3"], std::string("1"));  // mini
    CHECK_EQ(keys["kA8"], std::string("1"));  // dual
    CHECK_EQ(keys["kA10"], std::string("1")); // 2 player
    CHECK_EQ(keys["kA22"], std::string("1")); // platformer
    CHECK_EQ(keys["kA11"], std::string("1")); // flip gravity
    CHECK_EQ(keys["kA9"], std::string("0"));  // level start, not startpos
    CHECK_EQ(keys["kA14"], std::string(""));  // empty guideline string
    CHECK_EQ(keys["kA40"], std::string("1")); // 2.2 changes enabled
    CHECK(keys.count("kS38") == 1);
    CHECK(keys.count("kS39") == 1);
}

TEST(speed_enum_maps_to_kA4_values) {
    // gddocs Speed enum: 0 = 1x, 1 = 0.5x, 2 = 2x, 3 = 3x, 4 = 4x
    std::map<std::string, std::string> expect = {
        {"slow", "1"}, {"normal", "0"}, {"fast", "2"}, {"faster", "3"}, {"fastest", "4"}};
    for (auto const& [name, val] : expect) {
        auto r = compile("level \"S\" { speed: " + name + " }\n");
        CHECK(r.ok());
        CHECK_EQ(kv(records(encodeLevelString(r.ir))[0])["kA4"], val);
    }
}

TEST(color_channels_default_and_override) {
    auto r = compile("level \"C\" { bg: #FF8000 ground: #010203 }\n");
    CHECK(r.ok());
    auto header = records(encodeLevelString(r.ir))[0];
    std::string colors = kv(header)["kS38"];
    // exactly five channel entries, editor key layout, player color -1
    std::size_t entries = 1;
    for (char c : colors)
        if (c == '|') ++entries;
    CHECK_EQ(entries, std::size_t{5});
    CHECK_CONTAINS(colors, "1_255_2_128_3_0_11_255_12_255_13_255_4_-1_6_1000_7_1_15_1_18_0_8_1");
    CHECK_CONTAINS(colors, "1_1_2_2_3_3_11_255_12_255_13_255_4_-1_6_1001_7_1_15_1_18_0_8_1");
    CHECK_CONTAINS(colors, "_6_1002_");
    CHECK_CONTAINS(colors, "_6_1003_");
    CHECK_CONTAINS(colors, "_6_1004_");
}

TEST(object_properties_use_documented_keys) {
    auto r = compile("block 2 3 rot:90 scale:0.5 flipX:true flipY:true group:7 color:1004 layer:3 zOrder:5\n");
    CHECK(r.ok());
    auto rec = records(encodeLevelString(r.ir))[1];
    auto keys = kv(rec);
    CHECK_EQ(keys["1"], std::string("1"));
    CHECK_EQ(keys["2"], std::string("75"));
    CHECK_EQ(keys["3"], std::string("105"));
    CHECK_EQ(keys["4"], std::string("1"));
    CHECK_EQ(keys["5"], std::string("1"));
    CHECK_EQ(keys["6"], std::string("90"));
    CHECK_EQ(keys["20"], std::string("3"));
    CHECK_EQ(keys["21"], std::string("1004"));
    CHECK_EQ(keys["25"], std::string("5"));
    CHECK_EQ(keys["32"], std::string("0.5"));
    CHECK_EQ(keys["33"], std::string("7"));
}

TEST(default_properties_are_omitted) {
    auto r = compile("block 0 0\n");
    auto rec = records(encodeLevelString(r.ir))[1];
    CHECK_EQ(rec, std::string("1,1,2,15,3,15"));
}

TEST(matches_records_of_real_official_level_data) {
    // The fixture holds verbatim object records decoded from RobTop's own
    // "Stereo Madness" level. Re-create the same placements with GDCode and
    // check our encoder produces byte-identical records.
    std::string fixture = readFixture("stereo_madness_excerpt.txt");
    CHECK(!fixture.empty());
    CHECK_CONTAINS(fixture, "1,8,2,525,3,15");
    CHECK_CONTAINS(fixture, "1,1,2,1515,3,15");
    CHECK_CONTAINS(fixture, "1,1,2,1635,3,45");
    CHECK_CONTAINS(fixture, "1,1,2,1755,3,75");

    auto r = compile("spike 17 0\nblock 50 0\nblock 54 1\nblock 58 2\n");
    CHECK(r.ok());
    auto recs = records(encodeLevelString(r.ir));
    CHECK_EQ(recs[1], std::string("1,8,2,525,3,15"));
    CHECK_EQ(recs[2], std::string("1,1,2,1515,3,15"));
    CHECK_EQ(recs[3], std::string("1,1,2,1635,3,45"));
    CHECK_EQ(recs[4], std::string("1,1,2,1755,3,75"));

    // And the official header uses the same kA keys we emit.
    for (char const* key : {"kA13", "kA15", "kA16", "kA14", "kA6", "kA7", "kA2",
                            "kA3", "kA8", "kA4", "kA9", "kA10", "kA11"}) {
        CHECK_MSG(fixture.find(std::string(key) + ",") != std::string::npos, key);
        auto header = records(encodeLevelString(r.ir))[0];
        CHECK_MSG(kv(header).count(key) == 1, key);
    }
}

TEST(level_string_is_pure_ascii_and_has_no_newlines) {
    auto r = compile("level \"N\" { desc: \"multi\\nline\" }\nblock 0 0\n");
    auto ls = encodeLevelString(r.ir);
    for (unsigned char c : ls) {
        CHECK(c >= 0x20 && c < 0x7F);
    }
}

TEST(level_length_estimate_uses_speed_and_portals) {
    // 3115.8 units at normal speed (311.58/s) is 10 s -> "short" (key 1)
    auto r = compile("block 103 0\n"); // x = 103*30+15 = 3105 -> just under 10s
    CHECK(r.ok());
    double s = estimateLevelSeconds(r.ir);
    CHECK(s > 9.9 && s < 10.0);
    CHECK_EQ(levelLengthKey(s, false), 0);
    CHECK_EQ(levelLengthKey(s, true), 5);

    // fastest from the start: 4x speed -> much shorter time
    auto f = compile("level \"F\" { speed: fastest }\nblock 103 0\n");
    CHECK(estimateLevelSeconds(f.ir) < 6.0);

    // a slow portal at x=0 slows everything after it down
    auto p = compile("portal speedHalf 0 0\nblock 103 0\n");
    CHECK(estimateLevelSeconds(p.ir) > 12.0);

    CHECK_EQ(levelLengthKey(29.9, false), 1);
    CHECK_EQ(levelLengthKey(59.9, false), 2);
    CHECK_EQ(levelLengthKey(119.9, false), 3);
    CHECK_EQ(levelLengthKey(120.0, false), 4);
    CHECK_EQ(estimateLevelSeconds(compile("").ir), 0.0);
}
