#pragma once

#include "Ast.hpp"
#include "Diagnostic.hpp"
#include "Ir.hpp"
#include "Limits.hpp"
#include "Rng.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace gdcode {

struct LoweringStats {
    std::size_t expandedStatements = 0;
    std::uint64_t seed = 0;
    bool seedExplicit = false;
    bool randomUsed = false;
    bool abortedByLimit = false;
};

/// Semantic analysis + IR generation in one pass (the "semantic validation"
/// stage of the pipeline). Walks the AST, expands `repeat`/`define`, checks
/// every name/value against the object catalog, enforces all Limits, and
/// emits the LevelIR.
///
/// Never throws; all problems go to the diagnostic bag. When a hard limit is
/// hit it stops expanding (abortedByLimit) but keeps the IR produced so far.
class Lowering {
public:
    Lowering(Program const& program, DiagnosticBag& diags, Limits limits,
             std::string defaultLevelName = "Untitled");

    LevelIR run();
    LoweringStats const& stats() const { return m_stats; }

private:
    struct Macro {
        std::vector<std::string> params;
        std::vector<Stmt> const* body = nullptr;
        SourceSpan span{};
    };
    struct Scope {
        std::unordered_map<std::string, double> vars;
    };

    void execStatements(std::vector<Stmt> const& stmts, int macroDepth);
    void execStatement(Stmt const& stmt, int macroDepth);
    void execObject(Stmt const& stmt);
    void execRepeat(Stmt const& stmt, int macroDepth);
    void execCall(Stmt const& stmt, int macroDepth);
    void execLevel(Stmt const& stmt);

    bool evalExpr(Expr const& expr, double& out);
    bool evalPropNumber(Prop const& prop, double& out);
    std::optional<std::string> propEnumName(Prop const& prop) const;

    void addObject(ObjectIR obj, Stmt const& from);
    bool checkBudget();
    void registerMacro(Stmt const& stmt);
    static bool isReservedName(std::string const& name);

    double* lookupVar(std::string const& name);

    Program const& m_program;
    DiagnosticBag& m_diags;
    Limits m_limits;
    LevelIR m_ir;
    LoweringStats m_stats;
    Rng m_rng{1};
    std::vector<Scope> m_scopes{1};
    std::unordered_map<std::string, Macro> m_macros;
    std::chrono::steady_clock::time_point m_deadline{};
    bool m_levelBlockSeen = false;
    bool m_budgetErrorReported = false;
    int m_repeatDepth = 0;
};

} // namespace gdcode
