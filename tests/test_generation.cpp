#include "framework.hpp"

#include "gdcode/Catalog.hpp"
#include "gdcode/Compiler.hpp"
#include "gdcode/Encoder.hpp"

using namespace gdcode;

TEST(grid_to_gd_units_matches_official_level_convention) {
    // From the decoded official level "Stereo Madness": block id 1 at grid
    // column 50 sits at x=1515,y=15 and spike id 8 at column 17 at x=525,y=15.
    auto r = compile("block 50 0\nspike 17 0\n");
    CHECK(r.ok());
    CHECK_EQ(r.ir.objects[0].id, 1);
    CHECK_EQ(r.ir.objects[0].x, 1515.0);
    CHECK_EQ(r.ir.objects[0].y, 15.0);
    CHECK_EQ(r.ir.objects[1].id, 8);
    CHECK_EQ(r.ir.objects[1].x, 525.0);
    CHECK_EQ(r.ir.objects[1].y, 15.0);
}

TEST(fractional_coordinates_are_allowed) {
    auto r = compile("block 0.5 0\n");
    CHECK(r.ok());
    CHECK_EQ(r.ir.objects[0].x, 30.0);
}

TEST(every_catalog_entry_resolves_to_a_positive_gd_id) {
    for (auto const& spec : objectCatalog()) {
        if (spec.variants.empty()) {
            CHECK_MSG(spec.defaultId > 0, std::string(spec.type));
        }
        for (auto const& v : spec.variants) {
            CHECK_MSG(v.gdId > 0, std::string(spec.type) + " " + std::string(v.name));
            auto id = resolveObjectId(spec.type, v.name);
            CHECK(id.has_value() && *id == v.gdId);
        }
    }
    // Spot checks against the community object-id dataset.
    CHECK_EQ(*resolveObjectId("block", ""), 1);
    CHECK_EQ(*resolveObjectId("spike", ""), 8);
    CHECK_EQ(*resolveObjectId("spike", "half"), 39);
    CHECK_EQ(*resolveObjectId("orb", "yellow"), 36);
    CHECK_EQ(*resolveObjectId("pad", "yellow"), 35);
    CHECK_EQ(*resolveObjectId("portal", "ship"), 13);
    CHECK_EQ(*resolveObjectId("portal", "speed4x"), 1334);
    CHECK_EQ(*resolveObjectId("startpos", ""), 31);
    CHECK_EQ(*resolveObjectId("coin", ""), 1329);
    CHECK(!resolveObjectId("orb", "").has_value());
    CHECK(!resolveObjectId("nope", "").has_value());
}

TEST(portals_are_emitted_checked_like_the_editor_does) {
    auto r = compile("portal ship 10 3\nblock 1 1\n");
    CHECK(r.ok());
    CHECK(r.ir.objects[0].specialChecked);
    CHECK(!r.ir.objects[1].specialChecked);
    CHECK_CONTAINS(encodeLevelString(r.ir), "1,13,2,315,3,105,13,1;");
}

TEST(same_source_and_seed_produce_identical_output) {
    std::string src =
        "level \"R\" { seed: 1234 }\n"
        "repeat 200 as i { spike i random(0, 5) rot:random(0, 359) }\n";
    auto a = compile(src);
    auto b = compile(src);
    CHECK(a.ok() && b.ok());
    CHECK(irToJson(a.ir) == irToJson(b.ir));
    CHECK(encodeLevelString(a.ir) == encodeLevelString(b.ir));
}

TEST(different_seed_changes_random_output) {
    std::string body = "repeat 50 as i { spike i random(0, 100) }\n";
    auto a = compile("level \"R\" { seed: 1 }\n" + body);
    auto b = compile("level \"R\" { seed: 2 }\n" + body);
    CHECK(a.ok() && b.ok());
    CHECK(irToJson(a.ir) != irToJson(b.ir));
}

TEST(random_without_seed_is_deterministic_and_flagged) {
    std::string src = "repeat 20 as i { spike i random(0, 9) }\n";
    auto a = compile(src);
    auto b = compile(src);
    CHECK(a.ok());
    CHECK(irToJson(a.ir) == irToJson(b.ir));
    bool info = false;
    for (auto const& d : a.diagnostics.items())
        if (d.code == "default-seed") info = true;
    CHECK(info);
    CHECK(a.stats.randomUsed);
    CHECK(!a.stats.seedExplicit);
}

TEST(random_is_inclusive_and_within_bounds) {
    auto r = compile("level \"R\" { seed: 99 }\nrepeat 500 { block random(2, 4) 0 }\n");
    CHECK(r.ok());
    bool saw2 = false, saw4 = false;
    for (auto const& o : r.ir.objects) {
        double grid = (o.x - 15.0) / 30.0;
        CHECK(grid >= 2.0 && grid <= 4.0);
        if (grid == 2.0) saw2 = true;
        if (grid == 4.0) saw4 = true;
    }
    CHECK(saw2 && saw4);
}

TEST(stats_are_reported) {
    auto r = compile("repeat 10 as i { block i 0 }\n");
    CHECK_EQ(r.stats.objectCount, std::size_t{10});
    CHECK(r.stats.expandedStatements >= 11);
}

TEST(large_but_legal_script_stays_within_time_budget) {
    // 20k objects through nested repeats + a pattern; must finish well within
    // the default 2s budget without hitting any limit.
    std::string src =
        "define col(x, h) { repeat h as j { block x j } }\n"
        "repeat 200 as i { col(i, 100) }\n";
    auto r = compile(src);
    CHECK(r.ok());
    CHECK_EQ(r.ir.objects.size(), std::size_t{20000});
}
