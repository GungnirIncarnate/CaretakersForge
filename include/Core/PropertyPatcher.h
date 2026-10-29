#pragma once

#include <map>
#include "Helpers/String.hpp"
#include "nlohmann/json.hpp"

namespace RC::Unreal {
    class UStruct;
    class FProperty;
}

namespace Forge {
    // Tracks which mod last wrote each property so overwrites can be reported.
    struct PatchContext {
        RC::StringType ModName;
        std::map<RC::StringType, RC::StringType>* WrittenBy{};
        // Set when a referenced object isn't loaded yet; the caller should retry the patch later.
        bool MissingReference{};
    };

    class PropertyPatcher {
    public:
        // Applies every key of `patch` onto the matching property of the struct/object at `container`.
        // Arrays and maps are replaced wholesale; keys starting with '$' are reserved for directives.
        static bool ApplyObject(RC::Unreal::UStruct* structType, void* container, const nlohmann::json& patch,
            const RC::StringType& path, PatchContext& context);

        // Logs every property of the struct (name, type, simple values).
        static void Dump(RC::Unreal::UStruct* structType, void* container, const RC::StringType& label);

    private:
        static bool ApplyValue(RC::Unreal::FProperty* property, void* valuePtr, const nlohmann::json& value,
            const RC::StringType& path, PatchContext& context);
    };
}
