#pragma once

// Project persistence. One project = one folder inside the mod's save dir:
//
//   <save dir>/projects/<project id>/main.gdx       the script
//   <save dir>/projects/<project id>/project.json   name, timestamps, link to
//                                                   the generated GD level
//
// Plain files on purpose: users can back them up, share them or edit them
// with an external editor.

#include "../backend/LevelBackend.hpp"

#include <Geode/Result.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gdcode::storage {

struct ProjectMeta {
    std::string id;           ///< folder name; stable for the project's lifetime
    std::string name;         ///< display name
    std::int64_t createdAt = 0; ///< unix seconds
    std::int64_t updatedAt = 0;
    std::optional<backend::LevelLink> linkedLevel;
    std::size_t lastObjectCount = 0;
};

struct Project {
    ProjectMeta meta;
    std::string source;
};

class ProjectStore {
public:
    static ProjectStore& get();

    std::filesystem::path root() const;

    /// All projects, most recently updated first. Broken folders are skipped
    /// (and logged), never fatal.
    std::vector<ProjectMeta> list();

    geode::Result<Project> load(std::string const& id);
    geode::Result<> save(Project const& project);
    geode::Result<Project> create(std::string const& name, std::string const& source);
    geode::Result<> remove(std::string const& id);

    /// Default script placed into a brand-new project.
    static std::string starterSource(std::string const& projectName);

private:
    std::filesystem::path dirFor(std::string const& id) const;
    geode::Result<ProjectMeta> readMeta(std::filesystem::path const& dir) const;
    geode::Result<> writeMeta(ProjectMeta const& meta) const;
};

} // namespace gdcode::storage
