#pragma once

#include <format>
#include <HAL/Platform.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

namespace Forge {
    inline bool g_DebugLogging = false;

    template <RC::Unreal::int32 Level, typename... FmtArgs>
    auto Log(RC::File::StringViewType content, FmtArgs... fmt_args) -> void
    {
        if constexpr (Level == RC::LogLevel::Error)
        {
            RC::Output::send<Level>(std::format(STR("[CaretakersForge] [error] {}"), content), fmt_args...);
        }
        else if constexpr (Level == RC::LogLevel::Warning)
        {
            RC::Output::send<Level>(std::format(STR("[CaretakersForge] [warning] {}"), content), fmt_args...);
        }
        else if constexpr (Level == RC::LogLevel::Verbose)
        {
            if (!g_DebugLogging) return;
            RC::Output::send<Level>(std::format(STR("[CaretakersForge] [debug] {}"), content), fmt_args...);
        }
        else
        {
            RC::Output::send<Level>(std::format(STR("[CaretakersForge] {}"), content), fmt_args...);
        }
    }
}
