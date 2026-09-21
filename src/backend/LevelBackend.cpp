#include "LevelBackend.hpp"

#include "gdcode/Encoder.hpp"

#include <Geode/utils/base64.hpp>

using namespace geode::prelude;

namespace gdcode::backend {

std::string fingerprint(std::string const& data) {
    std::uint64_t h = 0xcbf29ce484222325ull;
    for (unsigned char c : data) {
        h ^= c;
        h *= 0x100000001b3ull;
    }
    return fmt::format("{:016x}", h);
}

namespace {

std::vector<GJGameLevel*> localLevelsNamed(std::string const& name) {
    std::vector<GJGameLevel*> out;
    auto* manager = LocalLevelManager::sharedState();
    if (!manager || !manager->m_localLevels) return out;
    for (auto* level : CCArrayExt<GJGameLevel*>(manager->m_localLevels)) {
        if (level && std::string(level->m_levelName) == name) out.push_back(level);
    }
    return out;
}

/// Make sure a level object created through the game's own factory really is
/// part of the local level list (which is what gets saved to CCLocalLevels).
void ensureInLocalLevels(GJGameLevel* level) {
    auto* manager = LocalLevelManager::sharedState();
    if (!manager || !manager->m_localLevels) return;
    if (!manager->m_localLevels->containsObject(level)) {
        log::warn("createNewLevel() did not register the level; inserting it manually");
        manager->m_localLevels->insertObject(level, 0);
        manager->updateLevelOrder();
    }
}

} // namespace

LinkedLevelLookup findLinkedLevel(std::optional<LevelLink> const& link) {
    LinkedLevelLookup result;
    if (!link || link->levelName.empty()) return result;

    auto candidates = localLevelsNamed(link->levelName);
    if (candidates.empty()) return result;

    for (auto* level : candidates) {
        if (fingerprint(std::string(level->m_levelString)) == link->fingerprint) {
            result.level = level;
            result.modifiedSinceGeneration = false;
            return result;
        }
    }
    if (candidates.size() == 1) {
        result.level = candidates.front();
        result.modifiedSinceGeneration = true;
        return result;
    }
    result.ambiguous = true;
    return result;
}

WriteResult writeLevel(LevelIR const& ir, GJGameLevel* target) {
    WriteResult result;

    // 1. Encode + compress + verify the round trip BEFORE touching any level.
    std::string raw = encodeLevelString(ir);
    std::string compressed = ZipUtils::compressString(raw, false, 0);
    if (compressed.empty()) {
        result.error = "ZipUtils::compressString returned an empty string";
        return result;
    }
    std::string roundTrip = ZipUtils::decompressString(compressed, false, 0);
    if (roundTrip != raw) {
        result.error = fmt::format(
            "level string round-trip check failed ({} raw bytes, {} after decompress)",
            raw.size(), roundTrip.size());
        return result;
    }

    // 2. Obtain the level object.
    GJGameLevel* level = target;
    if (!level) {
        level = GameLevelManager::sharedState()->createNewLevel();
        if (!level) {
            result.error = "GameLevelManager::createNewLevel() returned null";
            return result;
        }
        ensureInLocalLevels(level);
        result.createdNew = true;
    }

    // 3. Fill it in.
    level->m_levelName = ir.settings.name;
    level->m_levelString = compressed;
    level->m_levelType = GJLevelType::Editor;
    level->m_isEditable = true;
    level->m_audioTrack = ir.settings.audioTrack;
    level->m_songID = ir.settings.customSongId;
    level->m_objectCount = static_cast<int>(ir.objects.size());
    level->m_twoPlayerMode = ir.settings.twoPlayer;
    level->m_levelLength = levelLengthKey(estimateLevelSeconds(ir), ir.settings.platformer);
    if (!ir.settings.description.empty()) {
        // Descriptions are stored base64 (URL-safe) like the game does; see
        // GJGameLevel::getUnpackedLevelDescription.
        level->m_levelDesc = utils::base64::encode(
            ir.settings.description, utils::base64::Base64Variant::UrlWithPad);
    } else if (result.createdNew) {
        level->m_levelDesc = "";
    }
    level->m_hasBeenModified = true;

    if (!result.createdNew) {
        // Keep the regenerated level at the top of "My Levels".
        LocalLevelManager::sharedState()->moveLevelToTop(level);
    }

    result.level = level;
    result.link.levelName = ir.settings.name;
    result.link.fingerprint = fingerprint(compressed);
    result.rawBytes = raw.size();
    result.compressedBytes = compressed.size();

    log::info("GDCode: wrote level '{}' ({} objects, {} raw bytes, {} compressed, {})",
              ir.settings.name, ir.objects.size(), raw.size(), compressed.size(),
              result.createdNew ? "new level" : "updated in place");
    return result;
}

} // namespace gdcode::backend
