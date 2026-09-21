#include "gdcode/Span.hpp"

#include <algorithm>

namespace gdcode {

LineIndex::LineIndex(std::string const& source) : m_source(&source) {
    m_lineStarts.push_back(0);
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\n') m_lineStarts.push_back(i + 1);
    }
}

SourcePos LineIndex::posOf(std::size_t offset) const {
    std::size_t const n = m_source->size();
    if (offset > n) offset = n;
    // Binary search for the line containing `offset`.
    auto it = std::upper_bound(m_lineStarts.begin(), m_lineStarts.end(), offset);
    std::size_t lineIndex = static_cast<std::size_t>(it - m_lineStarts.begin()) - 1;
    SourcePos pos;
    pos.line = lineIndex + 1;
    pos.column = offset - m_lineStarts[lineIndex] + 1;
    return pos;
}

std::string LineIndex::lineText(std::size_t line) const {
    if (line == 0 || line > m_lineStarts.size()) return {};
    std::size_t start = m_lineStarts[line - 1];
    std::size_t end = (line < m_lineStarts.size()) ? m_lineStarts[line] : m_source->size();
    while (end > start && ((*m_source)[end - 1] == '\n' || (*m_source)[end - 1] == '\r')) --end;
    return m_source->substr(start, end - start);
}

} // namespace gdcode
