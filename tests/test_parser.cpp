#include "framework.hpp"

#include "gdcode/Lexer.hpp"
#include "gdcode/Parser.hpp"

using namespace gdcode;

namespace {
Program parseSrc(std::string const& src, DiagnosticBag& diags) {
    Lexer lexer(src, diags);
    Parser parser(lexer.lex(), diags);
    return parser.parse();
}
} // namespace

TEST(parses_object_with_variant_and_props) {
    DiagnosticBag diags;
    auto prog = parseSrc("spike half 4 1 rot:90 flipX:true\n", diags);
    CHECK(!diags.hasErrors());
    CHECK_EQ(prog.statements.size(), std::size_t{1});
    auto const& s = prog.statements[0];
    CHECK(s.kind == Stmt::Kind::Object);
    CHECK_EQ(s.type, std::string("spike"));
    CHECK_EQ(s.variant, std::string("half"));
    CHECK(s.x.kind == ExprKind::Number);
    CHECK_EQ(s.x.number, 4.0);
    CHECK_EQ(s.props.size(), std::size_t{2});
    CHECK_EQ(s.props[0].key, std::string("rot"));
    CHECK_EQ(s.props[1].key, std::string("flipX"));
    CHECK(s.props[1].expr.isBareIdent());
    CHECK_EQ(s.props[1].expr.name, std::string("true"));
}

TEST(variable_after_type_is_an_expression_not_a_variant) {
    DiagnosticBag diags;
    auto prog = parseSrc("repeat 3 as i {\n  block i 0\n  spike i 1\n}\n", diags);
    CHECK(!diags.hasErrors());
    auto const& body = prog.statements[0].body;
    CHECK_EQ(body.size(), std::size_t{2});
    CHECK(body[0].variant.empty());
    CHECK(body[0].x.kind == ExprKind::Var);
    CHECK_EQ(body[0].x.name, std::string("i"));
    CHECK(body[1].variant.empty());
    CHECK_EQ(body[1].x.name, std::string("i"));
}

TEST(misspelled_variant_is_kept_as_variant_for_semantic_suggestion) {
    DiagnosticBag diags;
    auto prog = parseSrc("orb yelow 8 3\n", diags);
    CHECK(!diags.hasErrors());
    CHECK_EQ(prog.statements[0].variant, std::string("yelow"));
    CHECK_EQ(prog.statements[0].x.number, 8.0);
}

TEST(expression_precedence_and_parens) {
    DiagnosticBag diags;
    auto prog = parseSrc("var v = 1 + 2 * 3 - (4 - 1) / 3\n", diags);
    CHECK(!diags.hasErrors());
    auto const& e = prog.statements[0].init;
    // ((1 + (2*3)) - ((4-1)/3))
    CHECK(e.kind == ExprKind::Binary);
    CHECK(e.op == Tok::Minus);
    CHECK(e.args[0].kind == ExprKind::Binary);
    CHECK(e.args[0].op == Tok::Plus);
    CHECK(e.args[0].args[1].op == Tok::Star);
    CHECK(e.args[1].op == Tok::Slash);
}

TEST(unary_minus_and_calls) {
    DiagnosticBag diags;
    auto prog = parseSrc("block -x random(1, 3)\n", diags);
    CHECK(!diags.hasErrors());
    auto const& s = prog.statements[0];
    CHECK(s.x.kind == ExprKind::Unary);
    CHECK(s.y.kind == ExprKind::Call);
    CHECK_EQ(s.y.name, std::string("random"));
    CHECK_EQ(s.y.args.size(), std::size_t{2});
}

TEST(level_block_settings) {
    DiagnosticBag diags;
    auto prog = parseSrc(
        "level \"My Level\" {\n  speed: fast\n  bg: #FF0000\n  desc: \"hi\"\n  song: 3\n}\n",
        diags);
    CHECK(!diags.hasErrors());
    auto const& s = prog.statements[0];
    CHECK(s.kind == Stmt::Kind::Level);
    CHECK_EQ(s.title, std::string("My Level"));
    CHECK_EQ(s.settings.size(), std::size_t{4});
    CHECK(s.settings[1].valueKind == Prop::ValueKind::Color);
    CHECK_EQ(s.settings[1].r, 255);
    CHECK_EQ(s.settings[1].g, 0);
    CHECK(s.settings[2].valueKind == Prop::ValueKind::Str);
    CHECK_EQ(s.settings[2].str, std::string("hi"));
}

TEST(define_and_call) {
    DiagnosticBag diags;
    auto prog = parseSrc("define wall(x, h) {\n  block x h\n}\nwall(3, 2)\n", diags);
    CHECK(!diags.hasErrors());
    CHECK_EQ(prog.statements.size(), std::size_t{2});
    CHECK(prog.statements[0].kind == Stmt::Kind::Define);
    CHECK_EQ(prog.statements[0].params.size(), std::size_t{2});
    CHECK(prog.statements[1].kind == Stmt::Kind::Call);
    CHECK_EQ(prog.statements[1].callArgs.size(), std::size_t{2});
}

TEST(brace_on_same_line_and_blank_lines) {
    DiagnosticBag diags;
    auto prog = parseSrc("\n\nrepeat 2 {\n\n  block 1 1\n\n}\n\n", diags);
    CHECK(!diags.hasErrors());
    CHECK_EQ(prog.statements.size(), std::size_t{1});
    CHECK_EQ(prog.statements[0].body.size(), std::size_t{1});
}

TEST(obj_escape_hatch_takes_three_values) {
    DiagnosticBag diags;
    auto prog = parseSrc("obj 1329 5 2 rot:45\nvar id = 8\nobj id 1 0\n", diags);
    CHECK(!diags.hasErrors());
    CHECK(prog.statements[0].thirdPresent);
    CHECK_EQ(prog.statements[0].x.number, 1329.0);
    CHECK_EQ(prog.statements[0].props.size(), std::size_t{1});
    CHECK(prog.statements[2].thirdPresent);
    CHECK(prog.statements[2].x.kind == ExprKind::Var);
}

TEST(error_recovery_collects_multiple_errors) {
    DiagnosticBag diags;
    auto prog = parseSrc("block 1 (2\nspike 1 1\n\"str\" 1 2\nblock 3 3\n", diags);
    CHECK_EQ(diags.errorCount(), std::size_t{2});
    // The two valid statements survive.
    CHECK_EQ(prog.statements.size(), std::size_t{2});
    CHECK_EQ(diags.items()[0].span.begin.line, std::size_t{1});
    CHECK_EQ(diags.items()[1].span.begin.line, std::size_t{3});
}

TEST(missing_closing_brace_is_reported) {
    DiagnosticBag diags;
    auto prog = parseSrc("repeat 2 {\n  block 1 1\n", diags);
    CHECK(diags.hasErrors());
    CHECK_CONTAINS(diags.items()[0].message, "missing its closing '}'");
    (void)prog;
}

TEST(stray_closing_brace_is_reported) {
    DiagnosticBag diags;
    parseSrc("block 1 1\n}\n", diags);
    CHECK(diags.hasErrors());
    CHECK_CONTAINS(diags.items()[0].message, "unexpected '}'");
}

TEST(extra_values_after_coordinates_get_variant_hint) {
    DiagnosticBag diags;
    parseSrc("orb 8 3 4\n", diags);
    CHECK(diags.hasErrors());
    CHECK_CONTAINS(diags.items()[0].message, "extra value");
    CHECK(!diags.items()[0].notes.empty());
    CHECK_CONTAINS(diags.items()[0].notes[0], "yellow");
}

TEST(unknown_type_produces_no_parse_error) {
    // The semantic pass owns the "unknown object" diagnostic (with
    // suggestions); the parser must not add noise.
    DiagnosticBag diags;
    auto prog = parseSrc("spirke half 4 1\n", diags);
    CHECK(!diags.hasErrors());
    CHECK_EQ(prog.statements.size(), std::size_t{1});
    CHECK_EQ(prog.statements[0].type, std::string("spirke"));
}

TEST(minus_with_space_before_and_none_after_starts_a_new_argument) {
    DiagnosticBag diags;
    auto prog = parseSrc("block 5 -1\nblock 5 - 1 2\nblock 5-1 2\nblock (3 -1) 2\n", diags);
    CHECK(!diags.hasErrors());
    CHECK_EQ(prog.statements.size(), std::size_t{4});
    // block 5 -1  -> x=5, y=-1
    CHECK(prog.statements[0].x.kind == ExprKind::Number);
    CHECK(prog.statements[0].y.kind == ExprKind::Unary);
    // block 5 - 1 2 -> x=(5-1), y=2
    CHECK(prog.statements[1].x.kind == ExprKind::Binary);
    CHECK_EQ(prog.statements[1].y.number, 2.0);
    // block 5-1 2 -> x=(5-1), y=2
    CHECK(prog.statements[2].x.kind == ExprKind::Binary);
    // parentheses switch the rule off: (3 -1) is a binary expression
    CHECK(prog.statements[3].x.kind == ExprKind::Binary);
    CHECK_EQ(prog.statements[3].y.number, 2.0);
}

TEST(object_type_followed_by_paren_is_not_a_call) {
    DiagnosticBag diags;
    auto prog = parseSrc("block (1 + 2) 0\n", diags);
    CHECK(!diags.hasErrors());
    CHECK(prog.statements[0].kind == Stmt::Kind::Object);
    CHECK(prog.statements[0].x.kind == ExprKind::Binary);
}

TEST(property_key_is_never_parsed_as_an_operand) {
    DiagnosticBag diags;
    parseSrc("block 1 rot:5 2\n", diags);
    CHECK(diags.hasErrors());
    CHECK_CONTAINS(diags.items()[0].message, "property 'rot:'");
}

TEST(level_settings_may_share_a_line) {
    DiagnosticBag diags;
    auto prog = parseSrc("level \"L\" { seed: 1 speed: fast\n mode: ship }\n", diags);
    CHECK(!diags.hasErrors());
    CHECK_EQ(prog.statements[0].settings.size(), std::size_t{3});
}
