#include "gdcode/Parser.hpp"

#include "gdcode/Catalog.hpp"

namespace gdcode {

namespace {
bool startsExpr(Token const& t) {
    return t.is(Tok::Number) || t.is(Tok::Ident) || t.is(Tok::LParen) ||
           t.is(Tok::Minus) || t.is(Tok::Plus);
}
} // namespace

Parser::Parser(std::vector<Token> tokens, DiagnosticBag& diags)
    : m_tokens(std::move(tokens)), m_diags(diags) {}

Token const& Parser::peek(std::size_t ahead) const {
    std::size_t i = m_index + ahead;
    if (i >= m_tokens.size()) i = m_tokens.size() - 1;
    return m_tokens[i];
}

Token Parser::consume() {
    Token t = peek();
    if (!t.is(Tok::End)) ++m_index;
    return t;
}

bool Parser::match(Tok kind) {
    if (peek().is(kind)) {
        consume();
        return true;
    }
    return false;
}

bool Parser::expect(Tok kind, std::string const& what) {
    if (peek().is(kind)) {
        consume();
        return true;
    }
    error(peek(), "syntax", "expected " + what + ", found " +
                                std::string(tokenName(peek().kind)) +
                                (peek().text.empty() ? "" : " '" + peek().text + "'"));
    return false;
}

void Parser::skipNewlines() {
    while (peek().is(Tok::Newline)) consume();
}

void Parser::error(Token const& tok, std::string code, std::string message) {
    Diagnostic d;
    d.severity = Severity::Error;
    d.span = tok.span;
    d.code = std::move(code);
    d.message = std::move(message);
    m_diags.report(std::move(d));
}

void Parser::synchronizeToStatement() {
    for (;;) {
        Tok k = peek().kind;
        if (k == Tok::End || k == Tok::Newline) return;
        if (k == Tok::RBrace) return; // let the block loop handle it
        consume();
    }
}

Program Parser::parse() {
    Program program;
    skipNewlines();
    while (!peek().is(Tok::End)) {
        if (auto stmt = parseStatement()) {
            program.statements.push_back(std::move(*stmt));
        } else {
            synchronizeToStatement();
        }
        // Statements are newline-separated; tolerate missing separator before
        // '}' (end of block) and report junk otherwise.
        if (peek().is(Tok::Newline)) {
            skipNewlines();
        } else if (!peek().is(Tok::End) && !peek().is(Tok::RBrace)) {
            error(peek(), "syntax",
                  "expected newline after statement, found " +
                      std::string(tokenName(peek().kind)));
            synchronizeToStatement();
            skipNewlines();
        }
    }
    return program;
}

std::optional<Stmt> Parser::parseStatement() {
    Token first = peek();

    if (first.is(Tok::RBrace)) {
        error(first, "syntax", "unexpected '}' outside of a block");
        consume();
        return std::nullopt;
    }
    if (!first.is(Tok::Ident)) {
        error(first, "syntax",
              "expected a statement (object placement, 'var', 'repeat', "
              "'define' or 'level'), found " +
                  std::string(tokenName(first.kind)));
        return std::nullopt;
    }

    Stmt stmt;
    bool ok = false;
    if (first.text == "level") {
        consume();
        ok = parseLevelStmt(stmt);
    } else if (first.text == "var") {
        consume();
        ok = parseVarStmt(stmt);
    } else if (first.text == "repeat") {
        consume();
        ok = parseRepeatStmt(stmt);
    } else if (first.text == "define") {
        consume();
        ok = parseDefineStmt(stmt);
    } else {
        consume();
        ok = parseObjectOrCallStmt(stmt, first);
    }
    if (!ok) return std::nullopt;
    return stmt;
}

bool Parser::parseLevelStmt(Stmt& out) {
    out.kind = Stmt::Kind::Level;
    if (!expect(Tok::String, "level title string (e.g. level \"My Level\")")) {
        synchronizeToStatement();
        return false;
    }
    out.title = m_tokens[m_index - 1].text;
    out.span = m_tokens[m_index - 1].span;
    if (!expect(Tok::LBrace, "'{' after level title")) {
        synchronizeToStatement();
        return false;
    }
    for (;;) {
        skipNewlines();
        if (peek().is(Tok::RBrace)) {
            consume();
            break;
        }
        if (peek().is(Tok::End)) {
            error(peek(), "syntax", "level block is missing its closing '}'");
            return false;
        }
        Prop prop;
        if (!parseProp(prop)) {
            synchronizeToStatement();
            if (peek().is(Tok::Newline)) consume();
            continue;
        }
        out.settings.push_back(std::move(prop));
        // Settings are separated by newlines or simply by whitespace
        // (`{ seed: 1 speed: fast }`); anything else is an error.
        bool nextIsSetting = peek().is(Tok::Ident) && peek(1).is(Tok::Colon);
        if (!peek().is(Tok::Newline) && !peek().is(Tok::RBrace) && !nextIsSetting) {
            error(peek(), "syntax",
                  "expected a newline or another 'name: value' setting, found " +
                      std::string(tokenName(peek().kind)));
            synchronizeToStatement();
        }
    }
    return true;
}

bool Parser::parseVarStmt(Stmt& out) {
    out.kind = Stmt::Kind::Var;
    if (!expect(Tok::Ident, "variable name after 'var'")) {
        synchronizeToStatement();
        return false;
    }
    Token name = m_tokens[m_index - 1];
    out.name = name.text;
    out.nameSpan = name.span;
    if (!expect(Tok::Assign, "'=' after variable name")) {
        synchronizeToStatement();
        return false;
    }
    if (!parseExpr(out.init)) {
        synchronizeToStatement();
        return false;
    }
    out.span = name.span;
    return true;
}

bool Parser::parseRepeatStmt(Stmt& out) {
    out.kind = Stmt::Kind::Repeat;
    Token kw = m_tokens[m_index - 1]; // 'repeat' already consumed
    out.span = kw.span;
    if (!parseExpr(out.count)) {
        synchronizeToStatement();
        return false;
    }
    if (peek().is(Tok::Ident) && peek().text == "as") {
        consume();
        if (!expect(Tok::Ident, "loop variable name after 'as'")) {
            synchronizeToStatement();
            return false;
        }
        Token lv = m_tokens[m_index - 1];
        out.loopVar = lv.text;
        out.loopVarSpan = lv.span;
    }
    if (!parseBlock(out.body, "repeat")) return false;
    return true;
}

bool Parser::parseDefineStmt(Stmt& out) {
    out.kind = Stmt::Kind::Define;
    Token kw = m_tokens[m_index - 1];
    out.span = kw.span;
    if (!expect(Tok::Ident, "pattern name after 'define'")) {
        synchronizeToStatement();
        return false;
    }
    out.name = m_tokens[m_index - 1].text;
    out.nameSpan = m_tokens[m_index - 1].span;
    if (!expect(Tok::LParen, "'(' after pattern name")) {
        synchronizeToStatement();
        return false;
    }
    if (!peek().is(Tok::RParen)) {
        for (;;) {
            if (!expect(Tok::Ident, "parameter name")) {
                synchronizeToStatement();
                return false;
            }
            out.params.push_back(m_tokens[m_index - 1].text);
            if (peek().is(Tok::Comma)) {
                consume();
                continue;
            }
            break;
        }
    }
    if (!expect(Tok::RParen, "')' after parameter list")) {
        synchronizeToStatement();
        return false;
    }
    if (!parseBlock(out.body, "define")) return false;
    return true;
}

bool Parser::parseBlock(std::vector<Stmt>& out, char const* context) {
    if (!expect(Tok::LBrace, std::string("'{' to open the ") + context + " block")) {
        synchronizeToStatement();
        return false;
    }
    if (m_blockDepth >= kMaxBlockDepth) {
        error(m_tokens[m_index - 1], "nesting-too-deep",
              "blocks nested deeper than " + std::to_string(kMaxBlockDepth) +
                  " levels");
        // Skip the rest of the input belonging to this block.
        int depth = 1;
        while (!peek().is(Tok::End) && depth > 0) {
            if (peek().is(Tok::LBrace)) ++depth;
            if (peek().is(Tok::RBrace)) --depth;
            consume();
        }
        return false;
    }
    struct DepthGuard {
        int& d;
        explicit DepthGuard(int& depth) : d(depth) { ++d; }
        ~DepthGuard() { --d; }
    } guard(m_blockDepth);
    for (;;) {
        skipNewlines();
        if (peek().is(Tok::RBrace)) {
            consume();
            break;
        }
        if (peek().is(Tok::End)) {
            error(peek(), "syntax",
                  std::string(context) + " block is missing its closing '}'");
            return false;
        }
        if (auto parsed = parseStatement()) {
            out.push_back(std::move(*parsed));
        } else {
            synchronizeToStatement();
        }
        if (peek().is(Tok::Newline)) {
            consume();
        } else if (!peek().is(Tok::RBrace) && !peek().is(Tok::End)) {
            error(peek(), "syntax", "expected newline after statement");
            synchronizeToStatement();
        }
    }
    return true;
}

bool Parser::parseObjectOrCallStmt(Stmt& out, Token first) {
    out.span = first.span;

    // Macro call:  name(args...)   (object types are never pattern names,
    // so `block (x - 1) 2` is an object statement with a parenthesised x)
    bool isObjectType = findObjectType(first.text) != nullptr || first.text == "obj";
    if (peek().is(Tok::LParen) && !isObjectType) {
        out.kind = Stmt::Kind::Call;
        out.name = first.text;
        out.nameSpan = first.span;
        consume(); // '('
        if (!peek().is(Tok::RParen)) {
            for (;;) {
                Expr arg;
                if (!parseExpr(arg)) {
                    synchronizeToStatement();
                    return false;
                }
                out.callArgs.push_back(std::move(arg));
                if (peek().is(Tok::Comma)) {
                    consume();
                    continue;
                }
                break;
            }
        }
        if (!expect(Tok::RParen, "')' after arguments")) {
            synchronizeToStatement();
            return false;
        }
        return true;
    }

    // Object placement:  type [variant] <x> <y> [prop:value ...]
    out.kind = Stmt::Kind::Object;
    out.type = first.text;
    out.typeSpan = first.span;

    ObjectSpec const* spec = findObjectType(out.type);
    if (!spec && out.type != "obj") {
        // Unknown object type. Do not try to make sense of the rest of the
        // line: skip it so the user gets exactly one diagnostic (with
        // suggestions) from the semantic pass instead of a cascade.
        synchronizeToStatement();
        return true;
    }

    // A following identifier is either a variant name or the start of an
    // expression (a variable such as `i`). Known variants are unambiguous.
    // For an unknown identifier on a type that has variants, first try to
    // read it as a (misspelled) variant so `orb yelow 8 3` reports "unknown
    // variant" with a suggestion; if the rest of the line does not fit that
    // shape, re-parse it as an expression.
    if (spec && peek().is(Tok::Ident) && !peek(1).is(Tok::Colon)) {
        bool knownVariant = resolveObjectId(out.type, peek().text).has_value();
        if (knownVariant) {
            Token v = consume();
            out.variant = v.text;
            out.variantSpan = v.span;
        } else if (!spec->variants.empty()) {
            std::size_t savedIndex = m_index;
            std::size_t savedDiags = m_diags.size();
            Stmt trial = out;
            Token v = consume();
            trial.variant = v.text;
            trial.variantSpan = v.span;
            bool atStatementEnd = false;
            if (parseObjectTail(trial) && m_diags.size() == savedDiags) {
                atStatementEnd = peek().is(Tok::Newline) || peek().is(Tok::End) ||
                                 peek().is(Tok::RBrace);
            }
            if (atStatementEnd) {
                out = std::move(trial);
                return true;
            }
            m_diags.truncate(savedDiags);
            m_index = savedIndex;
        }
    }

    return parseObjectTail(out);
}

bool Parser::parseObjectTail(Stmt& out) {
    if (!parseArgExpr(out.x)) {
        synchronizeToStatement();
        return false;
    }
    if (!parseArgExpr(out.y)) {
        synchronizeToStatement();
        return false;
    }

    // Escape hatch: `obj <id> <x> <y>` takes a leading raw GD object id.
    if (out.type == "obj" && startsExpr(peek())) {
        if (!parseArgExpr(out.init)) {
            synchronizeToStatement();
            return false;
        }
        out.thirdPresent = true;
    }

    while (peek().is(Tok::Ident) && peek(1).is(Tok::Colon)) {
        Prop prop;
        if (!parseProp(prop)) {
            synchronizeToStatement();
            return false;
        }
        out.props.push_back(std::move(prop));
    }

    // Stray expressions on the line: common when a required variant was
    // omitted (e.g. `orb 8 3`), so add a hint.
    if (startsExpr(peek())) {
        ObjectSpec const* spec = findObjectType(out.type);
        Diagnostic d;
        d.severity = Severity::Error;
        d.span = peek().span;
        d.code = "syntax";
        d.message = "unexpected extra value after coordinates";
        if (spec && !spec->variants.empty()) {
            std::string hint = "note: '" + out.type + "' takes a variant: '" +
                               out.type + " <variant> <x> <y>' (variants: ";
            for (std::size_t i = 0; i < spec->variants.size(); ++i) {
                if (i) hint += ", ";
                hint += std::string(spec->variants[i].name);
            }
            hint += ")";
            d.notes.push_back(hint);
        }
        m_diags.report(std::move(d));
        synchronizeToStatement();
        return false;
    }
    return true;
}

bool Parser::parseProp(Prop& out) {
    if (!expect(Tok::Ident, "property name")) return false;
    Token key = m_tokens[m_index - 1];
    out.key = key.text;
    out.keySpan = key.span;
    if (!expect(Tok::Colon, "':' after property name '" + key.text + "'")) {
        return false;
    }
    return parsePropValue(out);
}

bool Parser::parsePropValue(Prop& out) {
    if (peek().is(Tok::Color)) {
        Token c = consume();
        out.valueKind = Prop::ValueKind::Color;
        out.valueSpan = c.span;
        auto hexVal = [](char h) -> int {
            if (h >= '0' && h <= '9') return h - '0';
            if (h >= 'a' && h <= 'f') return h - 'a' + 10;
            if (h >= 'A' && h <= 'F') return h - 'A' + 10;
            return 0;
        };
        out.r = hexVal(c.text[0]) * 16 + hexVal(c.text[1]);
        out.g = hexVal(c.text[2]) * 16 + hexVal(c.text[3]);
        out.b = hexVal(c.text[4]) * 16 + hexVal(c.text[5]);
        return true;
    }
    if (peek().is(Tok::String)) {
        Token s = consume();
        out.valueKind = Prop::ValueKind::Str;
        out.str = s.text;
        out.valueSpan = s.span;
        return true;
    }
    out.valueKind = Prop::ValueKind::Expr;
    if (!parseExpr(out.expr)) return false;
    out.valueSpan = out.expr.span;
    return true;
}

// --- expressions -----------------------------------------------------------

bool Parser::parseExpr(Expr& out) {
    if (m_exprDepth >= kMaxExprDepth) {
        error(peek(), "nesting-too-deep",
              "expression nesting deeper than " + std::to_string(kMaxExprDepth) +
                  " levels");
        return false;
    }
    ++m_exprDepth;
    bool ok = parseAdditive(out);
    --m_exprDepth;
    return ok;
}

bool Parser::parseArgExpr(Expr& out) {
    bool saved = m_argContext;
    m_argContext = true;
    bool ok = parseExpr(out);
    m_argContext = saved;
    return ok;
}

bool Parser::operatorStartsNewArgument(Token const& op) const {
    // In positional-argument context (`block x -1`) a '+'/'-' that has
    // whitespace before it but none after it begins the next argument
    // rather than continuing the current expression:
    //   block 5 -1     -> two arguments (5, -1)
    //   block 5 - 1    -> one argument (4)
    //   block 5-1      -> one argument (4)
    if (!m_argContext || m_index == 0) return false;
    Token const& prev = m_tokens[m_index - 1];
    Token const& next = peek(1);
    bool spaceBefore = !(prev.span.end.line == op.span.begin.line &&
                         prev.span.end.column == op.span.begin.column);
    bool adjacentAfter = next.span.begin.line == op.span.end.line &&
                         next.span.begin.column == op.span.end.column;
    return spaceBefore && adjacentAfter;
}

bool Parser::parseAdditive(Expr& out) {
    if (!parseMultiplicative(out)) return false;
    while (peek().is(Tok::Plus) || peek().is(Tok::Minus)) {
        if (operatorStartsNewArgument(peek())) break;
        Token op = consume();
        Expr rhs;
        if (!parseMultiplicative(rhs)) return false;
        Expr bin;
        bin.kind = ExprKind::Binary;
        bin.op = op.kind;
        bin.span.begin = out.span.begin;
        bin.span.end = rhs.span.end;
        bin.args.push_back(std::move(out));
        bin.args.push_back(std::move(rhs));
        out = std::move(bin);
    }
    return true;
}

bool Parser::parseMultiplicative(Expr& out) {
    if (!parseUnary(out)) return false;
    while (peek().is(Tok::Star) || peek().is(Tok::Slash)) {
        Token op = consume();
        Expr rhs;
        if (!parseUnary(rhs)) return false;
        Expr bin;
        bin.kind = ExprKind::Binary;
        bin.op = op.kind;
        bin.span.begin = out.span.begin;
        bin.span.end = rhs.span.end;
        bin.args.push_back(std::move(out));
        bin.args.push_back(std::move(rhs));
        out = std::move(bin);
    }
    return true;
}

bool Parser::parseUnary(Expr& out) {
    if (peek().is(Tok::Minus) || peek().is(Tok::Plus)) {
        Token op = consume();
        Expr operand;
        if (!parseUnary(operand)) return false;
        if (op.is(Tok::Plus)) {
            out = std::move(operand);
        } else {
            out.kind = ExprKind::Unary;
            out.op = Tok::Minus;
            out.span.begin = op.span.begin;
            out.span.end = operand.span.end;
            out.args.push_back(std::move(operand));
        }
        return true;
    }
    return parsePrimary(out);
}

bool Parser::parsePrimary(Expr& out) {
    Token t = peek();
    if (t.is(Tok::Number)) {
        consume();
        out.kind = ExprKind::Number;
        out.number = t.number;
        out.span = t.span;
        return true;
    }
    if (t.is(Tok::Ident)) {
        if (m_argContext && peek(1).is(Tok::Colon)) {
            // `name:` is a property key, never an expression operand.
            error(t, "syntax", "expected an expression, found property '" +
                                   t.text + ":'");
            return false;
        }
        consume();
        if (peek().is(Tok::LParen)) {
            consume();
            out.kind = ExprKind::Call;
            out.name = t.text;
            out.span.begin = t.span.begin;
            bool saved = m_argContext;
            m_argContext = false; // comma-separated: no splitting rule needed
            if (!peek().is(Tok::RParen)) {
                for (;;) {
                    Expr arg;
                    if (!parseExpr(arg)) {
                        m_argContext = saved;
                        return false;
                    }
                    out.args.push_back(std::move(arg));
                    if (peek().is(Tok::Comma)) {
                        consume();
                        continue;
                    }
                    break;
                }
            }
            m_argContext = saved;
            if (!expect(Tok::RParen, "')' after arguments")) return false;
            out.span.end = m_tokens[m_index - 1].span.end;
            return true;
        }
        out.kind = ExprKind::Var;
        out.name = t.text;
        out.span = t.span;
        return true;
    }
    if (t.is(Tok::LParen)) {
        consume();
        // Inside parentheses the argument-splitting rule is off: the
        // parentheses delimit the expression explicitly.
        bool saved = m_argContext;
        m_argContext = false;
        bool ok = parseExpr(out);
        m_argContext = saved;
        if (!ok) return false;
        if (!expect(Tok::RParen, "')'")) return false;
        return true;
    }
    error(t, "syntax", "expected an expression, found " +
                           std::string(tokenName(t.kind)));
    return false;
}

} // namespace gdcode
