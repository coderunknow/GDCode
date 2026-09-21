#pragma once

#include "Span.hpp"

#include <string>
#include <utility>
#include <vector>

namespace gdcode {

enum class Severity {
    Error,
    Warning,
    Info,
};

/// One compiler diagnostic: severity, location, message, optional suggestion,
/// optional list of extra note lines.
struct Diagnostic {
    Severity severity = Severity::Error;
    SourceSpan span{};
    std::string code;         ///< machine-readable id, e.g. "unknown-object"
    std::string message;      ///< primary message
    std::string suggestion;   ///< "did you mean ..." payload (may be empty)
    std::vector<std::string> notes; ///< extra context lines

    bool isError() const { return severity == Severity::Error; }

    /// Human-readable rendering, e.g.
    /// error[unknown-object]: line 8, column 1: unknown object type 'spirke'
    std::string render() const;
};

/// Collects diagnostics. Parsing/semantic analysis never aborts on the first
/// error; it records as many useful diagnostics as it can.
///
/// Identical diagnostics (same severity, code, position and message) are
/// recorded once - a bad statement inside `repeat 1000` should not produce a
/// thousand copies of the same error. The bag is also capped so a
/// pathological script cannot allocate unbounded memory for messages.
class DiagnosticBag {
public:
    static constexpr std::size_t kMaxDiagnostics = 200;

    void report(Diagnostic diag);

    /// Sort by source position (diagnostics without a position go last).
    /// Stable, so same-position entries keep their emission order.
    void sortByPosition();

    void error(SourceSpan span, std::string code, std::string message) {
        report({Severity::Error, std::move(span), std::move(code), std::move(message), {}, {}});
    }
    void warn(SourceSpan span, std::string code, std::string message) {
        report({Severity::Warning, std::move(span), std::move(code), std::move(message), {}, {}});
    }
    void info(SourceSpan span, std::string code, std::string message) {
        report({Severity::Info, std::move(span), std::move(code), std::move(message), {}, {}});
    }

    bool hasErrors() const {
        for (auto const& d : m_items)
            if (d.isError()) return true;
        return false;
    }

    std::size_t errorCount() const {
        std::size_t n = 0;
        for (auto const& d : m_items)
            if (d.isError()) ++n;
        return n;
    }

    std::vector<Diagnostic> const& items() const { return m_items; }
    std::size_t size() const { return m_items.size(); }

    /// Drop every diagnostic recorded after `size() == count`. Used by the
    /// parser to discard the output of a speculative parse.
    void truncate(std::size_t count);

    void clear();

private:
    std::vector<Diagnostic> m_items;
    std::vector<std::string> m_keys; ///< parallel to m_items, for dedup
    bool m_overflowReported = false;
};

std::string const& severityName(Severity s);

} // namespace gdcode
