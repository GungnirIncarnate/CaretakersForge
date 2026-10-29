#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UClass.hpp"
#include "Core/AssetPatcher.h"
#include "Core/PropertyPatcher.h"
#include "Utility/Logging.h"

using namespace RC;
using namespace RC::Unreal;

namespace Forge {
    namespace {
        // "/Game/Dir/DA_X" -> "/Game/Dir/DA_X.DA_X"
        StringType NormalizeAssetPath(StringType path)
        {
            if (path.find(STR('.')) == StringType::npos)
            {
                auto slash = path.find_last_of(STR('/'));
                path += STR('.') + (slash == StringType::npos ? path : path.substr(slash + 1));
            }
            return path;
        }
    }

    AssetPatcher::AssetPatcher(const std::vector<ModInfo>& mods)
    {
        for (const auto& mod : mods)
        {
            for (const auto& file : mod.AssetPatches)
            {
                for (const auto& [assetPath, patch] : file.Data.items())
                {
                    m_pending[NormalizeAssetPath(to_generic_string(assetPath))].Patches.push_back({ mod.Name, &patch });
                }
            }
        }

        Log<LogLevel::Normal>(STR("Queued patches for {} asset(s).\n"), m_pending.size());
    }

    void AssetPatcher::ApplyPending(const StringType& reason)
    {
        if (m_pending.empty()) return;

        Log<LogLevel::Verbose>(STR("Checking {} pending asset(s) ({}).\n"), m_pending.size(), reason);

        for (auto it = m_pending.begin(); it != m_pending.end();)
        {
            if (TryApply(it->first, it->second))
            {
                it = m_pending.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    bool AssetPatcher::TryApply(const StringType& assetPath, Target& target)
    {
        auto* asset = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, assetPath);
        if (!asset) return false;

        std::map<StringType, StringType> writtenBy;
        UClass* assetClass = asset->GetClassPrivate();
        bool missingReference = false;

        for (const auto& patch : target.Patches)
        {
            if (!patch.Data->is_object())
            {
                Log<LogLevel::Error>(STR("Patch for '{}' from mod '{}' must be an object.\n"), assetPath, patch.ModName);
                continue;
            }

            if (patch.Data->value("$dump", false))
            {
                PropertyPatcher::Dump(assetClass, asset, assetPath);
            }

            PatchContext context{ patch.ModName, &writtenBy };
            PropertyPatcher::ApplyObject(assetClass, asset, *patch.Data, STR(""), context);
            missingReference |= context.MissingReference;

            if (patch.Data->value("$dump", false))
            {
                PropertyPatcher::Dump(assetClass, asset, std::format(STR("{} (after mod '{}')"), assetPath, patch.ModName));
            }
        }

        if (missingReference)
        {
            Log<LogLevel::Verbose>(STR("'{}' has unresolved references, keeping it pending.\n"), assetPath);
            return false;
        }

        Log<LogLevel::Normal>(STR("Patched '{}' with {} patch(es).\n"), assetPath, target.Patches.size());
        return true;
    }
}
