#pragma once

#include "Span.hpp"

#include <string>
#include <string_view>

namespace gdcode {

enum class Tok {
    End,
    Newline,

    Ident,   // block, spike, myVar ...
    Number,  // 4, 4.5, .5
    String,  // "..."
    Color,   // #RRGGBB (text = 6 hex digits)

    LBrace,  // {
    RBrace,  // }
    LParen,  // (
    RParen,  // )
    Comma,   // ,
    Colon,   // :
    Assign,  // =
    Plus,    // +
    Minus,   // -
    Star,    // *
    Slash,   // /

    Bad,     // lexical error placeholder
};

struct Token {
    Tok kind = Tok::End;
    SourceSpan span{};
    std::string text;    ///< lexeme (ident text, number literal, string contents)
    double number = 0.0; ///< parsed value when kind == Number

    bool is(Tok k) const { return kind == k; }
};

std::string_view tokenName(Tok t);

} // namespace gdcode
