#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

namespace gdcode::ui {

inline constexpr std::size_t kMaxEditorChars = 200'000;

/// Normalize before checking the limit. Reject, never truncate: callers must
/// leave the existing buffer/file untouched when no value is returned.
inline std::optional<std::string> prepareEditorText(std::string_view text) {
    std::string normalised;
    normalised.reserve(std::min(text.size(), kMaxEditorChars));
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') continue;
        if (normalised.size() == kMaxEditorChars) return std::nullopt;
        normalised += text[i] == '\r' ? '\n' : text[i];
    }
    return normalised;
}

} // namespace gdcode::ui
