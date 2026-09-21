#include "framework.hpp"

#include "gdcode/Compiler.hpp"
#include "gdcode/Encoder.hpp"

#include <chrono>
#include <cstdint>
#include <string>

using namespace gdcode;

TEST(render_format_is_human_readable) {
    auto r = compile("spirke 4 1\n");
    CHECK(!r.ok());
    std::string text = r.diagnostics.items()[0].render();
    CHECK_CONTAINS(text, "error[unknown-object]: line 1, column 1: unknown object type 'spirke'");
    CHECK_CONTAINS(text, "did you mean: 'spike'");
}

TEST(diagnostics_are_sorted_by_position) {
    auto r = compile("block 1 1 rott:1\nlevel \"a\" { speed: x }\nspirke 1 1\n");
    auto const& items = r.diagnostics.items();
    CHECK(items.size() >= 3);
    for (std::size_t i = 1; i < items.size(); ++i) {
        CHECK(items[i - 1].span.begin.line <= items[i].span.begin.line);
    }
}

TEST(many_errors_are_collected_not_just_the_first) {
    std::string src;
    for (int i = 0; i < 30; ++i) src += "spirke " + std::to_string(i) + " 1\n";
    auto r = compile(src);
    CHECK_EQ(r.diagnostics.errorCount(), std::size_t{30});
}

TEST(diagnostic_flood_is_capped) {
    std::string src;
    for (int i = 0; i < 1000; ++i) src += "spirke " + std::to_string(i) + " 1\n";
    auto r = compile(src);
    CHECK(r.diagnostics.size() <= DiagnosticBag::kMaxDiagnostics + 1);
    bool overflow = false;
    for (auto const& d : r.diagnostics.items())
        if (d.code == "too-many-diagnostics") overflow = true;
    CHECK(overflow);
}

TEST(garbage_input_never_crashes) {
    // A crude deterministic fuzz: random bytes from the DSL's alphabet plus
    // some noise. Every input must compile (with errors) without crashing or
    // hanging, and never produce a non-ASCII level string.
    char const alphabet[] =
        "abcdefghijklmnopqrstuvwxyz0123456789 \n\t(){},:=+-*/#\"\\._;|!@%^&[]<>?~`'\xff\x80";
    std::uint64_t state = 0x12345678ULL;
    auto next = [&]() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    };
    auto start = std::chrono::steady_clock::now();
    for (int iter = 0; iter < 300; ++iter) {
        std::string src;
        std::size_t len = next() % 400;
        for (std::size_t i = 0; i < len; ++i) {
            src += alphabet[next() % (sizeof(alphabet) - 1)];
        }
        auto r = compile(src);
        auto ls = encodeLevelString(r.ir);
        for (unsigned char c : ls) CHECK(c >= 0x20 && c < 0x7F);
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 20);
}

TEST(pathological_nesting_is_rejected_gracefully) {
    // deeply nested parentheses in an expression
    std::string deep = "block ";
    for (int i = 0; i < 5000; ++i) deep += "(";
    deep += "1";
    for (int i = 0; i < 5000; ++i) deep += ")";
    deep += " 0\n";
    auto r = compile(deep);
    // Either it parses (fine) or errors (fine) - but it must not crash.
    (void)r;

    // deeply nested blocks
    std::string blocks;
    for (int i = 0; i < 200; ++i) blocks += "repeat 1 {\n";
    blocks += "block 0 0\n";
    for (int i = 0; i < 200; ++i) blocks += "}\n";
    auto r2 = compile(blocks);
    CHECK(!r2.ok());
}

TEST(time_budget_is_enforced) {
    Limits lim;
    lim.compileTimeBudgetMs = 50;
    lim.maxExpandedStatements = 1'000'000'000;
    lim.maxObjects = 1'000'000'000;
    lim.maxRepeatCount = 1'000'000;
    CompileOptions opts;
    opts.limits = lim;
    auto start = std::chrono::steady_clock::now();
    auto r = compile("repeat 1000000 { repeat 1000000 { var a = 1 } }\n", opts);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start)
                  .count();
    CHECK(!r.ok());
    bool timeLimit = false;
    for (auto const& d : r.diagnostics.items())
        if (d.code == "time-limit") timeLimit = true;
    CHECK(timeLimit);
    CHECK_MSG(ms < 2000, "took " + std::to_string(ms) + "ms");
}

TEST(empty_and_comment_only_sources_compile_to_empty_level) {
    auto a = compile("");
    CHECK(a.ok());
    CHECK(a.ir.objects.empty());
    auto b = compile("# nothing\n\n// here\n");
    CHECK(b.ok());
    CHECK(b.ir.empty());
    CHECK_EQ(b.ir.settings.name, std::string("Untitled"));
}
