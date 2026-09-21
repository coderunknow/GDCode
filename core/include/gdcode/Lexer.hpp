#pragma once

#include "Diagnostic.hpp"
#include "Token.hpp"

#include <string>
#include <vector>

namespace gdcode {

/// Turns source text into a token stream.
///
/// * `#` and `//` start line comments.
/// * Newlines are significant tokens (statement separators).
/// * Strings support \" \\ \n \t escapes; raw control characters are errors.
/// * Non-ASCII bytes are rejected with a diagnostic (GD's bitmap fonts cannot
///   render them anyway).
class Lexer {
public:
    Lexer(std::string const& source, DiagnosticBag& diags);

    /// Lex the whole input. Always succeeds; problems are reported to the
    /// diagnostic bag as Tok::Bad-free stream (bad chars are skipped).
    std::vector<Token> lex();

private:
    Token next();
    Token lexIdentOrKeyword();
    Token lexNumber();
    Token lexString();
    void skipComment();

    char peek(std::size_t ahead = 0) const;
    char advance();
    bool atEnd() const;
    SourcePos here() const;
    void errorAt(SourcePos pos, std::string code, std::string message);

    std::string const& m_src;
    DiagnosticBag& m_diags;
    LineIndex m_lines;
    std::size_t m_offset = 0;
};

} // namespace gdcode
