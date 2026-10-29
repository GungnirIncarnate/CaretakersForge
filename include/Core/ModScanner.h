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

    class ModScanner {
    public:
        static std::filesystem::path GetModsRoot();

        static std::vector<ModInfo> Scan();
    };
}
