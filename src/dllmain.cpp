#include "Mod/CppUserModBase.hpp"
#include "Unreal/Hooks.hpp"
#include "Core/ModScanner.h"
#include "Core/AssetPatcher.h"
#include "Utility/Logging.h"
#include "../version.h"

using namespace RC;

class CaretakersForge : public RC::CppUserModBase
{
public:
    CaretakersForge() : CppUserModBase()
    {
        ModName = STR("CaretakersForge");
        ModVersion = std::format(STR("{}.{}.{}"), FORGE_VERSION_MAJOR, FORGE_VERSION_MINOR, FORGE_VERSION_REVISION);
        ModDescription = STR("JSON based asset modding framework for The Last Caretaker.");
        ModAuthors = STR("CaretakersForge contributors");

        Forge::Log<LogLevel::Normal>(STR("{} v{} loaded.\n"), ModName, ModVersion);
    }

    ~CaretakersForge() override = default;

    auto on_unreal_init() -> void override
    {
        m_mods = Forge::ModScanner::Scan();
        m_patcher = std::make_unique<Forge::AssetPatcher>(m_mods);

        // Assets that are already resident (e.g. loaded during engine startup)
        m_patcher->ApplyPending(STR("unreal init"));

        // Event driven retries on the game thread: new maps and new game states are when
        // the game loads the bulk of its data assets.
        Unreal::Hook::RegisterLoadMapPostCallback(
            [this](auto&, Unreal::UEngine*, Unreal::FWorldContext&, Unreal::FURL, Unreal::UPendingNetGame*, Unreal::FString&) {
                m_patcher->ApplyPending(STR("map loaded"));
            },
            { false, false, STR("CaretakersForge"), STR("LoadMapPatch") });

        Unreal::Hook::RegisterInitGameStatePostCallback(
            [this](auto&, Unreal::AGameModeBase*) {
                m_patcher->ApplyPending(STR("game state init"));
            },
            { false, false, STR("CaretakersForge"), STR("InitGameStatePatch") });
    }

private:
    std::vector<Forge::ModInfo> m_mods;
    std::unique_ptr<Forge::AssetPatcher> m_patcher;
};

#define FORGE_API __declspec(dllexport)
extern "C"
{
    FORGE_API RC::CppUserModBase* start_mod()
    {
        return new CaretakersForge();
    }

    FORGE_API void uninstall_mod(RC::CppUserModBase* mod)
    {
        delete mod;
    }
}
