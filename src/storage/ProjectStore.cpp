#include "ProjectStore.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>

#include <algorithm>
#include <chrono>
#include <random>

using namespace geode::prelude;

namespace gdcode::storage {

namespace {

std::int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string newProjectId() {
    // time-ordered + random suffix: sorts chronologically, never collides in
    // practice, and stays a safe directory name on every platform.
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 0xFFFF);
    return fmt::format("p{:x}-{:04x}", nowSeconds(), dist(rd));
}

bool validId(std::string const& id) {
    if (id.empty() || id.size() > 64) return false;
    for (char c : id) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
        if (!ok) return false;
    }
    return true;
}

} // namespace

ProjectStore& ProjectStore::get() {
    static ProjectStore instance;
    return instance;
}

std::filesystem::path ProjectStore::root() const {
    return Mod::get()->getSaveDir() / "projects";
}

std::filesystem::path ProjectStore::dirFor(std::string const& id) const {
    return root() / id;
}

std::vector<ProjectMeta> ProjectStore::list() {
    std::vector<ProjectMeta> out;
    std::error_code ec;
    if (!std::filesystem::exists(root(), ec)) return out;
    for (auto const& entry : std::filesystem::directory_iterator(root(), ec)) {
        if (!entry.is_directory()) continue;
        auto meta = readMeta(entry.path());
        if (meta) {
            out.push_back(meta.unwrap());
        } else {
            log::warn("GDCode: skipping project folder {}: {}", entry.path().string(),
                      meta.unwrapErr());
        }
    }
    std::sort(out.begin(), out.end(), [](ProjectMeta const& a, ProjectMeta const& b) {
        return a.updatedAt > b.updatedAt;
    });
    return out;
}

Result<ProjectMeta> ProjectStore::readMeta(std::filesystem::path const& dir) const {
    auto text = file::readString(dir / "project.json");
    if (!text) return Err("cannot read project.json: {}", text.unwrapErr());
    auto parsed = matjson::parse(text.unwrap());
    if (!parsed) return Err("project.json is not valid JSON");
    auto json = parsed.unwrap();

    ProjectMeta meta;
    meta.id = dir.filename().string();
    if (!validId(meta.id)) return Err("folder name is not a valid project id");
    meta.name = json["name"].asString().unwrapOr(meta.id);
    meta.createdAt = json["created_at"].asInt().unwrapOr(0);
    meta.updatedAt = json["updated_at"].asInt().unwrapOr(0);
    meta.lastObjectCount =
        static_cast<std::size_t>(json["last_object_count"].asInt().unwrapOr(0));
    if (json.contains("linked_level") && json["linked_level"].isObject()) {
        backend::LevelLink link;
        link.levelName = json["linked_level"]["name"].asString().unwrapOr("");
        link.fingerprint = json["linked_level"]["fingerprint"].asString().unwrapOr("");
        if (!link.levelName.empty()) meta.linkedLevel = link;
    }
    return Ok(meta);
}

Result<> ProjectStore::writeMeta(ProjectMeta const& meta) const {
    matjson::Value json = matjson::Value::object();
    json["name"] = meta.name;
    json["created_at"] = meta.createdAt;
    json["updated_at"] = meta.updatedAt;
    json["last_object_count"] = static_cast<std::int64_t>(meta.lastObjectCount);
    json["format"] = "gdcode-project-v1";
    if (meta.linkedLevel) {
        matjson::Value link = matjson::Value::object();
        link["name"] = meta.linkedLevel->levelName;
        link["fingerprint"] = meta.linkedLevel->fingerprint;
        json["linked_level"] = link;
    } else {
        json["linked_level"] = nullptr;
    }
    return file::writeStringSafe(dirFor(meta.id) / "project.json", json.dump());
}

Result<Project> ProjectStore::load(std::string const& id) {
    if (!validId(id)) return Err("invalid project id");
    auto dir = dirFor(id);
    auto meta = readMeta(dir);
    if (!meta) return Err(meta.unwrapErr());
    auto source = file::readString(dir / "main.gdx");
    if (!source) return Err("cannot read main.gdx: {}", source.unwrapErr());
    Project project;
    project.meta = meta.unwrap();
    project.source = source.unwrap();
    return Ok(project);
}

Result<> ProjectStore::save(Project const& project) {
    if (!validId(project.meta.id)) return Err("invalid project id");
    auto dir = dirFor(project.meta.id);
    GEODE_UNWRAP(file::createDirectoryAll(dir));
    ProjectMeta meta = project.meta;
    meta.updatedAt = nowSeconds();
    GEODE_UNWRAP(file::writeStringSafe(dir / "main.gdx", project.source));
    GEODE_UNWRAP(writeMeta(meta));
    return Ok();
}

Result<Project> ProjectStore::create(std::string const& name, std::string const& source) {
    Project project;
    project.meta.id = newProjectId();
    project.meta.name = name.empty() ? "Untitled project" : name;
    project.meta.createdAt = nowSeconds();
    project.meta.updatedAt = project.meta.createdAt;
    project.source = source;
    GEODE_UNWRAP(save(project));
    return Ok(project);
}

Result<> ProjectStore::remove(std::string const& id) {
    if (!validId(id)) return Err("invalid project id");
    std::error_code ec;
    std::filesystem::remove_all(dirFor(id), ec);
    if (ec) return Err("cannot delete project folder: {}", ec.message());
    return Ok();
}

std::string ProjectStore::starterSource(std::string const& projectName) {
    std::string title = projectName.empty() ? "My Level" : projectName;
    // Level names are plain text; strip quotes so the starter compiles.
    std::erase(title, '"');
    std::erase(title, '\\');
    return fmt::format(
        "# {0}\n"
        "# Coordinates are in blocks: x to the right, y up, y = 0 sits on the ground.\n"
        "\n"
        "level \"{0}\" {{\n"
        "    speed: normal\n"
        "    mode: cube\n"
        "    bg: #287DC8\n"
        "    ground: #0F5FA8\n"
        "}}\n"
        "\n"
        "repeat 30 as i {{\n"
        "    block i 0\n"
        "}}\n"
        "\n"
        "spike 8 1\n"
        "spike 12 1\n"
        "orb yellow 18 3\n"
        "block 22 1\n",
        title);
}

} // namespace gdcode::storage
