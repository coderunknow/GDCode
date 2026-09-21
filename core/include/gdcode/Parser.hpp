#pragma once

#include "Ast.hpp"
#include "Diagnostic.hpp"
#include "Token.hpp"

#include <optional>
#include <vector>

namespace gdcode {

/// Recursive-descent parser with error recovery.
///
/// On a syntax error the parser records a diagnostic and skips to the next
/// newline/`}` boundary, so one typo does not mask every later problem.
class Parser {
public:
    Parser(std::vector<Token> tokens, DiagnosticBag& diags);

    Program parse();

private:
    // statement-level
    std::optional<Stmt> parseStatement();
    bool parseLevelStmt(Stmt& out);
    bool parseVarStmt(Stmt& out);
    bool parseRepeatStmt(Stmt& out);
    bool parseDefineStmt(Stmt& out);
    bool parseObjectOrCallStmt(Stmt& out, Token first);
    bool parseObjectTail(Stmt& out);
    bool parseBlock(std::vector<Stmt>& out, char const* context);
    bool parseProp(Prop& out);
    bool parsePropValue(Prop& out);

    // expressions
    bool parseExpr(Expr& out);
    bool parseArgExpr(Expr& out); ///< positional argument (see .cpp)
    bool operatorStartsNewArgument(Token const& op) const;
    bool parseAdditive(Expr& out);
    bool parseMultiplicative(Expr& out);
    bool parseUnary(Expr& out);
    bool parsePrimary(Expr& out);

    // helpers
    Token const& peek(std::size_t ahead = 0) const;
    Token consume();
    bool match(Tok kind);
    bool expect(Tok kind, std::string const& what);
    void skipNewlines();
    void synchronizeToStatement();
    void error(Token const& tok, std::string code, std::string message);

    static constexpr int kMaxExprDepth = 64;
    static constexpr int kMaxBlockDepth = 32;

    std::vector<Token> m_tokens;
    std::size_t m_index = 0;
    DiagnosticBag& m_diags;
    int m_exprDepth = 0;
    int m_blockDepth = 0;
    bool m_argContext = false;
};

} // namespace gdcode
