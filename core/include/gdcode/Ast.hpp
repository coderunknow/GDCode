#pragma once

#include "Span.hpp"
#include "Token.hpp"

#include <string>
#include <vector>

namespace gdcode {

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

enum class ExprKind {
    Number,  // 4, 2.5
    Var,     // myVar            (also carries bare enum names; resolved later)
    Binary,  // a + b, a - b, a * b, a / b
    Unary,   // -a
    Call,    // random(a, b)
};

struct Expr {
    ExprKind kind = ExprKind::Number;
    SourceSpan span{};

    double number = 0.0;   ///< Number
    std::string name;      ///< Var / Call
    Tok op = Tok::End;     ///< Binary / Unary operator token

    /// Binary: {lhs, rhs}; Unary: {operand}; Call: arguments.
    std::vector<Expr> args;

    /// True when this expression is a single bare identifier (used by the
    /// semantic pass to interpret enum-valued settings like `speed: normal`).
    bool isBareIdent() const { return kind == ExprKind::Var; }
};

// ---------------------------------------------------------------------------
// Property / setting values
// ---------------------------------------------------------------------------

/// A `key: value` pair, used both for object properties (`spike 4 1 rot:90`)
/// and level settings (`level "T" { speed: fast }`).
struct Prop {
    enum class ValueKind {
        Expr,   ///< numeric expression, bare ident (enum/bool), or var
        Color,  ///< #RRGGBB
        Str,    ///< "..."
    };

    std::string key;
    SourceSpan keySpan{};

    ValueKind valueKind = ValueKind::Expr;
    Expr expr;          ///< when valueKind == Expr
    int r = 0, g = 0, b = 0; ///< when valueKind == Color
    std::string str;    ///< when valueKind == Str
    SourceSpan valueSpan{};
};

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

struct Stmt {
    enum class Kind {
        Object,  // spike half 4 1 rot:90
        Var,     // var h = 3
        Repeat,  // repeat 5 as i { ... }
        Define,  // define wall(x, y) { ... }
        Call,    // wall(10, 0)
        Level,   // level "Title" { ... }
    };

    Kind kind = Kind::Object;
    SourceSpan span{};

    // Object
    std::string type;
    SourceSpan typeSpan{};
    std::string variant; ///< empty when absent
    SourceSpan variantSpan{};
    Expr x, y;
    std::vector<Prop> props;
    /// Only for the `obj <id> <x> <y>` escape hatch: the y coordinate is
    /// stored in `init` and this flag is set (x holds the raw GD id, y holds
    /// the x coordinate). Kept this way to avoid a dedicated AST node kind.
    bool thirdPresent = false;

    // Var
    std::string name;
    SourceSpan nameSpan{};
    Expr init;

    // Repeat
    Expr count;
    std::string loopVar; ///< empty when `as` clause absent
    SourceSpan loopVarSpan{};

    // Define / Repeat body
    std::vector<std::string> params;
    std::vector<Stmt> body;

    // Call
    std::vector<Expr> callArgs;

    // Level
    std::string title;
    std::vector<Prop> settings;
};

struct Program {
    std::vector<Stmt> statements;
};

} // namespace gdcode
