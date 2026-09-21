// Golden (snapshot) tests.
//
// Every tests/golden/<name>.gdx is compiled and its IR JSON + raw level string
// are compared byte-for-byte with tests/golden/<name>.ir.json and
// tests/golden/<name>.level.txt. This pins the complete observable output of
// the compiler: any change to lowering, the catalog or the encoder shows up
// as a diff that must be reviewed.
//
// To (re)generate the expected files after an intentional change:
//     GDCODE_UPDATE_GOLDEN=1 ./test_golden
#include "framework.hpp"

#include "gdcode/Compiler.hpp"
#include "gdcode/Encoder.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace gdcode;
namespace fs = std::filesystem;

namespace {

std::string readAll(fs::path const& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void writeAll(fs::path const& p, std::string const& s) {
    std::ofstream out(p, std::ios::binary);
    out << s;
}

bool updateMode() {
    char const* v = std::getenv("GDCODE_UPDATE_GOLDEN");
    return v && *v && std::string(v) != "0";
}

std::string firstDiff(std::string const& a, std::string const& b) {
    std::size_t i = 0;
    while (i < a.size() && i < b.size() && a[i] == b[i]) ++i;
    std::size_t line = 1;
    for (std::size_t k = 0; k < i && k < a.size(); ++k)
        if (a[k] == '\n') ++line;
    return "first difference at byte " + std::to_string(i) + " (line " +
           std::to_string(line) + "): expected '" +
           b.substr(i, 40) + "' got '" + a.substr(i, 40) + "'";
}

} // namespace

TEST(golden_snapshots) {
    fs::path dir = fs::path(GDCODE_TEST_DIR) / "golden";
    std::size_t count = 0;
    for (auto const& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".gdx") continue;
        ++count;
        std::string name = entry.path().stem().string();
        auto result = compile(readAll(entry.path()));

        std::string diagText;
        for (auto const& d : result.diagnostics.items()) diagText += d.render() + "\n";
        std::string ir = irToJson(result.ir);
        std::string level = encodeLevelString(result.ir) + "\n";

        fs::path irPath = dir / (name + ".ir.json");
        fs::path levelPath = dir / (name + ".level.txt");
        fs::path diagPath = dir / (name + ".diagnostics.txt");

        if (updateMode()) {
            writeAll(irPath, ir);
            writeAll(levelPath, level);
            writeAll(diagPath, diagText);
            std::printf("    updated golden files for %s\n", name.c_str());
            continue;
        }

        CHECK_MSG(fs::exists(irPath), "missing golden file " + irPath.string() +
                                          " (run with GDCODE_UPDATE_GOLDEN=1)");
        if (!fs::exists(irPath)) continue;
        std::string expectedIr = readAll(irPath);
        std::string expectedLevel = readAll(levelPath);
        std::string expectedDiag = readAll(diagPath);
        CHECK_MSG(ir == expectedIr, name + ".ir.json: " + firstDiff(ir, expectedIr));
        CHECK_MSG(level == expectedLevel,
                  name + ".level.txt: " + firstDiff(level, expectedLevel));
        CHECK_MSG(diagText == expectedDiag,
                  name + ".diagnostics.txt: " + firstDiff(diagText, expectedDiag));
    }
    CHECK_MSG(count >= 3, "expected at least 3 golden scripts");
}
