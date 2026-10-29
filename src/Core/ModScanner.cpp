#include <algorithm>
#include "UE4SSProgram.hpp"
#include "Core/ModScanner.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"

namespace fs = std::filesystem;
using namespace RC;

namespace Forge {
    fs::path ModScanner::GetModsRoot()
    {
        return fs::path(UE4SSProgram::get_program().get_working_directory()) / "Mods" / "CaretakersForge" / "mods";
    }

    std::vector<ModInfo> ModScanner::Scan()
    {
        std::vector<ModInfo> mods;

        auto root = GetModsRoot();
        std::error_code ec;
        if (!fs::is_directory(root, ec))
        {
            Log<LogLevel::Warning>(STR("Mods folder not found: {}\n"), root.native());
            return mods;
        }

        for (const auto& entry : fs::directory_iterator(root, ec))
        {
            if (!entry.is_directory()) continue;

            ModInfo mod;
            mod.Path = entry.path();
            mod.Name = entry.path().filename().native();
            mods.push_back(std::move(mod));
        }

        std::ranges::sort(mods, {}, &ModInfo::Name);

        for (auto& mod : mods)
        {
            JsonHelpers::ParseJsonFilesInPath(mod.Path / "assets", [&](const fs::path& file, nlohmann::json&& data) {
                if (!data.is_object())
                {
                    Log<LogLevel::Error>(STR("'{}' must contain a JSON object at the top level, skipping.\n"), file.native());
                    return;
                }
                mod.AssetPatches.push_back({ file, std::move(data) });
            });

            Log<LogLevel::Normal>(STR("Found mod '{}' with {} asset patch file(s).\n"), mod.Name, mod.AssetPatches.size());
        }

        return mods;
    }
}
