#pragma once

#include <map>
#include <vector>
#include "Core/ModScanner.h"

namespace Forge {
    class AssetPatcher {
    public:
        explicit AssetPatcher(const std::vector<ModInfo>& mods);

        void ApplyPending(const RC::StringType& reason);

        bool HasPending() const { return !m_pending.empty(); }

    private:
        struct PendingPatch {
            RC::StringType ModName;
            const nlohmann::json* Data;
        };

        struct Target {
            std::vector<PendingPatch> Patches;
        };

        bool TryApply(const RC::StringType& assetPath, Target& target);

        std::map<RC::StringType, Target> m_pending;
    };
}
