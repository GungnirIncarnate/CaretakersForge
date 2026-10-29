#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include "nlohmann/json.hpp"

namespace Forge::JsonHelpers {
    // Parses a .json/.jsonc file (comments allowed). Returns nullopt and logs on failure.
    std::optional<nlohmann::json> ParseJsonFile(const std::filesystem::path& filePath);

    // Calls the callback for every parsed .json/.jsonc file under the folder (recursive, sorted by path).
    void ParseJsonFilesInPath(const std::filesystem::path& folder,
        const std::function<void(const std::filesystem::path&, nlohmann::json&&)>& callback);
}
