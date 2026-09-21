#include "gdcode/Lexer.hpp"

#include <cctype>
#include <cstdlib>

namespace gdcode {

Lexer::Lexer(std::string const& source, DiagnosticBag& diags)
    : m_src(source), m_diags(diags), m_lines(source) {}

char Lexer::peek(std::size_t ahead) const {
    std::size_t i = m_offset + ahead;
    return i < m_src.size() ? m_src[i] : '\0';
}

char Lexer::advance() {
    char c = peek();
    // EOF is a position, not a byte value: embedded NUL must still advance.
    if (!atEnd()) ++m_offset;
    return c;
}

bool Lexer::atEnd() const { return m_offset >= m_src.size(); }

SourcePos Lexer::here() const { return m_lines.posOf(m_offset); }

void Lexer::errorAt(SourcePos pos, std::string code, std::string message) {
    Diagnostic d;
    d.severity = Severity::Error;
    d.span.begin = pos;
    d.span.end = pos;
    d.code = std::move(code);
    d.message = std::move(message);
    m_diags.report(std::move(d));
}

void Lexer::skipComment() {
    while (!atEnd() && peek() != '\n') advance();
}

std::vector<Token> Lexer::lex() {
    std::vector<Token> tokens;
    for (;;) {
        Token t = next();
        Tok kind = t.kind;
        tokens.push_back(std::move(t));
        if (kind == Tok::End) break;
    }
    return tokens;
}

Token Lexer::next() {
    // Skip spaces/tabs/CR (not newlines - those are tokens).
    while (!atEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
            continue;
        }
        if ((c == '/' && peek(1) == '/') || c == '#') {
            // '#' followed by exactly 6 hex digits is a color literal;
            // any other '#' starts a line comment.
            if (c == '#') {
                bool looksLikeColor = true;
                for (int i = 1; i <= 6; ++i) {
                    char h = peek(static_cast<std::size_t>(i));
                    if (!std::isxdigit(static_cast<unsigned char>(h))) {
                        looksLikeColor = false;
                        break;
                    }
                }
                if (looksLikeColor &&
                    std::isxdigit(static_cast<unsigned char>(peek(7)))) {
                    looksLikeColor = false; // >6 hex digits: not a color
                }
                if (!looksLikeColor) {
                    skipComment();
                    continue;
                }
            } else {
                skipComment();
                continue;
            }
        }
        break;
    }

    Token tok;
    tok.span.begin = here();

    if (atEnd()) {
        tok.kind = Tok::End;
        tok.span.end = tok.span.begin;
        return tok;
    }

    char c = peek();

    if (c == '\n') {
        advance();
        tok.kind = Tok::Newline;
        tok.span.end = here();
        return tok;
    }

    // Punctuation
    auto single = [&](Tok k) {
        advance();
        tok.kind = k;
        tok.span.end = here();
        return tok;
    };
    switch (c) {
        case '{': return single(Tok::LBrace);
        case '}': return single(Tok::RBrace);
        case '(': return single(Tok::LParen);
        case ')': return single(Tok::RParen);
        case ',': return single(Tok::Comma);
        case ':': return single(Tok::Colon);
        case '=': return single(Tok::Assign);
        case '+': return single(Tok::Plus);
        case '-': return single(Tok::Minus);
        case '*': return single(Tok::Star);
        case '/': return single(Tok::Slash);
        default: break;
    }

    if (c == '#') {
        // Guaranteed to be a color literal here (comment case handled above).
        advance(); // '#'
        tok.kind = Tok::Color;
        std::string hex;
        for (int i = 0; i < 6; ++i) hex += advance();
        tok.text = std::move(hex);
        tok.span.end = here();
        return tok;
    }

    if (std::isdigit(static_cast<unsigned char>(c)) ||
        (c == '.' && std::isdigit(static_cast<unsigned char>(peek(1))))) {
        return lexNumber();
    }

    if (c == '_' || std::isalpha(static_cast<unsigned char>(c))) {
        return lexIdentOrKeyword();
    }

    if (c == '"') {
        return lexString();
    }

    // Reject anything else (including non-ASCII bytes).
    SourcePos pos = here();
    std::string msg;
    if (static_cast<unsigned char>(c) >= 0x80) {
        msg = "non-ASCII character in code (GD's fonts cannot render it; use ASCII)";
    } else if (c == '\0') {
        msg = "unexpected NUL byte in code";
    } else {
        msg = "unexpected character '";
        msg += c;
        msg += "'";
    }
    errorAt(pos, "unexpected-char", msg);
    advance();
    tok.kind = Tok::Bad;
    tok.span.end = here();
    return tok;
}

Token Lexer::lexIdentOrKeyword() {
    Token tok;
    tok.kind = Tok::Ident;
    tok.span.begin = here();
    std::string text;
    while (!atEnd()) {
        char c = peek();
        if (c == '_' || std::isalnum(static_cast<unsigned char>(c))) {
            text += c;
            advance();
        } else {
            break;
        }
    }
    tok.text = std::move(text);
    tok.span.end = here();
    return tok;
}

Token Lexer::lexNumber() {
    Token tok;
    tok.kind = Tok::Number;
    tok.span.begin = here();
    std::string text;
    bool sawDot = false;
    while (!atEnd()) {
        char c = peek();
        if (std::isdigit(static_cast<unsigned char>(c))) {
            text += c;
            advance();
        } else if (c == '.' && !sawDot &&
                   std::isdigit(static_cast<unsigned char>(peek(1)))) {
            sawDot = true;
            text += c;
            advance();
        } else {
            break;
        }
    }
    tok.number = std::strtod(text.c_str(), nullptr);
    tok.text = std::move(text);
    tok.span.end = here();
    return tok;
}

Token Lexer::lexString() {
    Token tok;
    tok.kind = Tok::String;
    tok.span.begin = here();
    advance(); // opening quote
    std::string out;
    bool closed = false;
    while (!atEnd()) {
        char c = advance();
        if (c == '"') {
            closed = true;
            break;
        }
        if (c == '\n') {
            errorAt(tok.span.begin, "unterminated-string",
                    "string literal is not closed on this line");
            break;
        }
        if (c == '\\') {
            char e = peek();
            switch (e) {
                case 'n': out += '\n'; advance(); continue;
                case 't': out += '\t'; advance(); continue;
                case '\\': out += '\\'; advance(); continue;
                case '"': out += '"'; advance(); continue;
                default: {
                    SourcePos pos = here();
                    std::string msg = "unknown escape sequence '\\";
                    msg += (e ? std::string(1, e) : std::string("?")) + "'";
                    errorAt(pos, "bad-escape", msg);
                    advance();
                    continue;
                }
            }
        }
        if (static_cast<unsigned char>(c) < 0x20) {
            SourcePos pos = here();
            errorAt(pos, "control-char", "control character inside string literal");
            continue;
        }
        out += c;
    }
    if (!closed && atEnd()) {
        errorAt(tok.span.begin, "unterminated-string", "string literal is not closed");
    }
    tok.text = std::move(out);
    tok.span.end = here();
    return tok;
}

} // namespace gdcode
