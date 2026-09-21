#include "framework.hpp"

#include "gdcode/Compiler.hpp"

using namespace gdcode;

namespace {
bool hasCode(CompileResult const& r, std::string const& code) {
    for (auto const& d : r.diagnostics.items())
        if (d.code == code) return true;
    return false;
}
Diagnostic const* find(CompileResult const& r, std::string const& code) {
    for (auto const& d : r.diagnostics.items())
        if (d.code == code) return &d;
    return nullptr;
}
} // namespace

TEST(unknown_object_type_gets_suggestion) {
    auto r = compile("spirke 4 1\n");
    CHECK(!r.ok());
    auto d = find(r, "unknown-object");
    CHECK(d != nullptr);
    if (d) {
        CHECK_CONTAINS(d->suggestion, "spike");
        CHECK_EQ(d->span.begin.line, std::size_t{1});
        CHECK_EQ(d->span.begin.column, std::size_t{1});
    }
}

TEST(missing_and_unknown_variant) {
    auto r = compile("orb 4 1\nportal shp 4 1\n");
    CHECK(!r.ok());
    CHECK(hasCode(r, "missing-variant"));
    auto d = find(r, "unknown-variant");
    CHECK(d != nullptr);
    if (d) CHECK_CONTAINS(d->suggestion, "ship");
}

TEST(variables_scopes_and_shadowing_rules) {
    auto r = compile("var h = 2\nrepeat 2 as i {\n  var t = h + i\n  block i t\n}\nblock 0 h\n");
    CHECK(r.ok());
    CHECK_EQ(r.ir.objects.size(), std::size_t{3});
    CHECK_EQ(r.ir.objects[0].y, 2 * 30 + 15.0);
    CHECK_EQ(r.ir.objects[1].y, 3 * 30 + 15.0);

    // t is scoped to the loop body
    auto r2 = compile("repeat 1 { var t = 1 }\nblock t 0\n");
    CHECK(hasCode(r2, "undefined-variable"));

    // redefinition in the same scope is an error
    auto r3 = compile("var a = 1\nvar a = 2\n");
    CHECK(hasCode(r3, "redefined-variable"));

    // reserved names cannot be variables
    auto r4 = compile("var spike = 1\n");
    CHECK(hasCode(r4, "reserved-name"));
}

TEST(patterns_are_hoisted_and_argument_checked) {
    auto r = compile("tower(2, 3)\ndefine tower(x, h) {\n  repeat h as j { block x j }\n}\n");
    CHECK(r.ok());
    CHECK_EQ(r.ir.objects.size(), std::size_t{3});

    auto r2 = compile("define p(a) { block a 0 }\np(1, 2)\n");
    CHECK(hasCode(r2, "arg-count"));

    auto r3 = compile("define p(a) { block a 0 }\ndefine p(b) { block b 1 }\n");
    CHECK(hasCode(r3, "redefined-pattern"));

    auto r4 = compile("repeat 1 { define q() { } }\n");
    CHECK(hasCode(r4, "nested-define"));

    auto r5 = compile("define wall() { block 0 0 }\nwall\n");
    auto d = find(r5, "unknown-object");
    CHECK(d != nullptr);
    if (d) {
        bool hinted = false;
        for (auto const& n : d->notes)
            if (n.find("parentheses") != std::string::npos) hinted = true;
        CHECK(hinted);
    }
}

TEST(recursive_pattern_hits_depth_limit_not_stack_overflow) {
    auto r = compile("define f(n) { block n 0\n f(n + 1) }\nf(0)\n");
    CHECK(!r.ok());
    CHECK(hasCode(r, "pattern-depth"));
}

TEST(repeat_limits_and_validation) {
    Limits small;
    small.maxRepeatCount = 10;
    CompileOptions opts;
    opts.limits = small;

    CHECK(hasCode(compile("repeat 11 { block 0 0 }\n", opts), "repeat-limit"));
    CHECK(hasCode(compile("repeat -1 { block 0 0 }\n"), "bad-repeat-count"));
    CHECK(hasCode(compile("repeat 1.5 { block 0 0 }\n"), "bad-repeat-count"));
    CHECK(compile("repeat 0 { block 0 0 }\n").ir.objects.empty());

    Limits nest;
    nest.maxRepeatNesting = 2;
    CompileOptions nopts;
    nopts.limits = nest;
    auto r = compile("repeat 1 { repeat 1 { repeat 1 { block 0 0 } } }\n", nopts);
    CHECK(hasCode(r, "repeat-nesting"));
}

TEST(object_limit_stops_generation_cleanly) {
    Limits lim;
    lim.maxObjects = 50;
    CompileOptions opts;
    opts.limits = lim;
    auto r = compile("repeat 1000 as i { block i 0 }\n", opts);
    CHECK(!r.ok());
    CHECK(hasCode(r, "object-limit"));
    CHECK_EQ(r.ir.objects.size(), std::size_t{50});
}

TEST(expansion_limit_stops_generation_cleanly) {
    Limits lim;
    lim.maxExpandedStatements = 100;
    CompileOptions opts;
    opts.limits = lim;
    auto r = compile("repeat 1000 { var a = 1 }\n", opts);
    CHECK(hasCode(r, "expansion-limit"));
}

TEST(source_size_limits) {
    Limits lim;
    lim.maxSourceBytes = 10;
    CompileOptions opts;
    opts.limits = lim;
    CHECK(hasCode(compile("block 1 1\nblock 2 2\n", opts), "source-too-large"));

    Limits lines;
    lines.maxLines = 2;
    CompileOptions lopts;
    lopts.limits = lines;
    CHECK(hasCode(compile("\n\n\n", lopts), "too-many-lines"));
}

TEST(property_validation) {
    CHECK(hasCode(compile("block 1 1 rott:90\n"), "unknown-property"));
    CHECK(hasCode(compile("block 1 1 group:1000\n"), "bad-property-value"));
    CHECK(hasCode(compile("block 1 1 group:1.5\n"), "bad-property-value"));
    CHECK(hasCode(compile("block 1 1 flipX:1\n"), "bad-property-value"));
    CHECK(hasCode(compile("block 1 1 scale:-1\n"), "bad-property-value"));
    CHECK(hasCode(compile("block 1 1 rot:#FF0000\n"), "bad-property-value"));

    auto ok = compile("block 1 1 rot:90 scale:0.5 flipX:true flipY:false group:12 color:1004 layer:2 zOrder:-3\n");
    CHECK(ok.ok());
    auto const& o = ok.ir.objects[0];
    CHECK_EQ(o.rotation, 90.0);
    CHECK_EQ(o.scale, 0.5);
    CHECK(o.flipX);
    CHECK(!o.flipY);
    CHECK_EQ(o.groupId, 12);
    CHECK_EQ(o.colorChannel, 1004);
    CHECK_EQ(o.editorLayer, 2);
    CHECK_EQ(o.zOrder, -3);
}

TEST(level_settings_validation) {
    auto r = compile("level \"T\" {\n speed: fsat\n mode: 3\n bg: 12\n foo: 1\n mini: yes\n song: -1\n}\n");
    CHECK(!r.ok());
    auto d = find(r, "bad-setting");
    CHECK(d != nullptr);
    if (d) CHECK_CONTAINS(d->suggestion, "fast");
    CHECK(hasCode(r, "unknown-setting"));
    CHECK_EQ(r.diagnostics.errorCount(), std::size_t{6});

    auto ok = compile(
        "level \"Good\" {\n desc: \"d\"\n song: 5\n customSong: 123456\n speed: fastest\n mode: ship\n"
        " mini: true\n dual: true\n flip: true\n twoplayer: true\n platformer: true\n"
        " bg: #FF0000\n ground: #00FF00\n line: #0000FF\n object: #101010\n seed: 7\n}\n");
    CHECK(ok.ok());
    auto const& s = ok.ir.settings;
    CHECK_EQ(s.name, std::string("Good"));
    CHECK_EQ(s.description, std::string("d"));
    CHECK_EQ(s.audioTrack, 5);
    CHECK_EQ(s.customSongId, 123456);
    CHECK_EQ(s.speed, 4);
    CHECK_EQ(s.mode, 1);
    CHECK(s.mini && s.dual && s.flipGravity && s.twoPlayer && s.platformer);
    CHECK_EQ(s.colors.size(), std::size_t{4});
    CHECK_EQ(s.colors[0].channelId, 1000);
    CHECK_EQ(s.colors[0].r, 255);
    CHECK_EQ(s.colors[3].channelId, 1004);
    CHECK_EQ(s.colors[3].r, 16);
    CHECK(s.seed.has_value());
    CHECK_EQ(*s.seed, std::uint64_t{7});
}

TEST(duplicate_and_nested_level_blocks) {
    CHECK(hasCode(compile("level \"a\" { }\nlevel \"b\" { }\n"), "duplicate-level"));
    CHECK(hasCode(compile("repeat 1 { level \"a\" { } }\n"), "level-block-placement"));
    CHECK(hasCode(compile("level \"\" { }\n"), "empty-title"));
}

TEST(expression_errors) {
    CHECK(hasCode(compile("block 1 / 0 1\n"), "division-by-zero"));
    CHECK(hasCode(compile("block foo(1) 1\n"), "unknown-function"));
    CHECK(hasCode(compile("block random(1) 1\n"), "arg-count"));
    CHECK(hasCode(compile("block random(3, 1) 1\n"), "bad-random"));
    CHECK(hasCode(compile("block random(1.5, 2) 1\n"), "bad-random"));
    CHECK(hasCode(compile("block true 1\n"), "bad-expression"));
    CHECK(hasCode(compile("random(1, 2)\n"), "bad-call"));
}

TEST(coordinates_out_of_range) {
    CHECK(hasCode(compile("block 1000000 0\n"), "coord-out-of-range"));
    CHECK(compile("block -5 -5\n").ok());
}

TEST(obj_escape_hatch_validation) {
    auto ok = compile("obj 1329 5 2\n");
    CHECK(ok.ok());
    CHECK_EQ(ok.ir.objects[0].id, 1329);
    CHECK_EQ(ok.ir.objects[0].x, 5 * 30 + 15.0);
    CHECK(hasCode(compile("obj 0 1 1\n"), "bad-object-id"));
    CHECK(hasCode(compile("obj 99999 1 1\n"), "bad-object-id"));
    CHECK(hasCode(compile("obj 1.5 1 1\n"), "bad-object-id"));
    CHECK(hasCode(compile("obj 1 1\n"), "bad-object"));
}

TEST(errors_do_not_abort_the_whole_script) {
    // 1 bad statement in the middle; the good ones still generate.
    auto r = compile("block 0 0\nspirke 1 0\nblock 2 0\n");
    CHECK(!r.ok());
    CHECK_EQ(r.ir.objects.size(), std::size_t{2});
}

TEST(identical_diagnostics_inside_loops_are_reported_once) {
    auto r = compile("repeat 50 { block 0 0 flipX:maybe }\n");
    CHECK_EQ(r.diagnostics.errorCount(), std::size_t{1});
}
