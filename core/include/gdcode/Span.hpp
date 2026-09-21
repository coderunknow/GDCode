#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace gdcode {

/// A position in the source: 1-based line and column (column counts bytes).
struct SourcePos {
    std::size_t line = 1;
    std::size_t column = 1;
};

/// A source location used for diagnostics.
struct SourceSpan {
    SourcePos begin{};
    SourcePos end{};

    /// True when this span carries real location information.
    bool valid() const { return begin.line != 0; }

    /// "line L, column C"
    std::string describe() const {
        if (!valid()) return {};
        return "line " + std::to_string(begin.line) + ", column " + std::to_string(begin.column);
    }
};

/// Map of byte offsets -> (line, column). Built once per source file.
class LineIndex {
public:
    explicit LineIndex(std::string const& source);

    /// Position of the byte at `offset` (clamped to the source length).
    SourcePos posOf(std::size_t offset) const;

    std::size_t lineCount() const { return m_lineStarts.size(); }

    /// Text of 1-based line `line` (without the trailing newline).
    std::string lineText(std::size_t line) const;

private:
    std::string const* m_source = nullptr;
    std::vector<std::size_t> m_lineStarts;
};

} // namespace gdcode
