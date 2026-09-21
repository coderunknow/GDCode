// Minimal single-header test framework (no external dependencies - the sandbox
// and CI both have to run these without package downloads).
//
// Each test executable is ONE .cpp that includes this header; main() is
// provided here.
#pragma once

#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace gdtest {

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

struct Failure {
    std::string file;
    int line;
    std::string message;
};

inline std::vector<Failure>& failures() {
    static std::vector<Failure> f;
    return f;
}

inline void reportFailure(char const* file, int line, std::string message) {
    failures().push_back({file, line, std::move(message)});
    std::printf("    FAIL %s:%d: %s\n", file, line, failures().back().message.c_str());
}

} // namespace gdtest

#define TEST(name)                                                     \
    static void gdtest_fn_##name();                                    \
    static ::gdtest::Registrar gdtest_reg_##name(#name, gdtest_fn_##name); \
    static void gdtest_fn_##name()

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            ::gdtest::reportFailure(__FILE__, __LINE__,                \
                                    "CHECK failed: " #cond);           \
        }                                                              \
    } while (0)

#define CHECK_MSG(cond, msg)                                           \
    do {                                                               \
        if (!(cond)) {                                                 \
            ::gdtest::reportFailure(__FILE__, __LINE__,                \
                                    std::string("CHECK failed: " #cond \
                                                " -- ") + (msg));      \
        }                                                              \
    } while (0)

#define CHECK_EQ(a, b)                                                 \
    do {                                                               \
        auto gdtest_va = (a);                                          \
        auto gdtest_vb = (b);                                          \
        if (!(gdtest_va == gdtest_vb)) {                               \
            std::ostringstream gdtest_ss;                              \
            gdtest_ss << "CHECK_EQ failed: " #a " == " #b " ("        \
                      << gdtest_va << " vs " << gdtest_vb << ")";      \
            ::gdtest::reportFailure(__FILE__, __LINE__, gdtest_ss.str()); \
        }                                                              \
    } while (0)

#define CHECK_CONTAINS(haystack, needle)                               \
    do {                                                               \
        std::string gdtest_h = (haystack);                             \
        std::string gdtest_n = (needle);                               \
        if (gdtest_h.find(gdtest_n) == std::string::npos) {            \
            ::gdtest::reportFailure(                                   \
                __FILE__, __LINE__,                                    \
                "CHECK_CONTAINS failed: expected to find '" + gdtest_n + \
                "' in:\n----\n" + gdtest_h + "\n----");                \
        }                                                              \
    } while (0)

int main() {
    int failed = 0;
    std::printf("Running %zu test case(s)\n", gdtest::registry().size());
    for (auto const& c : gdtest::registry()) {
        std::size_t before = gdtest::failures().size();
        c.fn();
        if (gdtest::failures().size() != before) {
            ++failed;
            std::printf("  [FAIL] %s\n", c.name);
        } else {
            std::printf("  [ ok ] %s\n", c.name);
        }
    }
    if (failed) {
        std::printf("RESULT: %d/%zu case(s) had failures\n", failed,
                    gdtest::registry().size());
        return 1;
    }
    std::printf("RESULT: all %zu case(s) passed\n", gdtest::registry().size());
    return 0;
}
