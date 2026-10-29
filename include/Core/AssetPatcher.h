#pragma once

#include <map>
#include <vector>
#include "Core/ModScanner.h"

namespace Forge {
    // Queues patches per target asset and applies them once the asset exists in memory.
    // Driven purely by events (see ApplyPending callers); nothing here runs per frame.
    class AssetPatcher {
    public:
        explicit AssetPatcher(const std::vector<ModInfo>& mods);

        // Tries every still-pending target. Must be called from the game thread.
        void ApplyPending(const RC::StringType& reason);

        bool HasPending() const { return !m_pending.empty(); }

    private:
        struct PendingPatch {
            RC::StringType ModName;
            const nlohmann::json* Data;
        };

        struct Target {
            std::vector<PendingPatch> Patches; // in mod load order
        };

        bool TryApply(const RC::StringType& assetPath, Target& target);

        std::map<RC::StringType, Target> m_pending;
    };
}
