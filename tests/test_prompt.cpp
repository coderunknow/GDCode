#include "framework.hpp"
#include "AiCodingPrompt.hpp"

#include <fstream>
#include <iterator>
#include <string>

TEST(embedded_prompt_is_complete_and_identical_to_canonical_text) {
    std::ifstream file(GDCODE_PROMPT_FILE, std::ios::binary);
    CHECK(file.good());
    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::string embedded = gdcode::content::kAiCodingPrompt;
    CHECK_EQ(embedded, source);
    CHECK(embedded.size() > 8000);
    CHECK_CONTAINS(embedded, "You are now a GDCode coding agent.");
    CHECK_CONTAINS(embedded, "LevelNavigation");
    CHECK_CONTAINS(embedded, "ctest --test-dir");
}

TEST(prompt_is_safe_for_gd_bitmap_fonts) {
    for (unsigned char byte : std::string(gdcode::content::kAiCodingPrompt)) {
        CHECK(byte == '\n' || (byte >= 0x20 && byte < 0x7f));
    }
}
