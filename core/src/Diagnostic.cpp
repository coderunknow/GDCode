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

} // namespace gdcode
