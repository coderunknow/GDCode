#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gdcode {

/// One named variant of an object type, mapped to its real Geometry Dash
/// object ID.
struct VariantInfo {
    std::string_view name;
    int gdId = 0;
};

/// A DSL object type ("block", "spike", "orb", ...).
///
/// The compiler is table-driven: adding an object is adding a row here, not
/// adding an if/else branch in the parser. Every gdId in this table is cross
/// checked by tools/check_gd_api.py against the community-verified dataset
/// (see docs/DATA-SOURCES.md).
struct ObjectSpec {
    std::string_view type;
    /// Used when the statement names no variant; -1 = a variant is required.
    int defaultId = 0;
    /// Named variants (may be empty).
    std::vector<VariantInfo> variants;
    /// Emit object key 13 ("special checked"), like the editor does for
    /// freshly placed portals.
    bool checkedByDefault = false;
};

/// The full MVP object catalog.
std::vector<ObjectSpec> const& objectCatalog();

/// Find a type by exact name (case-sensitive).
ObjectSpec const* findObjectType(std::string_view type);

/// Resolve (type, variant) -> GD object id. `variant` may be empty.
std::optional<int> resolveObjectId(std::string_view type, std::string_view variant);

/// "did you mean" suggestions: catalog names within edit distance <= 2.
std::vector<std::string> suggestObjectTypes(std::string_view typo);
std::vector<std::string> suggestVariants(std::string_view type, std::string_view typo);

/// Levenshtein distance (used for suggestions and exposed for tests).
std::size_t editDistance(std::string_view a, std::string_view b);

/// Allowed range for the `obj <id>` escape hatch.
constexpr int kMinRawObjectId = 1;
constexpr int kMaxRawObjectId = 4539; ///< last 2.2 object id (floppy disc)

} // namespace gdcode
