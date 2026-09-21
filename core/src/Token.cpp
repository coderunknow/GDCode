#include "gdcode/Token.hpp"

namespace gdcode {

std::string_view tokenName(Tok t) {
    switch (t) {
        case Tok::End: return "end of file";
        case Tok::Newline: return "newline";
        case Tok::Ident: return "identifier";
        case Tok::Number: return "number";
        case Tok::String: return "string";
        case Tok::Color: return "color";
        case Tok::LBrace: return "'{'";
        case Tok::RBrace: return "'}'";
        case Tok::LParen: return "'('";
        case Tok::RParen: return "')'";
        case Tok::Comma: return "','";
        case Tok::Colon: return "':'";
        case Tok::Assign: return "'='";
        case Tok::Plus: return "'+'";
        case Tok::Minus: return "'-'";
        case Tok::Star: return "'*'";
        case Tok::Slash: return "'/'";
        case Tok::Bad: return "invalid token";
    }
    return "?";
}

} // namespace gdcode
