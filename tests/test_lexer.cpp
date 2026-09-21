#include "framework.hpp"

#include "gdcode/Lexer.hpp"

using namespace gdcode;

namespace {
std::vector<Token> lexAll(std::string const& src, DiagnosticBag& diags) {
    Lexer lexer(src, diags);
    return lexer.lex();
}
std::vector<Tok> kinds(std::vector<Token> const& toks) {
    std::vector<Tok> out;
    for (auto const& t : toks) out.push_back(t.kind);
    return out;
}
} // namespace

TEST(lexes_basic_object_line) {
    DiagnosticBag diags;
    auto toks = lexAll("spike half 4 1 rot:90\n", diags);
    CHECK(!diags.hasErrors());
    std::vector<Tok> expected = {Tok::Ident, Tok::Ident, Tok::Number, Tok::Number,
                                 Tok::Ident, Tok::Colon, Tok::Number, Tok::Newline,
                                 Tok::End};
    CHECK(kinds(toks) == expected);
    CHECK_EQ(toks[0].text, std::string("spike"));
    CHECK_EQ(toks[2].number, 4.0);
    CHECK_EQ(toks[6].number, 90.0);
}

TEST(positions_are_one_based_line_and_column) {
    DiagnosticBag diags;
    auto toks = lexAll("block 1 2\n  spike 3 4", diags);
    CHECK_EQ(toks[0].span.begin.line, std::size_t{1});
    CHECK_EQ(toks[0].span.begin.column, std::size_t{1});
    // "spike" on line 2 after two spaces -> column 3
    CHECK_EQ(toks[4].text, std::string("spike"));
    CHECK_EQ(toks[4].span.begin.line, std::size_t{2});
    CHECK_EQ(toks[4].span.begin.column, std::size_t{3});
}

TEST(comments_hash_and_double_slash) {
    DiagnosticBag diags;
    auto toks = lexAll("# full line\nblock 1 2 // trailing\nspike 1 1 # trailing hash\n", diags);
    CHECK(!diags.hasErrors());
    std::size_t idents = 0;
    for (auto const& t : toks)
        if (t.is(Tok::Ident)) ++idents;
    CHECK_EQ(idents, std::size_t{2});
}

TEST(color_literal_vs_comment) {
    DiagnosticBag diags;
    auto toks = lexAll("bg: #FF8800 # comment\n", diags);
    CHECK(!diags.hasErrors());
    CHECK(toks[2].is(Tok::Color));
    CHECK_EQ(toks[2].text, std::string("FF8800"));
    CHECK(toks[3].is(Tok::Newline));

    // '#' followed by 7 hex digits is a comment, not a color
    DiagnosticBag d2;
    auto t2 = lexAll("x #1234567\n", d2);
    CHECK(t2[1].is(Tok::Newline));
}

TEST(numbers_integers_and_decimals) {
    DiagnosticBag diags;
    auto toks = lexAll("4 4.5 .5 0.25", diags);
    CHECK(!diags.hasErrors());
    CHECK_EQ(toks[0].number, 4.0);
    CHECK_EQ(toks[1].number, 4.5);
    CHECK_EQ(toks[2].number, 0.5);
    CHECK_EQ(toks[3].number, 0.25);
}

TEST(strings_with_escapes) {
    DiagnosticBag diags;
    auto toks = lexAll(R"("a \"quoted\" name\ttab")", diags);
    CHECK(!diags.hasErrors());
    CHECK(toks[0].is(Tok::String));
    CHECK_EQ(toks[0].text, std::string("a \"quoted\" name\ttab"));
}

TEST(unterminated_string_reports_error_and_continues) {
    DiagnosticBag diags;
    auto toks = lexAll("level \"oops\nblock 1 2\n", diags);
    CHECK(diags.hasErrors());
    CHECK_EQ(diags.items()[0].code, std::string("unterminated-string"));
    CHECK_EQ(diags.items()[0].span.begin.line, std::size_t{1});
    // lexing continued: 'block' is still tokenized
    bool sawBlock = false;
    for (auto const& t : toks)
        if (t.is(Tok::Ident) && t.text == "block") sawBlock = true;
    CHECK(sawBlock);
}

TEST(non_ascii_is_rejected_with_location) {
    DiagnosticBag diags;
    auto toks = lexAll("block 1 2\nspike \xC3\xA9 1\n", diags);
    CHECK(diags.hasErrors());
    CHECK_EQ(diags.items()[0].code, std::string("unexpected-char"));
    CHECK_EQ(diags.items()[0].span.begin.line, std::size_t{2});
    CHECK_EQ(diags.items()[0].span.begin.column, std::size_t{7});
    (void)toks;
}

TEST(operators_and_punctuation) {
    DiagnosticBag diags;
    auto toks = lexAll("(a+b)*c-d/e={},:", diags);
    CHECK(!diags.hasErrors());
    std::vector<Tok> expected = {Tok::LParen, Tok::Ident, Tok::Plus,   Tok::Ident,
                                 Tok::RParen, Tok::Star,  Tok::Ident,  Tok::Minus,
                                 Tok::Ident,  Tok::Slash, Tok::Ident,  Tok::Assign,
                                 Tok::LBrace, Tok::RBrace, Tok::Comma, Tok::Colon,
                                 Tok::End};
    CHECK(kinds(toks) == expected);
}

TEST(crlf_line_endings_are_handled) {
    DiagnosticBag diags;
    auto toks = lexAll("block 1 2\r\nspike 3 4\r\n", diags);
    CHECK(!diags.hasErrors());
    std::size_t newlines = 0;
    for (auto const& t : toks)
        if (t.is(Tok::Newline)) ++newlines;
    CHECK_EQ(newlines, std::size_t{2});
}

TEST(empty_input_yields_only_end) {
    DiagnosticBag diags;
    auto toks = lexAll("", diags);
    CHECK_EQ(toks.size(), std::size_t{1});
    CHECK(toks[0].is(Tok::End));
}

TEST(embedded_nul_is_not_eof_and_never_stalls) {
    for (std::string prefix : {"", "# comment ", "// comment ", "\"text", "\"escape\\"}) {
        std::string source = prefix + std::string(1, '\0');
        if (prefix.starts_with('"')) source += '"';
        source += "\nblock 2 3\n";
        DiagnosticBag diags;
        auto tokens = lexAll(source, diags);
        CHECK(tokens.back().is(Tok::End));
        CHECK_EQ(tokens.back().span.begin.line, std::size_t{3});
        bool foundBlock = false;
        for (auto const& token : tokens) if (token.text == "block") foundBlock = true;
        CHECK(foundBlock);
        if (!prefix.starts_with('#') && !prefix.starts_with('/')) CHECK(diags.hasErrors());
    }
}

TEST(all_byte_values_make_forward_progress) {
    std::string source;
    for (int byte = 0; byte < 256; ++byte) source += static_cast<char>(byte);
    DiagnosticBag diags;
    auto tokens = lexAll(source, diags);
    CHECK(tokens.back().is(Tok::End));
    CHECK(diags.hasErrors());
}
