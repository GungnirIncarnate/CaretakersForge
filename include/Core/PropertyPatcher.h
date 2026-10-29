#pragma once

#include <map>
#include "Helpers/String.hpp"
#include "nlohmann/json.hpp"

namespace RC::Unreal {
    class UStruct;
    class FProperty;
}

namespace Forge {
    struct PatchContext {
        RC::StringType ModName;
        std::map<RC::StringType, RC::StringType>* WrittenBy{};
        bool MissingReference{};
    };

    class PropertyPatcher {
    public:
        static bool ApplyObject(RC::Unreal::UStruct* structType, void* container, const nlohmann::json& patch,
            const RC::StringType& path, PatchContext& context);

        static void Dump(RC::Unreal::UStruct* structType, void* container, const RC::StringType& label);

    private:
        static bool ApplyValue(RC::Unreal::FProperty* property, void* valuePtr, const nlohmann::json& value,
            const RC::StringType& path, PatchContext& context);
    };
}
