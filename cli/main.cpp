// gdcode-cli: compile a GDCode script from the command line.
//
// Usage:
//   gdcode-cli <file.gdx> [--ir] [--level-string] [--quiet]
//
// Exit code 0 when the script compiles without errors, 1 otherwise.
// This is the same compiler core the Geode mod uses; it just lacks the game.

#include "gdcode/Compiler.hpp"
#include "gdcode/Encoder.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <file.gdx> [--ir] [--level-string] [--quiet]\n",
                     argv[0]);
        return 2;
    }
    bool printIr = false, printLevel = false, quiet = false;
    std::string path;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ir") printIr = true;
        else if (arg == "--level-string") printLevel = true;
        else if (arg == "--quiet") quiet = true;
        else path = arg;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        return 2;
    }
    std::stringstream buf;
    buf << in.rdbuf();

    std::string source = buf.str();
    auto result = gdcode::compile(source);
    if (!quiet) {
        gdcode::LineIndex lines(source);
        for (auto const& d : result.diagnostics.items()) {
            std::cout << d.renderWithSource(lines) << "\n";
        }
        std::cout << (result.ok() ? "OK" : "FAILED") << ": "
                  << result.stats.objectCount << " object(s), "
                  << result.diagnostics.errorCount() << " error(s)\n";
    }
    if (printIr) std::cout << gdcode::irToJson(result.ir);
    if (printLevel) std::cout << gdcode::encodeLevelString(result.ir) << "\n";
    return result.ok() ? 0 : 1;
}
