#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/Core/Containers/Map.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/ScriptContainerLayout.hpp"
#include "Unreal/UScriptStruct.hpp"
#include "Unreal/UClass.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/FText.hpp"
#include "Unreal/NameTypes.hpp"
#include "Core/PropertyPatcher.h"
#include "Utility/Logging.h"

using namespace RC;
using namespace RC::Unreal;

namespace Forge {
    namespace {
        // Accepts "/Game/Dir/DA_X.DA_X" or the FModel/export form "ClassName'/Game/Dir/DA_X.DA_X'".
        StringType ParseObjectPath(const std::string& text)
        {
            auto path = to_generic_string(text);
            auto quote = path.find(STR('\''));
            if (quote != StringType::npos)
            {
                auto end = path.rfind(STR('\''));
                if (end > quote) path = path.substr(quote + 1, end - quote - 1);
            }
            return path;
        }
    }

    bool PropertyPatcher::ApplyObject(UStruct* structType, void* container, const nlohmann::json& patch,
        const StringType& path, PatchContext& context)
    {
        if (!patch.is_object())
        {
            Log<LogLevel::Error>(STR("'{}' expected a JSON object.\n"), path);
            return false;
        }

        bool valid = true;
        for (const auto& [key, value] : patch.items())
        {
            if (key.starts_with('$')) continue;

            auto wideKey = to_generic_string(key);
            auto childPath = path.empty() ? wideKey : std::format(STR("{}.{}"), path, wideKey);

            FProperty* property = nullptr;
            for (FProperty* candidate : TFieldRange<FProperty>(structType, EFieldIterationFlags::IncludeSuper))
            {
                if (candidate->GetName() == wideKey)
                {
                    property = candidate;
                    break;
                }
            }

            if (!property)
            {
                Log<LogLevel::Error>(STR("Property '{}' does not exist (mod '{}').\n"), childPath, context.ModName);
                valid = false;
                continue;
            }

            if (!ApplyValue(property, property->ContainerPtrToValuePtr<void>(container), value, childPath, context))
            {
                valid = false;
            }
        }
        return valid;
    }

    bool PropertyPatcher::ApplyValue(FProperty* property, void* valuePtr, const nlohmann::json& value,
        const StringType& path, PatchContext& context)
    {
        auto reportWrite = [&]() {
            if (!context.WrittenBy) return;
            auto [it, inserted] = context.WrittenBy->try_emplace(path, context.ModName);
            if (!inserted && it->second != context.ModName)
            {
                Log<LogLevel::Warning>(STR("Conflict on '{}': mod '{}' overrides value set by mod '{}'.\n"), path, context.ModName, it->second);
                it->second = context.ModName;
            }
        };

        try
        {
            if (auto boolProp = CastField<FBoolProperty>(property))
            {
                boolProp->SetPropertyValue(valuePtr, value.get<bool>());
            }
            else if (auto floatProp = CastField<FFloatProperty>(property))
            {
                *static_cast<float*>(valuePtr) = value.get<float>();
            }
            else if (auto doubleProp = CastField<FDoubleProperty>(property))
            {
                *static_cast<double*>(valuePtr) = value.get<double>();
            }
            else if (auto numericProp = CastField<FNumericProperty>(property))
            {
                numericProp->SetIntPropertyValue(valuePtr, value.get<int64_t>());
            }
            else if (CastField<FNameProperty>(property))
            {
                *static_cast<FName*>(valuePtr) = FName(to_generic_string(value.get<std::string>()), FNAME_Add);
            }
            else if (CastField<FStrProperty>(property))
            {
                *static_cast<FString*>(valuePtr) = FString(to_generic_string(value.get<std::string>()).c_str());
            }
            else if (CastField<FTextProperty>(property))
            {
                *static_cast<FText*>(valuePtr) = FText(to_generic_string(value.get<std::string>()).c_str());
            }
            else if (auto objectProp = CastField<FObjectProperty>(property))
            {
                // The virtual SetObjectPropertyValue isn't available in this engine build, so write the pointer directly like PalSchema does
                if (value.is_null())
                {
                    objectProp->SetPropertyValue(valuePtr, nullptr);
                }
                else
                {
                    auto objectPath = ParseObjectPath(value.get<std::string>());
                    auto* object = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, objectPath);
                    if (!object)
                    {
                        Log<LogLevel::Warning>(STR("'{}' references '{}' which isn't loaded (yet); will retry.\n"), path, objectPath);
                        context.MissingReference = true;
                        return false;
                    }
                    objectProp->SetPropertyValue(valuePtr, object);
                }
            }
            else if (auto arrayProp = CastField<FArrayProperty>(property))
            {
                if (!value.is_array())
                {
                    Log<LogLevel::Error>(STR("Property '{}' expects a JSON array for full replacement.\n"), path);
                    return false;
                }

                auto* innerProp = arrayProp->GetInner();
                const int32 elementSize = innerProp->GetElementSize();
                const uint32 elementAlign = static_cast<uint32>(innerProp->GetMinAlignment());
                // Element writes shouldn't each register as conflicts, only the array as a whole does
                PatchContext elementContext{ context.ModName, nullptr };
                bool valid = true;

                // Elements are built in temporary buffers first so a failure never touches the live array.
                std::vector<uint8*> elements;
                for (size_t index = 0; index < value.size(); ++index)
                {
                    auto* element = static_cast<uint8*>(FMemory::Malloc(elementSize));
                    FMemory::Memzero(element, elementSize);
                    innerProp->InitializeValue(element);
                    elements.push_back(element);

                    auto elementPath = std::format(STR("{}[{}]"), path, index);
                    if (!ApplyValue(innerProp, element, value.at(index), elementPath, elementContext))
                    {
                        valid = false;
                        break;
                    }
                }
                context.MissingReference |= elementContext.MissingReference;

                if (valid)
                {
                    auto* liveArray = static_cast<FScriptArray*>(valuePtr);
                    auto* liveData = static_cast<uint8*>(liveArray->GetData());
                    for (int32 index = 0; index < liveArray->Num(); ++index)
                    {
                        innerProp->DestroyValue(liveData + static_cast<size_t>(index) * elementSize);
                    }
                    liveArray->Empty(static_cast<int32>(elements.size()), elementSize, elementAlign);
                    if (!elements.empty())
                    {
                        liveArray->Add(static_cast<int32>(elements.size()), elementSize, elementAlign);
                        auto* newData = static_cast<uint8*>(liveArray->GetData());
                        for (size_t index = 0; index < elements.size(); ++index)
                        {
                            FMemory::Memcpy(newData + index * elementSize, elements[index], elementSize);
                        }
                    }

                    // Ownership moved into the live array, so the temporary buffers are freed without destructing.
                    for (auto* element : elements) FMemory::Free(element);
                    reportWrite();
                    Log<LogLevel::Verbose>(STR("Replaced '{}' with {} array element(s) (mod '{}').\n"),
                        path, value.size(), context.ModName);
                    return true;
                }

                for (auto* element : elements)
                {
                    innerProp->DestroyValue(element);
                    FMemory::Free(element);
                }
                return false;
            }
            else if (auto mapProp = CastField<FMapProperty>(property))
            {
                if (!value.is_object() && !value.is_array())
                {
                    Log<LogLevel::Error>(STR("Property '{}' expects a JSON object or an array of {{\"Key\", \"Value\"}} entries for full replacement.\n"), path);
                    return false;
                }

                // Two accepted forms: {"key": value, ...} or FModel's [{"Key": k, "Value": v}, ...]
                struct Entry { nlohmann::json Key; nlohmann::json Value; StringType Label; };
                std::vector<Entry> entries;
                if (value.is_object())
                {
                    for (const auto& [key, jsonValue] : value.items())
                    {
                        Entry entry{ nlohmann::json(), jsonValue, to_generic_string(key) };
                        if (CastField<FNameProperty>(mapProp->GetKeyProp()) || CastField<FStrProperty>(mapProp->GetKeyProp()) ||
                            CastField<FObjectProperty>(mapProp->GetKeyProp()) || CastField<FTextProperty>(mapProp->GetKeyProp()))
                        {
                            entry.Key = key;
                        }
                        else
                        {
                            try
                            {
                                entry.Key = nlohmann::json::parse(key);
                            }
                            catch (const nlohmann::json::exception&)
                            {
                                Log<LogLevel::Error>(STR("Map key '{}' for '{}' must be valid JSON for key type {}.\n"),
                                    entry.Label, path, mapProp->GetKeyProp()->GetClass().GetName());
                                return false;
                            }
                        }
                        entries.push_back(std::move(entry));
                    }
                }
                else
                {
                    for (size_t index = 0; index < value.size(); ++index)
                    {
                        const auto& item = value.at(index);
                        if (!item.is_object() || !item.contains("Key") || !item.contains("Value"))
                        {
                            Log<LogLevel::Error>(STR("Each entry of '{}' must be an object with 'Key' and 'Value'.\n"), path);
                            return false;
                        }
                        entries.push_back({ item.at("Key"), item.at("Value"), std::format(STR("{}"), index) });
                    }
                }

                auto* keyProp = mapProp->GetKeyProp();
                auto* valueProp = mapProp->GetValueProp();
                auto layout = FScriptMap::GetScriptLayout(
                    keyProp->GetSize(), keyProp->GetMinAlignment(), valueProp->GetSize(), valueProp->GetMinAlignment());
                PatchContext elementContext{ context.ModName, nullptr };
                bool valid = true;

                // Pairs are validated in temporary buffers first so a failure never leaves the live map half-written.
                const auto pairSize = static_cast<size_t>(layout.ValueOffset + valueProp->GetSize());
                std::vector<uint8*> pairs;

                for (const auto& entry : entries)
                {
                    auto* pair = static_cast<uint8*>(FMemory::Malloc(pairSize));
                    FMemory::Memzero(pair, pairSize);
                    auto* keyValue = pair;
                    auto* mappedValue = pair + layout.ValueOffset;
                    keyProp->InitializeValue(keyValue);
                    valueProp->InitializeValue(mappedValue);
                    pairs.push_back(pair);

                    auto keyPath = std::format(STR("{}[key:{}]"), path, entry.Label);
                    auto valuePath = std::format(STR("{}[{}]"), path, entry.Label);
                    const auto& jsonKey = entry.Key;
                    const auto& jsonValue = entry.Value;
                    if (valid && !ApplyValue(keyProp, keyValue, jsonKey, keyPath, elementContext))
                    {
                        valid = false;
                    }
                    if (valid && !ApplyValue(valueProp, mappedValue, jsonValue, valuePath, elementContext))
                    {
                        valid = false;
                    }
                    if (!valid) break;
                }
                context.MissingReference |= elementContext.MissingReference;

                for (size_t a = 0; valid && a < pairs.size(); ++a)
                {
                    for (size_t b = a + 1; b < pairs.size(); ++b)
                    {
                        if (keyProp->Identical(pairs[a], pairs[b]))
                        {
                            Log<LogLevel::Error>(STR("'{}' has duplicate map keys (entries {} and {}).\n"), path, a, b);
                            valid = false;
                            break;
                        }
                    }
                }

                if (valid)
                {
                    auto* liveMap = static_cast<FScriptMap*>(valuePtr);
                    for (int32 index = 0; index < liveMap->GetMaxIndex(); ++index)
                    {
                        if (!liveMap->IsValidIndex(index)) continue;
                        auto* existing = static_cast<uint8*>(liveMap->GetData(index, layout));
                        keyProp->DestroyValue(existing);
                        valueProp->DestroyValue(existing + layout.ValueOffset);
                    }
                    liveMap->Empty(static_cast<int32>(pairs.size()), layout);

                    for (auto* pair : pairs)
                    {
                        auto index = liveMap->AddUninitialized(layout);
                        auto* slot = static_cast<uint8*>(liveMap->GetData(index, layout));
                        FMemory::Memcpy(slot, pair, keyProp->GetSize());
                        FMemory::Memcpy(slot + layout.ValueOffset, pair + layout.ValueOffset, valueProp->GetSize());
                    }
                    liveMap->Rehash(layout, [keyProp](const void* item) -> uint32 { return keyProp->GetValueTypeHash(item); });

                    // Ownership moved into the live map, so the temporary buffers are freed without destructing.
                    for (auto* pair : pairs) FMemory::Free(pair);
                    reportWrite();
                    Log<LogLevel::Verbose>(STR("Replaced '{}' with {} map entry/entries (mod '{}').\n"),
                        path, value.size(), context.ModName);
                    return true;
                }

                for (auto* pair : pairs)
                {
                    keyProp->DestroyValue(pair);
                    valueProp->DestroyValue(pair + layout.ValueOffset);
                    FMemory::Free(pair);
                }
                return false;
            }
            else if (auto structProp = CastField<FStructProperty>(property))
            {
                auto valid = ApplyObject(structProp->GetStruct().Get(), valuePtr, value, path, context);
                if (!valid) return false;
            }
            else
            {
                Log<LogLevel::Error>(STR("Property '{}' has an unsupported type ({}).\n"), path, property->GetClass().GetName());
                return false;
            }
        }
        catch (const nlohmann::json::exception& e)
        {
            Log<LogLevel::Error>(STR("Invalid value for '{}': {}\n"), path, to_generic_string(e.what()));
            return false;
        }

        reportWrite();
        Log<LogLevel::Verbose>(STR("Set '{}' (mod '{}').\n"), path, context.ModName);
        return true;
    }

    void PropertyPatcher::Dump(UStruct* structType, void* container, const StringType& label)
    {
        Log<LogLevel::Normal>(STR("--- Property dump: {} ---\n"), label);

        for (FProperty* property : TFieldRange<FProperty>(structType, EFieldIterationFlags::IncludeSuper))
        {
            void* valuePtr = property->ContainerPtrToValuePtr<void>(container);
            StringType valueText;

            if (auto boolProp = CastField<FBoolProperty>(property))
                valueText = boolProp->GetPropertyValue(valuePtr) ? STR("true") : STR("false");
            else if (CastField<FFloatProperty>(property))
                valueText = std::format(STR("{}"), *static_cast<float*>(valuePtr));
            else if (CastField<FDoubleProperty>(property))
                valueText = std::format(STR("{}"), *static_cast<double*>(valuePtr));
            else if (auto numericProp = CastField<FNumericProperty>(property))
                valueText = std::format(STR("{}"), numericProp->GetSignedIntPropertyValue(valuePtr));
            else if (CastField<FNameProperty>(property))
                valueText = static_cast<FName*>(valuePtr)->ToString();
            else if (auto arrayProp = CastField<FArrayProperty>(property))
                valueText = std::format(STR("{} element(s)"), FScriptArrayHelper(arrayProp, valuePtr).Num());
            else if (auto mapProp = CastField<FMapProperty>(property))
                valueText = std::format(STR("{} entry/entries"), static_cast<FScriptMap*>(valuePtr)->Num());

            Log<LogLevel::Normal>(STR("  {} : {} = {}\n"), property->GetName(), property->GetClass().GetName(), valueText);
        }
    }
}
