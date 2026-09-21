#include "gdcode/Lowering.hpp"

#include "gdcode/Catalog.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <unordered_set>

namespace gdcode {

namespace {

constexpr double kGridUnit = 30.0;  // one GD block == 30 position units
constexpr double kCellCenter = 15.0;

bool isIntegral(double v) { return std::floor(v) == v && std::fabs(v) < 1e12; }

template <typename It>
std::string joinNamesRange(It begin, It end, std::size_t max = 8) {
    std::vector<std::string> names(begin, end);
    std::string out;
    for (std::size_t i = 0; i < names.size() && i < max; ++i) {
        if (i) out += ", ";
        out += "'" + names[i] + "'";
    }
    if (names.size() > max) out += ", ...";
    return out;
}

std::string joinNames(std::vector<std::string> const& names, std::size_t max = 8) {
    std::string out;
    for (std::size_t i = 0; i < names.size() && i < max; ++i) {
        if (i) out += ", ";
        out += "'" + names[i] + "'";
    }
    if (names.size() > max) out += ", ...";
    return out;
}

std::vector<std::string> suggestAmong(std::string_view typo,
                                      std::initializer_list<std::string_view> names) {
    std::vector<std::pair<std::size_t, std::string>> scored;
    for (auto name : names) {
        std::size_t d = editDistance(typo, name);
        if (d <= (typo.size() <= 3 ? 1 : 2)) scored.emplace_back(d, std::string(name));
    }
    std::sort(scored.begin(), scored.end());
    std::vector<std::string> out;
    for (auto& [d, n] : scored) {
        if (out.size() >= 3) break;
        out.push_back(std::move(n));
    }
    return out;
}

} // namespace

Lowering::Lowering(Program const& program, DiagnosticBag& diags, Limits limits,
                   std::string defaultLevelName)
    : m_program(program), m_diags(diags), m_limits(std::move(limits)) {
    m_ir.settings.name = defaultLevelName.empty() ? "Untitled" : std::move(defaultLevelName);
}

LevelIR Lowering::run() {
    m_deadline = std::chrono::steady_clock::now() +
                 std::chrono::milliseconds(m_limits.compileTimeBudgetMs);

    // Pass 1: level settings first, so `seed:` is known before any random().
    for (auto const& stmt : m_program.statements) {
        if (stmt.kind == Stmt::Kind::Level) execLevel(stmt);
    }
    m_rng.reseed(m_ir.settings.seed.value_or(1));

    // Pass 2: hoist top-level pattern definitions so call order does not
    // matter (a pattern may be used above its definition).
    for (auto const& stmt : m_program.statements) {
        if (stmt.kind == Stmt::Kind::Define) registerMacro(stmt);
    }

    // Pass 3: execute everything else in order.
    for (auto const& stmt : m_program.statements) {
        if (m_stats.abortedByLimit) break;
        if (stmt.kind == Stmt::Kind::Level || stmt.kind == Stmt::Kind::Define) {
            continue; // already applied
        }
        execStatement(stmt, 0);
    }

    // Object-count sanity for the backend even when no explicit error fired.
    if (m_ir.objects.size() > m_limits.maxObjects) {
        m_ir.objects.resize(m_limits.maxObjects);
    }
    return m_ir;
}

bool Lowering::checkBudget() {
    ++m_stats.expandedStatements;
    if (m_stats.expandedStatements > m_limits.maxExpandedStatements) {
        if (!m_budgetErrorReported) {
            m_budgetErrorReported = true;
            Diagnostic d;
            d.severity = Severity::Error;
            d.code = "expansion-limit";
            d.message = "statement expansion limit reached (" +
                        std::to_string(m_limits.maxExpandedStatements) +
                        " statements) - generation stopped to protect the game";
            m_diags.report(std::move(d));
        }
        m_stats.abortedByLimit = true;
        return false;
    }
    if ((m_stats.expandedStatements & 0x3FF) == 0) {
        if (std::chrono::steady_clock::now() > m_deadline) {
            if (!m_budgetErrorReported) {
                m_budgetErrorReported = true;
                Diagnostic d;
                d.severity = Severity::Error;
                d.code = "time-limit";
                d.message = "compile time budget exceeded (" +
                            std::to_string(m_limits.compileTimeBudgetMs) +
                            " ms) - generation stopped";
                m_diags.report(std::move(d));
            }
            m_stats.abortedByLimit = true;
            return false;
        }
    }
    return true;
}

bool Lowering::isReservedName(std::string const& name) {
    static std::set<std::string> const keywords = {
        "level", "var", "repeat", "define", "as", "true", "false", "random", "obj",
    };
    if (keywords.count(name)) return true;
    return findObjectType(name) != nullptr;
}

void Lowering::registerMacro(Stmt const& stmt) {
    if (isReservedName(stmt.name)) {
        m_diags.error(stmt.nameSpan, "reserved-name",
                      "'" + stmt.name + "' is a reserved name and cannot be used "
                      "as a pattern name");
        return;
    }
    if (m_macros.count(stmt.name)) {
        m_diags.error(stmt.nameSpan, "redefined-pattern",
                      "pattern '" + stmt.name + "' is already defined");
        return;
    }
    if (stmt.params.size() > m_limits.maxMacroArgs) {
        m_diags.error(stmt.nameSpan, "too-many-params",
                      "pattern '" + stmt.name + "' has more than " +
                          std::to_string(m_limits.maxMacroArgs) + " parameters");
        return;
    }
    std::unordered_set<std::string> seen;
    for (auto const& p : stmt.params) {
        if (isReservedName(p)) {
            m_diags.error(stmt.nameSpan, "reserved-name",
                          "parameter '" + p + "' of pattern '" + stmt.name +
                              "' uses a reserved name");
            return;
        }
        if (!seen.insert(p).second) {
            m_diags.error(stmt.nameSpan, "duplicate-param",
                          "duplicate parameter '" + p + "' in pattern '" +
                              stmt.name + "'");
            return;
        }
    }
    m_macros[stmt.name] = Macro{stmt.params, &stmt.body, stmt.span};
}

double* Lowering::lookupVar(std::string const& name) {
    for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
        auto found = it->vars.find(name);
        if (found != it->vars.end()) return &found->second;
    }
    return nullptr;
}

void Lowering::execStatements(std::vector<Stmt> const& stmts, int macroDepth) {
    for (auto const& stmt : stmts) {
        if (m_stats.abortedByLimit) return;
        execStatement(stmt, macroDepth);
    }
}

void Lowering::execStatement(Stmt const& stmt, int macroDepth) {
    if (!checkBudget()) return;
    switch (stmt.kind) {
        case Stmt::Kind::Object: execObject(stmt); return;
        case Stmt::Kind::Repeat: execRepeat(stmt, macroDepth); return;
        case Stmt::Kind::Call: execCall(stmt, macroDepth); return;
        case Stmt::Kind::Level:
            m_diags.error(stmt.span, "level-block-placement",
                          "'level' blocks must appear at the top level, not "
                          "inside repeat/define blocks");
            return;
        case Stmt::Kind::Define:
            // Only top-level definitions are hoisted in run(); reaching one
            // here means it is nested inside a repeat/define body.
            m_diags.error(stmt.span, "nested-define",
                          "'define' blocks must appear at the top level, not "
                          "inside repeat/define blocks");
            return;
        case Stmt::Kind::Var: {
            if (isReservedName(stmt.name)) {
                m_diags.error(stmt.nameSpan, "reserved-name",
                              "'" + stmt.name + "' is a reserved name and cannot "
                              "be used as a variable");
                return;
            }
            double value = 0.0;
            if (!evalExpr(stmt.init, value)) return;
            auto& scope = m_scopes.back().vars;
            if (scope.count(stmt.name)) {
                m_diags.error(stmt.nameSpan, "redefined-variable",
                              "variable '" + stmt.name +
                                  "' is already defined in this scope");
                return;
            }
            scope[stmt.name] = value;
            return;
        }
    }
}

void Lowering::execObject(Stmt const& stmt) {
    ObjectIR obj;
    obj.span = stmt.span;
    obj.dslType = stmt.type;
    obj.dslVariant = stmt.variant;

    double gridX = 0.0, gridY = 0.0;

    if (stmt.type == "obj") {
        // Escape hatch: obj <id> <x> <y> [props]
        if (!stmt.thirdPresent) {
            Diagnostic d;
            d.severity = Severity::Error;
            d.span = stmt.span;
            d.code = "bad-object";
            d.message = "'obj' needs three values: obj <id> <x> <y>";
            d.notes.push_back("example: obj 1 5 0   (raw GD object id 1 at block 5,0)");
            m_diags.report(std::move(d));
            return;
        }
        double idVal = 0.0, xVal = 0.0, yVal = 0.0;
        if (!evalExpr(stmt.x, idVal)) return;
        if (!evalExpr(stmt.y, xVal)) return;
        if (!evalExpr(stmt.init, yVal)) return;
        if (!isIntegral(idVal) || idVal < kMinRawObjectId ||
            idVal > kMaxRawObjectId) {
            m_diags.error(stmt.span, "bad-object-id",
                          "'obj' id must be an integer between " +
                              std::to_string(kMinRawObjectId) + " and " +
                              std::to_string(kMaxRawObjectId));
            return;
        }
        obj.id = static_cast<int>(idVal);
        gridX = xVal;
        gridY = yVal;
    } else {
        ObjectSpec const* spec = findObjectType(stmt.type);
        if (!spec) {
            Diagnostic d;
            d.severity = Severity::Error;
            d.span = stmt.typeSpan;
            d.code = "unknown-object";
            d.message = "unknown object type '" + stmt.type + "'";
            auto suggestions = suggestObjectTypes(stmt.type);
            if (!suggestions.empty()) d.suggestion = joinNames(suggestions, 3);
            if (m_macros.count(stmt.type)) {
                d.notes.push_back("'" + stmt.type +
                                  "' is a pattern - call it with parentheses: " +
                                  stmt.type + "(...)");
            }
            m_diags.report(std::move(d));
            return;
        }

        std::optional<int> id = resolveObjectId(stmt.type, stmt.variant);
        if (!id) {
            if (stmt.variant.empty() && !spec->variants.empty()) {
                Diagnostic d;
                d.severity = Severity::Error;
                d.span = stmt.typeSpan;
                d.code = "missing-variant";
                d.message = "'" + stmt.type + "' requires a variant";
                std::vector<std::string> names;
                for (auto const& v : spec->variants)
                    names.emplace_back(v.name);
                d.notes.push_back("usage: " + stmt.type + " <variant> <x> <y>");
                d.notes.push_back("variants: " + joinNames(names, 12));
                m_diags.report(std::move(d));
                return;
            }
            if (!stmt.variant.empty()) {
                Diagnostic d;
                d.severity = Severity::Error;
                d.span = stmt.variantSpan;
                d.code = "unknown-variant";
                d.message = "unknown variant '" + stmt.variant + "' for '" +
                            stmt.type + "'";
                auto suggestions = suggestVariants(stmt.type, stmt.variant);
                if (!suggestions.empty()) d.suggestion = joinNames(suggestions, 3);
                m_diags.report(std::move(d));
                return;
            }
            m_diags.error(stmt.span, "bad-object",
                          "cannot resolve object '" + stmt.type + "'");
            return;
        }
        obj.id = *id;
        obj.specialChecked = spec->checkedByDefault;

        double xVal = 0.0, yVal = 0.0;
        if (!evalExpr(stmt.x, xVal)) return;
        if (!evalExpr(stmt.y, yVal)) return;
        gridX = xVal;
        gridY = yVal;
    }

    if (std::fabs(gridX) > m_limits.maxCoordBlocks ||
        std::fabs(gridY) > m_limits.maxCoordBlocks) {
        m_diags.error(stmt.span, "coord-out-of-range",
                      "coordinates out of range (|x|, |y| must be <= " +
                          std::to_string(static_cast<long long>(
                              m_limits.maxCoordBlocks)) +
                          " blocks)");
        return;
    }
    obj.x = gridX * kGridUnit + kCellCenter;
    obj.y = gridY * kGridUnit + kCellCenter;

    // ---- properties ----
    for (auto const& prop : stmt.props) {
        if (prop.valueKind == Prop::ValueKind::Color ||
            prop.valueKind == Prop::ValueKind::Str) {
            m_diags.error(prop.valueSpan, "bad-property-value",
                          "property '" + prop.key +
                              "' expects a number or true/false here");
            continue;
        }
        auto boolValue = [&](bool& out) -> bool {
            auto name = propEnumName(prop);
            if (name && (*name == "true" || *name == "false")) {
                out = (*name == "true");
                return true;
            }
            m_diags.error(prop.valueSpan, "bad-property-value",
                          "property '" + prop.key + "' expects true or false");
            return false;
        };
        auto intValue = [&](int& out, long long lo, long long hi) -> bool {
            double v = 0.0;
            if (!evalPropNumber(prop, v)) return false;
            if (!isIntegral(v) || v < static_cast<double>(lo) ||
                v > static_cast<double>(hi)) {
                m_diags.error(prop.valueSpan, "bad-property-value",
                              "property '" + prop.key +
                                  "' expects an integer between " +
                                  std::to_string(lo) + " and " +
                                  std::to_string(hi));
                return false;
            }
            out = static_cast<int>(v);
            return true;
        };

        if (prop.key == "rot") {
            if (!evalPropNumber(prop, obj.rotation)) continue;
        } else if (prop.key == "scale") {
            if (!evalPropNumber(prop, obj.scale)) continue;
            if (obj.scale <= 0.0) {
                m_diags.error(prop.valueSpan, "bad-property-value",
                              "scale must be > 0");
                obj.scale = 1.0;
            }
        } else if (prop.key == "flipX") {
            boolValue(obj.flipX);
        } else if (prop.key == "flipY") {
            boolValue(obj.flipY);
        } else if (prop.key == "group") {
            intValue(obj.groupId, 0, 999);
        } else if (prop.key == "color") {
            // 1..999 user channels, 1000..1014 special channels
            // (BG, G, Line, 3DL, Obj, P1, P2, LBG, G2, Black, White, Lighter, MG, MG2)
            intValue(obj.colorChannel, 0, 1014);
        } else if (prop.key == "layer") {
            intValue(obj.editorLayer, 0, 9999);
        } else if (prop.key == "zOrder") {
            intValue(obj.zOrder, -9999, 9999);
        } else {
            Diagnostic d;
            d.severity = Severity::Error;
            d.span = prop.keySpan;
            d.code = "unknown-property";
            d.message = "unknown property '" + prop.key + "'";
            auto suggestions = suggestAmong(prop.key, {"rot", "scale", "flipX",
                                                       "flipY", "group", "color",
                                                       "layer", "zOrder"});
            if (!suggestions.empty()) d.suggestion = joinNames(suggestions, 3);
            m_diags.report(std::move(d));
        }
    }

    addObject(std::move(obj), stmt);
}

void Lowering::addObject(ObjectIR obj, Stmt const& from) {
    if (m_ir.objects.size() >= m_limits.maxObjects) {
        if (!m_budgetErrorReported) {
            m_budgetErrorReported = true;
            Diagnostic d;
            d.severity = Severity::Error;
            d.span = from.span;
            d.code = "object-limit";
            d.message = "object limit reached (" +
                        std::to_string(m_limits.maxObjects) +
                        " objects) - generation stopped to protect the game";
            m_diags.report(std::move(d));
        }
        m_stats.abortedByLimit = true;
        return;
    }
    m_ir.objects.push_back(std::move(obj));
}

void Lowering::execRepeat(Stmt const& stmt, int macroDepth) {
    int& nesting = m_repeatDepth;
    if (nesting + 1 > m_limits.maxRepeatNesting) {
        m_diags.error(stmt.span, "repeat-nesting",
                      "repeat blocks nested more than " +
                          std::to_string(m_limits.maxRepeatNesting) +
                          " levels deep");
        m_stats.abortedByLimit = true;
        return;
    }

    double countVal = 0.0;
    if (!evalExpr(stmt.count, countVal)) return;
    if (!isIntegral(countVal)) {
        m_diags.error(stmt.count.span, "bad-repeat-count",
                      "repeat count must be a whole number");
        return;
    }
    if (countVal < 0.0) {
        m_diags.error(stmt.count.span, "bad-repeat-count",
                      "repeat count cannot be negative");
        return;
    }
    if (countVal > static_cast<double>(m_limits.maxRepeatCount)) {
        m_diags.error(stmt.count.span, "repeat-limit",
                      "repeat count " + std::to_string(static_cast<long long>(countVal)) +
                          " exceeds the limit of " +
                          std::to_string(m_limits.maxRepeatCount));
        return;
    }

    if (!stmt.loopVar.empty() && isReservedName(stmt.loopVar)) {
        m_diags.error(stmt.loopVarSpan, "reserved-name",
                      "'" + stmt.loopVar + "' is a reserved name and cannot be "
                      "used as a loop variable");
        return;
    }

    ++nesting;
    long long const n = static_cast<long long>(countVal);
    for (long long i = 0; i < n; ++i) {
        if (m_stats.abortedByLimit) break;
        // Fresh scope per iteration: `var` declarations inside the body are
        // re-created each time instead of colliding with the previous pass.
        m_scopes.emplace_back();
        if (!stmt.loopVar.empty()) {
            m_scopes.back().vars[stmt.loopVar] = static_cast<double>(i);
        }
        execStatements(stmt.body, macroDepth);
        m_scopes.pop_back();
    }
    --nesting;
}

void Lowering::execCall(Stmt const& stmt, int macroDepth) {
    if (stmt.name == "random") {
        m_diags.error(stmt.nameSpan, "bad-call",
                      "random(a, b) is an expression, not a statement");
        return;
    }
    auto it = m_macros.find(stmt.name);
    if (it == m_macros.end()) {
        Diagnostic d;
        d.severity = Severity::Error;
        d.span = stmt.nameSpan;
        d.code = "unknown-pattern";
        d.message = "unknown pattern '" + stmt.name + "'";
        std::vector<std::pair<std::size_t, std::string>> scored;
        for (auto const& [name, macro] : m_macros) {
            std::size_t dist = editDistance(stmt.name, name);
            if (dist <= 2) scored.emplace_back(dist, name);
        }
        std::sort(scored.begin(), scored.end());
        std::vector<std::string> suggestions;
        for (auto& [dist, name] : scored) {
            if (suggestions.size() >= 3) break;
            suggestions.push_back(name);
        }
        if (!suggestions.empty()) d.suggestion = joinNames(suggestions, 3);
        if (m_macros.empty()) {
            d.notes.push_back("no patterns are defined - use 'define name(args) "
                              "{ ... }' first");
        }
        m_diags.report(std::move(d));
        return;
    }
    Macro const& macro = it->second;
    if (stmt.callArgs.size() != macro.params.size()) {
        m_diags.error(stmt.span, "arg-count",
                      "pattern '" + stmt.name + "' expects " +
                          std::to_string(macro.params.size()) +
                          " argument(s), got " +
                          std::to_string(stmt.callArgs.size()));
        return;
    }
    if (macroDepth + 1 > m_limits.maxMacroDepth) {
        m_diags.error(stmt.span, "pattern-depth",
                      "pattern expansion nested more than " +
                          std::to_string(m_limits.maxMacroDepth) +
                          " levels deep (infinite recursion?)");
        m_stats.abortedByLimit = true;
        return;
    }

    // Evaluate arguments in the caller scope before opening the new one.
    std::vector<double> values;
    values.reserve(stmt.callArgs.size());
    for (auto const& arg : stmt.callArgs) {
        double v = 0.0;
        if (!evalExpr(arg, v)) return;
        values.push_back(v);
    }

    m_scopes.emplace_back();
    for (std::size_t i = 0; i < macro.params.size(); ++i) {
        m_scopes.back().vars[macro.params[i]] = values[i];
    }
    execStatements(*macro.body, macroDepth + 1);
    m_scopes.pop_back();
}

void Lowering::execLevel(Stmt const& stmt) {
    if (m_levelBlockSeen) {
        m_diags.error(stmt.span, "duplicate-level",
                      "only one 'level' block is allowed per script");
        return;
    }
    m_levelBlockSeen = true;

    if (stmt.title.empty()) {
        m_diags.error(stmt.span, "empty-title", "level name cannot be empty");
    } else {
        m_ir.settings.name = stmt.title;
    }

    for (auto const& prop : stmt.settings) {
        auto const& key = prop.key;

        auto needColor = [&]() -> bool {
            if (prop.valueKind != Prop::ValueKind::Color) {
                m_diags.error(prop.valueSpan, "bad-setting",
                              "setting '" + key + "' expects a #RRGGBB color");
                return false;
            }
            return true;
        };
        auto needBool = [&](bool& out) -> bool {
            auto name = propEnumName(prop);
            if (name && (*name == "true" || *name == "false")) {
                out = (*name == "true");
                return true;
            }
            m_diags.error(prop.valueSpan, "bad-setting",
                          "setting '" + key + "' expects true or false");
            return false;
        };
        auto needInt = [&](long long lo, long long hi) -> std::optional<long long> {
            if (prop.valueKind != Prop::ValueKind::Expr) {
                m_diags.error(prop.valueSpan, "bad-setting",
                              "setting '" + key + "' expects a number");
                return std::nullopt;
            }
            double v = 0.0;
            if (!evalExpr(prop.expr, v)) return std::nullopt;
            if (!isIntegral(v) || v < static_cast<double>(lo) ||
                v > static_cast<double>(hi)) {
                m_diags.error(prop.valueSpan, "bad-setting",
                              "setting '" + key + "' expects an integer between " +
                                  std::to_string(lo) + " and " + std::to_string(hi));
                return std::nullopt;
            }
            return static_cast<long long>(v);
        };
        auto needEnum = [&](std::initializer_list<std::string_view> values)
            -> std::optional<std::string> {
            auto name = propEnumName(prop);
            if (!name) {
                m_diags.error(prop.valueSpan, "bad-setting",
                              "setting '" + key + "' expects one of: " +
                                  joinNamesRange(values.begin(), values.end(), 12));
                return std::nullopt;
            }
            for (auto v : values) {
                if (*name == v) return *name;
            }
            Diagnostic d;
            d.severity = Severity::Error;
            d.span = prop.valueSpan;
            d.code = "bad-setting";
            d.message = "invalid value '" + *name + "' for setting '" + key + "'";
            auto suggestions = suggestAmong(*name, values);
            if (!suggestions.empty()) d.suggestion = joinNames(suggestions, 3);
            d.notes.push_back("expected one of: " +
                              joinNamesRange(values.begin(), values.end(), 12));
            m_diags.report(std::move(d));
            return std::nullopt;
        };

        auto setColor = [&](int channel) {
            if (!needColor()) return;
            ColorIR c;
            c.channelId = channel;
            c.r = prop.r;
            c.g = prop.g;
            c.b = prop.b;
            // Replace if the script sets the same channel twice.
            for (auto& existing : m_ir.settings.colors) {
                if (existing.channelId == channel) {
                    existing = c;
                    return;
                }
            }
            m_ir.settings.colors.push_back(c);
        };

        if (key == "desc") {
            if (prop.valueKind != Prop::ValueKind::Str) {
                m_diags.error(prop.valueSpan, "bad-setting",
                              "setting 'desc' expects a string");
                continue;
            }
            m_ir.settings.description = prop.str;
        } else if (key == "song") {
            // Built-in soundtrack index (0 = Stereo Madness ...). GD 2.2 ships
            // 22 main-level tracks plus extra bonus tracks; allow some slack.
            if (auto v = needInt(0, 100)) m_ir.settings.audioTrack = static_cast<int>(*v);
        } else if (key == "customSong") {
            if (auto v = needInt(0, 2'000'000'000LL)) {
                m_ir.settings.customSongId = static_cast<int>(*v);
            }
        } else if (key == "seed") {
            if (auto v = needInt(0, 9'223'372'036'854'775'000LL)) {
                m_ir.settings.seed = static_cast<std::uint64_t>(*v);
                m_stats.seed = static_cast<std::uint64_t>(*v);
                m_stats.seedExplicit = true;
            }
        } else if (key == "speed") {
            if (auto v = needEnum({"slow", "normal", "fast", "faster", "fastest"})) {
                m_ir.settings.speed = *v == "slow"     ? 1
                                      : *v == "normal" ? 0
                                      : *v == "fast"   ? 2
                                      : *v == "faster" ? 3
                                                       : 4;
            }
        } else if (key == "mode") {
            if (auto v = needEnum({"cube", "ship", "ball", "ufo", "wave",
                                   "robot", "spider", "swing"})) {
                static std::map<std::string, int> const modes = {
                    {"cube", 0}, {"ship", 1}, {"ball", 2}, {"ufo", 3},
                    {"wave", 4}, {"robot", 5}, {"spider", 6}, {"swing", 7},
                };
                m_ir.settings.mode = modes.at(*v);
            }
        } else if (key == "mini") {
            needBool(m_ir.settings.mini);
        } else if (key == "dual") {
            needBool(m_ir.settings.dual);
        } else if (key == "flip") {
            needBool(m_ir.settings.flipGravity);
        } else if (key == "twoplayer") {
            needBool(m_ir.settings.twoPlayer);
        } else if (key == "platformer") {
            needBool(m_ir.settings.platformer);
        } else if (key == "bg") {
            setColor(1000);
        } else if (key == "ground") {
            setColor(1001);
        } else if (key == "line") {
            setColor(1002);
        } else if (key == "object") {
            setColor(1004);
        } else {
            Diagnostic d;
            d.severity = Severity::Error;
            d.span = prop.keySpan;
            d.code = "unknown-setting";
            d.message = "unknown level setting '" + key + "'";
            auto suggestions =
                suggestAmong(key, {"desc", "song", "customSong", "seed", "speed",
                                   "mode", "mini", "dual", "flip", "twoplayer",
                                   "platformer", "bg", "ground", "line",
                                   "object"});
            if (!suggestions.empty()) d.suggestion = joinNames(suggestions, 3);
            m_diags.report(std::move(d));
        }
    }
}

bool Lowering::evalExpr(Expr const& expr, double& out) {
    switch (expr.kind) {
        case ExprKind::Number:
            out = expr.number;
            return true;
        case ExprKind::Var: {
            if (expr.name == "true" || expr.name == "false") {
                m_diags.error(expr.span, "bad-expression",
                              "'" + expr.name +
                                  "' is only valid where a property expects "
                                  "true/false");
                return false;
            }
            double* value = lookupVar(expr.name);
            if (!value) {
                m_diags.error(expr.span, "undefined-variable",
                              "undefined variable '" + expr.name + "'");
                return false;
            }
            out = *value;
            return true;
        }
        case ExprKind::Unary: {
            double v = 0.0;
            if (!evalExpr(expr.args[0], v)) return false;
            out = -v;
            return true;
        }
        case ExprKind::Binary: {
            double lhs = 0.0, rhs = 0.0;
            if (!evalExpr(expr.args[0], lhs)) return false;
            if (!evalExpr(expr.args[1], rhs)) return false;
            switch (expr.op) {
                case Tok::Plus: out = lhs + rhs; return true;
                case Tok::Minus: out = lhs - rhs; return true;
                case Tok::Star: out = lhs * rhs; return true;
                case Tok::Slash:
                    if (rhs == 0.0) {
                        m_diags.error(expr.span, "division-by-zero",
                                      "division by zero");
                        return false;
                    }
                    out = lhs / rhs;
                    return true;
                default: break;
            }
            m_diags.error(expr.span, "bad-expression", "unsupported operator");
            return false;
        }
        case ExprKind::Call: {
            if (expr.name != "random") {
                Diagnostic d;
                d.severity = Severity::Error;
                d.span = expr.span;
                d.code = "unknown-function";
                d.message = "unknown function '" + expr.name + "'";
                d.notes.push_back("the only built-in expression function is "
                                  "random(a, b)");
                m_diags.report(std::move(d));
                return false;
            }
            if (expr.args.size() != 2) {
                m_diags.error(expr.span, "arg-count",
                              "random(a, b) expects exactly 2 arguments");
                return false;
            }
            double lo = 0.0, hi = 0.0;
            if (!evalExpr(expr.args[0], lo)) return false;
            if (!evalExpr(expr.args[1], hi)) return false;
            if (!isIntegral(lo) || !isIntegral(hi)) {
                m_diags.error(expr.span, "bad-random",
                              "random(a, b) expects whole numbers");
                return false;
            }
            long long a = static_cast<long long>(lo);
            long long b = static_cast<long long>(hi);
            if (a > b) {
                m_diags.error(expr.span, "bad-random",
                              "random(a, b) expects a <= b");
                return false;
            }
            if (b - a > m_limits.randomMaxSpan) {
                m_diags.error(expr.span, "bad-random", "random(a, b) range too large");
                return false;
            }
            if (!m_stats.randomUsed && !m_stats.seedExplicit) {
                Diagnostic d;
                d.severity = Severity::Info;
                d.span = expr.span;
                d.code = "default-seed";
                d.message = "random() used without a 'seed:' setting - using "
                            "default seed 1 so output stays deterministic";
                m_diags.report(std::move(d));
            }
            m_stats.randomUsed = true;
            out = static_cast<double>(m_rng.nextInt(a, b));
            return true;
        }
    }
    m_diags.error(expr.span, "bad-expression", "invalid expression");
    return false;
}

bool Lowering::evalPropNumber(Prop const& prop, double& out) {
    if (prop.valueKind != Prop::ValueKind::Expr) {
        m_diags.error(prop.valueSpan, "bad-property-value",
                      "property '" + prop.key + "' expects a number");
        return false;
    }
    return evalExpr(prop.expr, out);
}

std::optional<std::string> Lowering::propEnumName(Prop const& prop) const {
    if (prop.valueKind == Prop::ValueKind::Expr && prop.expr.isBareIdent()) {
        return prop.expr.name;
    }
    return std::nullopt;
}

} // namespace gdcode
