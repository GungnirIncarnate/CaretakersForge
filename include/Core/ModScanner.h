#pragma once

#include <filesystem>
#include <vector>
#include "Helpers/String.hpp"
#include "nlohmann/json.hpp"

namespace Forge {
    struct PatchFile {
        std::filesystem::path Path;
        nlohmann::json Data;
    };

    struct ModInfo {
        RC::StringType Name;
        std::filesystem::path Path;
        std::vector<PatchFile> AssetPatches;
    };

    // Discovers mods under Mods/CaretakersForge/mods/<ModName>/ and reads their asset patch files.
    class ModScanner {
    public:
        static std::filesystem::path GetModsRoot();

        // Mods are returned in load order (currently alphabetical by folder name).
        static std::vector<ModInfo> Scan();
    };
}
