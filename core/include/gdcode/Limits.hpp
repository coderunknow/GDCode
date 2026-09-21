#pragma once

#include <cstddef>
#include <cstdint>

namespace gdcode {

/// Safety limits. Every potentially unbounded operation in the compiler is
/// bounded by one of these so malformed or hostile scripts can never hang or
/// OOM the game. All values are tunable for tests.
struct Limits {
    std::size_t maxSourceBytes = 1'000'000;    ///< 1 MB of code
    std::size_t maxLines = 20'000;
    std::size_t maxObjects = 100'000;          ///< generated GD objects
    std::size_t maxExpandedStatements = 200'000; ///< total executed statements
    int maxRepeatNesting = 8;
    int maxMacroDepth = 16;                    ///< define/call expansion depth
    std::size_t maxMacroArgs = 16;
    std::size_t maxRepeatCount = 100'000;      ///< per single `repeat`
    long compileTimeBudgetMs = 2'000;          ///< wall-clock budget
    double maxCoordBlocks = 100'000.0;         ///< |grid coord| cap
    std::int64_t randomMaxSpan = 1'000'000;    ///< random(a,b): b - a cap
};

} // namespace gdcode
