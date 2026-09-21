#include "gdcode/Diagnostic.hpp"

#include <algorithm>

namespace gdcode {

std::string const& severityName(Severity s) {
    static std::string const error = "error";
    static std::string const warning = "warning";
    static std::string const info = "info";
    switch (s) {
        case Severity::Error: return error;
        case Severity::Warning: return warning;
        case Severity::Info: return info;
    }
    return error;
}

void DiagnosticBag::report(Diagnostic diag) {
    if (m_items.size() >= kMaxDiagnostics) {
        if (!m_overflowReported) {
            m_overflowReported = true;
            Diagnostic overflow;
            overflow.severity = Severity::Info;
            overflow.code = "too-many-diagnostics";
            overflow.message = "more than " + std::to_string(kMaxDiagnostics) +
                               " diagnostics; further messages were dropped";
            m_items.push_back(std::move(overflow));
            m_keys.emplace_back();
        }
        return;
    }
    std::string key = severityName(diag.severity) + "|" + diag.code + "|" +
                      std::to_string(diag.span.begin.line) + ":" +
                      std::to_string(diag.span.begin.column) + "|" + diag.message;
    for (auto const& existing : m_keys) {
        if (existing == key) return;
    }
    m_keys.push_back(std::move(key));
    m_items.push_back(std::move(diag));
}

void DiagnosticBag::sortByPosition() {
    // Keys are only needed for de-duplication while reporting; rebuild the
    // parallel vector after sorting so truncate() stays consistent.
    std::stable_sort(m_items.begin(), m_items.end(),
                     [](Diagnostic const& a, Diagnostic const& b) {
                         bool av = a.span.valid(), bv = b.span.valid();
                         if (av != bv) return av; // positioned first
                         if (!av) return false;
                         if (a.span.begin.line != b.span.begin.line)
                             return a.span.begin.line < b.span.begin.line;
                         return a.span.begin.column < b.span.begin.column;
                     });
    m_keys.assign(m_items.size(), std::string());
}

void DiagnosticBag::truncate(std::size_t count) {
    if (count >= m_items.size()) return;
    m_items.resize(count);
    m_keys.resize(count);
}

void DiagnosticBag::clear() {
    m_items.clear();
    m_keys.clear();
    m_overflowReported = false;
}

std::string Diagnostic::render() const {
    std::string out = severityName(severity);
    if (!code.empty()) out += "[" + code + "]";
    out += ": ";
    if (span.valid()) out += span.describe() + ": ";
    out += message;
    if (!suggestion.empty()) out += "\n    did you mean: " + suggestion;
    for (auto const& note : notes) out += "\n    " + note;
    return out;
}

std::string Diagnostic::renderWithSource(LineIndex const& lines) const {
    std::string out = render();
    if (!span.valid() || span.begin.line > lines.lineCount() || span.begin.column == 0) return out;
    auto line = lines.lineText(span.begin.line);
    std::size_t column = span.begin.column - 1;
    // A caret one past the last byte is meaningful for missing-token/EOF errors.
    if (column > line.size()) return out;

    constexpr std::size_t kWindow = 120;
    std::size_t start = column > kWindow / 2 ? column - kWindow / 2 : 0;
    std::size_t end = std::min(line.size(), start + kWindow);
    std::string excerpt = start ? "..." : "";
    std::size_t caret = excerpt.size();
    for (std::size_t i = start; i < end; ++i) {
        unsigned char c = static_cast<unsigned char>(line[i]);
        std::size_t width = c == '\t' ? 4 : 1;
        if (i < column) caret += width;
        if (c == '\t') excerpt += "    ";
        else excerpt += c >= 0x20 && c < 0x7f ? static_cast<char>(c) : '?';
    }
    if (end < line.size()) excerpt += "...";
    std::string number = std::to_string(span.begin.line);
    out += "\n    " + number + " | " + excerpt;
    out += "\n    " + std::string(number.size(), ' ') + " | " + std::string(caret, ' ') + "^";
    return out;
}

} // namespace gdcode
