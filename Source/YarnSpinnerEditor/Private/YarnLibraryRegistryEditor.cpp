// Fill out your copyright notice in the Description page of Project Settings.


#include "YarnLibraryRegistryEditor.h"

#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Internationalization/Regex.h"
#include "Library/YarnCommandLibrary.h"
#include "Library/YarnFunctionLibrary.h"
#include "Library/YarnLibraryRegistry.h"
#include "Library/YarnSpinnerLibraryData.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/FileHelper.h"
#include "Misc/YarnAssetHelpers.h"
#include "Misc/YSLogging.h"


UYarnLibraryRegistryEditor::UYarnLibraryRegistryEditor()
{
    YS_LOG_FUNCSIG
    
    IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();

    // Set up asset registry delegates
    OnAssetRegistryFilesLoadedHandle = AssetRegistry.OnFilesLoaded().AddUObject(this, &UYarnLibraryRegistryEditor::OnAssetRegistryFilesLoaded);
    OnAssetAddedHandle = AssetRegistry.OnAssetAdded().AddUObject(this, &UYarnLibraryRegistryEditor::OnAssetAdded);
    OnAssetRemovedHandle = AssetRegistry.OnAssetRemoved().AddUObject(this, &UYarnLibraryRegistryEditor::OnAssetRemoved);
    OnAssetUpdatedHandle = AssetRegistry.OnAssetUpdated().AddUObject(this, &UYarnLibraryRegistryEditor::OnAssetUpdated);
    OnAssetRenamedHandle = AssetRegistry.OnAssetRenamed().AddUObject(this, &UYarnLibraryRegistryEditor::OnAssetRenamed);
    OnStartGameHandle = FWorldDelegates::OnStartGameInstance.AddUObject(this, &UYarnLibraryRegistryEditor::OnStartGameInstance);
}


void UYarnLibraryRegistryEditor::BeginDestroy()
{
    UObject::BeginDestroy();

    IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();
    
    AssetRegistry.OnFilesLoaded().Remove(OnAssetRegistryFilesLoadedHandle);
    AssetRegistry.OnAssetAdded().Remove(OnAssetAddedHandle);
    AssetRegistry.OnAssetRemoved().Remove(OnAssetRemovedHandle);
    AssetRegistry.OnAssetUpdated().Remove(OnAssetUpdatedHandle);
    AssetRegistry.OnAssetRenamed().Remove(OnAssetRenamedHandle);
    FWorldDelegates::OnStartGameInstance.Remove(OnStartGameHandle);
}


void UYarnLibraryRegistryEditor::FindFunctionsAndCommands()
{
    YS_LOG_FUNCSIG
    YS_LOG("Project content dir: %s", *FPaths::ProjectContentDir());

    // Import function references for all UYarnFunctionLibrary blueprints
    const TArray<UBlueprint*> FunctionLibraries = FYarnAssetHelpers::FindBlueprintAssetsOfClass<UYarnFunctionLibrary>();
    for (UBlueprint* FunctionLibrary : FunctionLibraries)
    {
        {
            YS_LOG_FUNC("Found function library: %s (%s) %s", *FunctionLibrary->GetFullName(),
                *FunctionLibrary->GetPackage()->GetName(), *FunctionLibrary->GetPathName());

            ImportFunctions(FunctionLibrary);
        }
    }

    // Import command references for all UYarnCommandLibrary blueprints
    const TArray<UBlueprint*> CommandLibraries = FYarnAssetHelpers::FindBlueprintAssetsOfClass<UYarnCommandLibrary>();
    for (UBlueprint* CommandLibrary : CommandLibraries)
    {
        {
            YS_LOG_FUNC("Found command library: %s (%s) %s", *CommandLibrary->GetFullName(),
                *CommandLibrary->GetPackage()->GetName(), *CommandLibrary->GetPathName());

            ImportCommands(CommandLibrary);
        }
    }

    // Finally, save the YSLS file
    YSLSData.Save();
}


void UYarnLibraryRegistryEditor::ExtractFunctionDataFromBlueprintGraph(UBlueprint* YarnFunctionLibrary, UEdGraph* Func, FYarnBlueprintLibFunction& FuncDetails, FYarnBlueprintLibFunctionMeta& FuncMeta, bool bExpectDialogueRunnerParam)
{
    YS_LOG("Function graph: '%s'", *Func->GetName())
    FuncDetails.Library = YarnFunctionLibrary;
    FuncDetails.Name = Func->GetFName();

    // Here is the graph of a simple example function that takes a bool, float and string and returns a string (the original string if the bool is true, otherwise the float as a string):
    //
    // Found asset: Blueprint /Game/AnotherYarnFuncLib.AnotherYarnFuncLib (/Game/AnotherYarnFuncLib) /Game/AnotherYarnFuncLib.AnotherYarnFuncLib
    // Function graph: DoStuff
    // Node: K2Node_FunctionEntry_0 (Class: K2Node_FunctionEntry, Title: Do Stuff)
    // Node is a function entry node with 4 pins
    // Pin: then (Direction: 1, PinType: exec)
    // Pin: FirstInputParam (Direction: 1, PinType: bool)
    // Pin: SecondInputParam (Direction: 1, PinType: float)
    // Pin: ThirdInputParam (Direction: 1, PinType: string)
    // Node: K2Node_IfThenElse_0 (Class: K2Node_IfThenElse, Title: Branch)
    // Pin: execute (Direction: 0, PinType: exec)
    // Pin: Condition (Direction: 0, PinType: bool)
    // Pin: then (Direction: 1, PinType: exec)
    // Pin: else (Direction: 1, PinType: exec)
    // Node: K2Node_FunctionResult_0 (Class: K2Node_FunctionResult, Title: Return Node)
    // Node is a function result node with 2 pins
    // Pin: execute (Direction: 0, PinType: exec)
    // Pin: OutputParam (Direction: 0, PinType: string)
    // Node: K2Node_CallFunction_0 (Class: K2Node_CallFunction, Title: ToString (float))
    // Pin: self (Direction: 0, PinType: object)
    // Pin: InFloat (Direction: 0, PinType: float)
    // Pin: ReturnValue (Direction: 1, PinType: string)
    // Node: K2Node_FunctionResult_1 (Class: K2Node_FunctionResult, Title: Return Node)
    // Node is a function result node with 2 pins
    // Pin: execute (Direction: 0, PinType: exec)
    // Pin: OutputParam (Direction: 0, PinType: string)


    // Find function entry and return nodes, ensure they have a valid number of pins, etc
    for (auto Node : Func->Nodes)
    {
        YS_LOG("Node: '%s' (Class: '%s', Title: '%s')", *Node->GetName(), *Node->GetClass()->GetName(), *Node->GetNodeTitle(ENodeTitleType::ListView).ToString())
        if (Node->IsA(UK2Node_FunctionEntry::StaticClass()))
        {
            auto EntryNode = CastChecked<UK2Node_FunctionEntry>(Node);
            if (EntryNode->GetFunctionFlags() & FUNC_BlueprintPure)
            {
                FuncMeta.bIsPure = true;
                // YS_LOG("BLUEPRINT PURE")
            }
            if (EntryNode->GetFunctionFlags() & FUNC_Public)
            {
                FuncMeta.bIsPublic = true;
                // YS_LOG("PUBLIC FUNCTION")
            }
            if (EntryNode->GetFunctionFlags() & FUNC_Const)
            {
                FuncMeta.bIsConst = true;
                // YS_LOG("CONST FUNCTION")
            }

            YS_LOG("Node is a function entry node with %d pins", Node->Pins.Num())
            for (auto Pin : Node->Pins)
            {
                auto& Category = Pin->PinType.PinCategory;
                auto& SubCategory = Pin->PinType.PinSubCategory;
                YS_LOG("PinName: '%s', Category: '%s', SubCategory: '%s'", *Pin->PinName.ToString(), *Category.ToString(), *SubCategory.ToString())
                if (bExpectDialogueRunnerParam && Pin->PinName == FName(TEXT("DialogueRunner")) && Category == FName(TEXT("object")))
                {
                    FuncMeta.bHasDialogueRunnerRefParam = true;
                }
                else if (Category == TEXT("bool"))
                {
                    FYarnBlueprintParam Param;
                    Param.Name = Pin->PinName;
                    bool Value = Pin->GetDefaultAsString() == "true";
                    Param.Value = Yarn::FValue(Value);
                    FuncDetails.InParams.Add(Param);
                }
                else if (Category == TEXT("float") || (Category == TEXT("real") && (SubCategory == TEXT("float") || SubCategory == TEXT("double"))))
                {
                    FYarnBlueprintParam Param;
                    Param.Name = Pin->PinName;
                    float Value = 0;
                    FDefaultValueHelper::ParseFloat(Pin->GetDefaultAsString(), Value);
                    Param.Value = Yarn::FValue(Value);
                    FuncDetails.InParams.Add(Param);
                }
                else if (Category == TEXT("string"))
                {
                    FYarnBlueprintParam Param;
                    Param.Name = Pin->PinName;
                    Param.Value = Yarn::FValue(TCHAR_TO_UTF8(*Pin->GetDefaultAsString()));
                    FuncDetails.InParams.Add(Param);
                }
                else if (Category != TEXT("exec"))
                {
                    YS_WARN("Invalid Yarn function input pin type: '%s' ('%s').  Must be bool, float or string.", *Pin->PinType.PinCategory.ToString(), *Pin->PinType.PinSubCategory.ToString())
                    FuncMeta.InvalidParams.Add(Pin->PinName.ToString());
                }
            }
            YS_LOG("Added %d parameters to function details", FuncDetails.InParams.Num())
        }
        else if (Node->IsA(UK2Node_FunctionResult::StaticClass()))
        {
            YS_LOG("Node is a function result node with %d pins", Node->Pins.Num())
            for (auto Pin : Node->Pins)
            {
                auto& Category = Pin->PinType.PinCategory;
                auto& SubCategory = Pin->PinType.PinSubCategory;
                if (Category == TEXT("bool"))
                {
                    if (FuncDetails.OutParam.IsSet() && (FuncDetails.OutParam->Value.GetType() != Yarn::FValue::EValueType::Bool || FuncDetails.OutParam->Name != Pin->PinName))
                    {
                        // YS_WARN("Function %s has multiple return values or types.  Yarn functions may only have one return value.", *FuncDetails.Name.ToString())
                        FuncMeta.bHasMultipleOutParams = true;
                    }
                    FYarnBlueprintParam Param;
                    Param.Name = Pin->PinName;
                    bool Value = Pin->GetDefaultAsString() == "true";
                    Param.Value = Yarn::FValue(Value);
                    FuncDetails.OutParam = Param;
                }
                // Recent UE5 builds use "real" instead of "float" for float pins and force the subcategory for return values to be "double"
                else if (Category == TEXT("float") || (Category == TEXT("real") && (SubCategory == TEXT("float") || SubCategory == TEXT("double"))))
                {
                    if (FuncDetails.OutParam.IsSet() && (FuncDetails.OutParam->Value.GetType() != Yarn::FValue::EValueType::Number || FuncDetails.OutParam->Name != Pin->PinName))
                    {
                        // YS_WARN("Function %s has multiple return values or types.  Only one is supported.", *FuncDetails.Name.ToString())
                        FuncMeta.bHasMultipleOutParams = true;
                    }
                    FYarnBlueprintParam Param;
                    Param.Name = Pin->PinName;
                    float Value = 0;
                    FDefaultValueHelper::ParseFloat(Pin->GetDefaultAsString(), Value);
                    Param.Value = Yarn::FValue(Value);
                    FuncDetails.OutParam = Param;
                }
                else if (Category == TEXT("string"))
                {
                    if (FuncDetails.OutParam.IsSet() && (FuncDetails.OutParam->Value.GetType() != Yarn::FValue::EValueType::String || FuncDetails.OutParam->Name != Pin->PinName))
                    {
                        // YS_WARN("Function %s has multiple return values or types.  Only one is supported.", *FuncDetails.Name.ToString())
                        FuncMeta.bHasMultipleOutParams = true;
                    }
                    FYarnBlueprintParam Param;
                    Param.Name = Pin->PinName;
                    Param.Value = Yarn::FValue(TCHAR_TO_UTF8(*Pin->GetDefaultAsString()));
                    FuncDetails.OutParam = Param;
                }
                else if (Category != TEXT("exec"))
                {
                    YS_WARN("Invalid Yarn function result pin type: '%s' ('%s').  Must be bool, float or string.", *Pin->PinType.PinCategory.ToString(), *Pin->PinType.PinSubCategory.ToString())
                    FuncMeta.InvalidParams.Add(Pin->PinName.ToString());
                }
            }
        }
        else if (Node->IsA(UK2Node_FunctionTerminator::StaticClass()))
        {
            // YS_LOG("Node is a function terminator node with %d pins", Node->Pins.Num())
        }
        // for (auto Pin : Node->Pins)
        // {
        //     YS_LOG("Pin: %s (Direction: %d, PinType: %s, DefaultValue: %s)", *Pin->GetName(), (int)Pin->Direction, *Pin->PinType.PinCategory.ToString(), *Pin->GetDefaultAsString())
        // }
    }
}


void UYarnLibraryRegistryEditor::AddToYSLSData(FYarnBlueprintLibFunction FuncDetails)
{
    // convert to .ysls
    
    const bool bIsCommand = !FuncDetails.OutParam.IsSet();
    
    FYSLSAction Action;
    Action.YarnName = FuncDetails.Name.ToString();
    Action.DefinitionName = FuncDetails.Name.ToString();
    Action.DefinitionName = FuncDetails.Name.ToString();
    Action.FileName = FuncDetails.Library->GetPathName();
    
    if (bIsCommand)
    {
        Action.Signature = FuncDetails.Name.ToString();
        for (auto Param : FuncDetails.InParams)
        {
            FYSLSParameter Parameter;
            Parameter.Name = Param.Name.ToString();
            Parameter.Type = LexToString(Param.Value.GetType());
            // TODO: add default value support by capturing the default value when inspecting blueprint function
            // Parameter.DefaultValue = Param.Value;
            Action.Parameters.Add(Parameter);
            Action.Signature += " " + Parameter.Name;
        }
        YSLSData.Commands.Add(Action);
    }
    else
    {
        Action.ReturnType = LexToString(FuncDetails.OutParam->Value.GetType());
        Action.Signature = Action.ReturnType + "(";
        for (auto Param : FuncDetails.InParams)
        {
            FYSLSParameter Parameter;
            Parameter.Name = Param.Name.ToString();
            Parameter.Type = LexToString(Param.Value.GetType());
            Action.Parameters.Add(Parameter);
            Action.Signature += Parameter.Name + ", ";
        }
        if (Action.Signature.EndsWith(", "))
        {
            Action.Signature.LeftInline(Action.Signature.Len() - 2);
        }
        Action.Signature += ")";
        YSLSData.Functions.Add(Action);
    }
}

void UYarnLibraryRegistryEditor::ImportFunctions(UBlueprint* YarnFunctionLibrary)
{
    if (!IsValid(YarnFunctionLibrary))
    {
        YS_WARN_FUNC("Tried to import an invalid function library")
        return;
    }

    // Store a reference to the Blueprint library
    LibFunctions.Add(YarnFunctionLibrary, {});

    for (UEdGraph* Func : YarnFunctionLibrary->FunctionGraphs)
    {
        FYarnBlueprintLibFunction FuncDetails;
        FYarnBlueprintLibFunctionMeta FuncMeta;

        ExtractFunctionDataFromBlueprintGraph(YarnFunctionLibrary, Func, FuncDetails, FuncMeta);

        // Ignore private functions
        // if (!FuncMeta.bIsPublic)
        // {
        //     YS_LOG("Ignoring private Blueprint function '%s'.", *FuncDetails.Name.ToString())
        // }
        // else
        {
            // Test for valid function
            bool bIsValid = true;
            if (FuncMeta.bHasMultipleOutParams || !FuncDetails.OutParam.IsSet())
            {
                bIsValid = false;
                YS_WARN("Function '%s' must have exactly one return parameter named '%s'.", *FuncDetails.Name.ToString(), *GYSFunctionReturnParamName.ToString())
            }
            if (FuncDetails.OutParam.IsSet() && FuncDetails.OutParam->Name != GYSFunctionReturnParamName)
            {
                bIsValid = false;
                YS_WARN("Function '%s' must have exactly one return parameter named '%s'.", *FuncDetails.Name.ToString(), *GYSFunctionReturnParamName.ToString())
            }
            if (FuncMeta.InvalidParams.Num() > 0)
            {
                bIsValid = false;
                YS_WARN("Function '%s' has invalid parameter types. Yarn functions only support boolean, float and string.", *FuncDetails.Name.ToString())
            }
            // Check name is unique
            if (AllFunctions.Contains(FuncDetails.Name) && AllFunctions[FuncDetails.Name].Library != FuncDetails.Library)
            {
                bIsValid = false;
                YS_WARN("Function '%s' already exists in another Blueprint.  Yarn function names must be unique.", *FuncDetails.Name.ToString())
            }
            // Check name is valid
            const FRegexPattern Pattern{TEXT("^[\\w_][\\w\\d_]*$")};
            FRegexMatcher FunctionNameMatcher(Pattern, FuncDetails.Name.ToString());
            if (!FunctionNameMatcher.FindNext())
            {
                bIsValid = false;
                YS_WARN("Function '%s' has an invalid name.  Yarn function names must be valid C++ function names.", *FuncDetails.Name.ToString())
            }

            // TODO: check function has no async nodes

            // Add to function sets
            if (bIsValid)
            {
                YS_LOG("Adding function '%s' to available YarnSpinner functions.", *FuncDetails.Name.ToString())
                LibFunctions.FindOrAdd(YarnFunctionLibrary).Names.Add(FuncDetails.Name);
                AllFunctions.Add(FuncDetails.Name, FuncDetails);

                AddToYSLSData(FuncDetails);
            }
        }
    }
}


void UYarnLibraryRegistryEditor::ImportCommands(UBlueprint* YarnCommandLibrary)
{
    if (!IsValid(YarnCommandLibrary))
    {
        YS_WARN_FUNC("Tried to import an invalid command library")
        return;
    }

    // Store a reference to the Blueprint library
    LibCommands.FindOrAdd(YarnCommandLibrary);

    for (UEdGraph* Func : YarnCommandLibrary->EventGraphs)
    {
        FYarnBlueprintLibFunction FuncDetails;
        FYarnBlueprintLibFunctionMeta FuncMeta;

        ExtractFunctionDataFromBlueprintGraph(YarnCommandLibrary, Func, FuncDetails, FuncMeta, true);

        // Test for valid event function
        bool bIsValid = true;
        if (!FuncMeta.bHasDialogueRunnerRefParam)
        {
            bIsValid = false;
            // Only show the warning for non-builtin events.
            const FRegexPattern BuiltinEventPattern(TEXT("^[a-zA-Z]+_[A-Z,0-9]+$"));
            FRegexMatcher BuiltinEventMatcher(BuiltinEventPattern, FuncDetails.Name.ToString());
            if (!BuiltinEventMatcher.FindNext())
            {
                YS_WARN("Event '%s' requires a parameter called 'DialogueRunner' which is a reference to a DialogueRunner object in order to be usable as a Yarn command.", *FuncDetails.Name.ToString())
            }
        }
        if (FuncDetails.OutParam.IsSet())
        {
            bIsValid = false;
            YS_WARN("Event '%s' has a return value.  Yarn commands must not return values.", *FuncDetails.Name.ToString())
        }
        if (FuncMeta.InvalidParams.Num() > 0)
        {
            bIsValid = false;
            YS_WARN("Event '%s' has invalid parameter types. Yarn commands only support boolean, float and string.", *FuncDetails.Name.ToString())
        }
        // Check name is unique
        if (AllFunctions.Contains(FuncDetails.Name) && AllFunctions[FuncDetails.Name].Library != FuncDetails.Library)
        {
            bIsValid = false;
            YS_WARN("Event '%s' already exists in another Blueprint.  Yarn command names must be unique.", *FuncDetails.Name.ToString())
        }
        // Check name is valid
        const FRegexPattern Pattern{TEXT("^[\\w_][\\w\\d_]*$")};
        FRegexMatcher FunctionNameMatcher(Pattern, FuncDetails.Name.ToString());
        if (!FunctionNameMatcher.FindNext())
        {
            bIsValid = false;
            YS_WARN("Event '%s' has an invalid name.  Yarn function names must be valid C++ function names.", *FuncDetails.Name.ToString())
        }

        // TODO: check function calls Continue() at some point

        // Add to function sets
        if (bIsValid)
        {
            YS_LOG("Adding command '%s' to available YarnSpinner commands.", *FuncDetails.Name.ToString())
            LibCommands.FindOrAdd(YarnCommandLibrary).Names.Add(FuncDetails.Name);
            AllCommands.Add(FuncDetails.Name, FuncDetails);

            AddToYSLSData(FuncDetails);
        }
    }
}


void UYarnLibraryRegistryEditor::UpdateFunctions(UBlueprint* YarnFunctionLibrary)
{
    RemoveFunctions(YarnFunctionLibrary);
    ImportFunctions(YarnFunctionLibrary);
}


void UYarnLibraryRegistryEditor::UpdateCommands(UBlueprint* YarnCommandLibrary)
{
    RemoveCommands(YarnCommandLibrary);
    ImportCommands(YarnCommandLibrary);
}


void UYarnLibraryRegistryEditor::RemoveFunctions(UBlueprint* YarnFunctionLibrary)
{
    FYarnLibFuncNames RemovedFunctions;
    if (LibFunctions.RemoveAndCopyValue(YarnFunctionLibrary, RemovedFunctions))
    {
        // If the map had an entry for this library,
        // ensure we remove all of the relative function names from the AllFunctions mapping
        
        for(TMap<FName, FYarnBlueprintLibFunction>::TIterator FuncIter = AllFunctions.CreateIterator(); FuncIter; ++FuncIter)
        {
            if (RemovedFunctions.Names.Contains(FuncIter->Key) &&
                FuncIter->Value.Library == YarnFunctionLibrary)
            {
                FuncIter.RemoveCurrent();
            }
        }

        // And remove all of the functions from the YSLS data

        FString LibraryPathName = YarnFunctionLibrary->GetPathName();
        YSLSData.Functions.RemoveAll([LibraryPathName = MoveTemp(LibraryPathName), &RemovedFunctions](const FYSLSAction& Action)
        {
            return Action.FileName == LibraryPathName &&
                RemovedFunctions.Names.Contains(Action.YarnName) &&
                RemovedFunctions.Names.Contains(Action.DefinitionName);
        });
    }
}


void UYarnLibraryRegistryEditor::RemoveCommands(UBlueprint* YarnCommandLibrary)
{
    FYarnLibFuncNames RemovedCommands;
    if (LibCommands.RemoveAndCopyValue(YarnCommandLibrary, RemovedCommands))
    {
        // If the map had an entry for this library,
        // ensure we remove all of the relative command names from the AllCommands mapping
        
        for(TMap<FName, FYarnBlueprintLibFunction>::TIterator FuncIter = AllCommands.CreateIterator(); FuncIter; ++FuncIter)
        {
            if (RemovedCommands.Names.Contains(FuncIter->Key) &&
                FuncIter->Value.Library == YarnCommandLibrary)
            {
                FuncIter.RemoveCurrent();
            }
        }

        // And remove all of the commands from the YSLS data

        FString LibraryPathName = YarnCommandLibrary->GetPathName();
        YSLSData.Commands.RemoveAll([LibraryPathName = MoveTemp(LibraryPathName), &RemovedCommands](const FYSLSAction& Action)
        {
            return Action.FileName == LibraryPathName &&
                RemovedCommands.Names.Contains(Action.YarnName) &&
                RemovedCommands.Names.Contains(Action.DefinitionName);
        });
    }
}


void UYarnLibraryRegistryEditor::OnAssetRegistryFilesLoaded()
{
    YS_LOG_FUNCSIG

    FindFunctionsAndCommands();
}


void UYarnLibraryRegistryEditor::OnAssetAdded(const FAssetData& AssetData)
{
    // Ignore this until the registry has finished loading the first time.
    if (FAssetRegistryModule::GetRegistry().IsLoadingAssets())
    {
        return;
    }

    YS_LOG_FUNCSIG

    UBlueprint* BlueprintAsset = Cast<UBlueprint>(AssetData.GetAsset());
    const bool bIsFunctionLib = FYarnAssetHelpers::IsBlueprintAssetOfClass<UYarnFunctionLibrary>(BlueprintAsset);
    const bool bIsCommandLib = FYarnAssetHelpers::IsBlueprintAssetOfClass<UYarnCommandLibrary>(BlueprintAsset);

    if (bIsFunctionLib)
    {
        ImportFunctions(BlueprintAsset);
    }
    if (bIsCommandLib)
    {
        ImportCommands(BlueprintAsset);
    }
    if (bIsFunctionLib || bIsCommandLib)
    {
        YSLSData.Save();
    }
}


void UYarnLibraryRegistryEditor::OnAssetRemoved(const FAssetData& AssetData)
{
    UBlueprint* BlueprintAsset = Cast<UBlueprint>(AssetData.GetAsset());
    const bool bIsFunctionLib = FYarnAssetHelpers::IsBlueprintAssetOfClass<UYarnFunctionLibrary>(BlueprintAsset);
    const bool bIsCommandLib = FYarnAssetHelpers::IsBlueprintAssetOfClass<UYarnCommandLibrary>(BlueprintAsset);

    if (bIsFunctionLib)
    {
        RemoveFunctions(BlueprintAsset);
    }
    if (bIsCommandLib)
    {
        RemoveCommands(BlueprintAsset);
    }
    if (bIsFunctionLib || bIsCommandLib)
    {
        YSLSData.Save();
    }
}


void UYarnLibraryRegistryEditor::OnAssetUpdated(const FAssetData& AssetData)
{
    UBlueprint* BlueprintAsset = Cast<UBlueprint>(AssetData.GetAsset());
    const bool bIsFunctionLib = FYarnAssetHelpers::IsBlueprintAssetOfClass<UYarnFunctionLibrary>(BlueprintAsset);
    const bool bIsCommandLib = FYarnAssetHelpers::IsBlueprintAssetOfClass<UYarnCommandLibrary>(BlueprintAsset);

    if (bIsFunctionLib)
    {
        UpdateFunctions(BlueprintAsset);
    }
    if (bIsCommandLib)
    {
        UpdateCommands(BlueprintAsset);
    }
    if (bIsFunctionLib || bIsCommandLib)
    {
        YSLSData.Save();
    }
}


void UYarnLibraryRegistryEditor::OnAssetRenamed(const FAssetData& AssetData, const FString& String)
{
    UBlueprint* BlueprintAsset = Cast<UBlueprint>(AssetData.GetAsset());
    const bool bIsFunctionLib = FYarnAssetHelpers::IsBlueprintAssetOfClass<UYarnFunctionLibrary>(BlueprintAsset);
    const bool bIsCommandLib = FYarnAssetHelpers::IsBlueprintAssetOfClass<UYarnCommandLibrary>(BlueprintAsset);

    if (bIsFunctionLib)
    {
        UpdateFunctions(BlueprintAsset);
    }
    if (bIsCommandLib)
    {
        UpdateCommands(BlueprintAsset);
    }
    if (bIsFunctionLib || bIsCommandLib)
    {
        YSLSData.Save();
    }
}


void UYarnLibraryRegistryEditor::OnStartGameInstance(UGameInstance* GameInstance)
{
    FindFunctionsAndCommands();
}
