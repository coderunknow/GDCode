#include "framework.hpp"
#include "../src/ui/EditorText.hpp"

using namespace gdcode::ui;

TEST(normalize_windows_and_legacy_line_endings_without_truncating) {
    auto text = prepareEditorText("block 0 0\r\nspike 1 1\r# end\n");
    CHECK(text.has_value());
    CHECK_EQ(*text, "block 0 0\nspike 1 1\n# end\n");
    CHECK_EQ(*prepareEditorText(""), "");
    CHECK_EQ(*prepareEditorText("\tblock 0 0"), "\tblock 0 0");
}

TEST(exact_limit_is_accepted_but_oversize_is_rejected_not_cropped) {
    std::string original(kMaxEditorChars, 'x');
    auto atLimit = prepareEditorText(original);
    CHECK(atLimit.has_value());
    CHECK_EQ(*atLimit, original);
    original += "KEEP THIS TAIL";
    CHECK(!prepareEditorText(original).has_value());
    CHECK(original.ends_with("KEEP THIS TAIL"));
}

TEST(limit_applies_after_crlf_normalization) {
    std::string windows;
    for (std::size_t i = 0; i < kMaxEditorChars; ++i) windows += "\r\n";
    auto text = prepareEditorText(windows);
    CHECK(text.has_value());
    CHECK_EQ(text->size(), kMaxEditorChars);
    CHECK(!prepareEditorText(windows + "x").has_value());
    CHECK_EQ(*prepareEditorText("x\r\r\ny"), "x\n\ny");
}
