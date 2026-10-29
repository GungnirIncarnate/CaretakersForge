#include <algorithm>
#include <fstream>
#include <vector>
#include "Helpers/String.hpp"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"

namespace fs = std::filesystem;
using namespace RC;

namespace Forge::JsonHelpers {
    std::optional<nlohmann::json> ParseJsonFile(const fs::path& filePath)
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            Log<LogLevel::Error>(STR("Unable to open '{}'\n"), filePath.native());
            return std::nullopt;
        }

        try
        {
            return nlohmann::json::parse(file, nullptr, true, true);
        }
        catch (const std::exception& e)
        {
            Log<LogLevel::Error>(STR("Failed to parse '{}': {}\n"), filePath.native(), to_generic_string(e.what()));
            return std::nullopt;
        }
    }

    void ParseJsonFilesInPath(const fs::path& folder,
        const std::function<void(const fs::path&, nlohmann::json&&)>& callback)
    {
        std::error_code ec;
        if (!fs::is_directory(folder, ec))
        {
            return;
        }

        std::vector<fs::path> files;
        for (const auto& entry : fs::recursive_directory_iterator(folder, ec))
        {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".json" || ext == ".jsonc")
            {
                files.push_back(entry.path());
            }
        }

        std::ranges::sort(files);

        for (const auto& file : files)
        {
            if (auto parsed = ParseJsonFile(file))
            {
                callback(file, std::move(*parsed));
            }
        }
    }
}
