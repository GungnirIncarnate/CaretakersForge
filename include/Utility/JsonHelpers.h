#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include "nlohmann/json.hpp"

namespace Forge::JsonHelpers {
    std::optional<nlohmann::json> ParseJsonFile(const std::filesystem::path& filePath);

    void ParseJsonFilesInPath(const std::filesystem::path& folder,
        const std::function<void(const std::filesystem::path&, nlohmann::json&&)>& callback);
}
