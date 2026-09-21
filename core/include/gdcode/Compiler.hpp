#pragma once

#include "Diagnostic.hpp"
#include "Ir.hpp"
#include "Limits.hpp"

#include <string>

namespace gdcode {

struct CompileOptions {
    Limits limits;
};

struct CompileStats {
    std::size_t objectCount = 0;
    std::size_t expandedStatements = 0;
    std::uint64_t seed = 0;
    bool seedExplicit = false;
    bool randomUsed = false;
};

/// Result of compiling one GDCode source.
struct CompileResult {
    LevelIR ir;
    DiagnosticBag diagnostics;
    CompileStats stats;

    /// True when the IR is safe to hand to the GD backend.
    bool ok() const { return !diagnostics.hasErrors(); }
};

/// Compile GDCode source to a Geometry Dash level IR.
///
/// Pure function of (source, options): same input => same IR, guaranteed.
/// Never throws, never hangs: every unbounded construct is limited.
CompileResult compile(std::string const& source, CompileOptions const& options = {});

} // namespace gdcode
