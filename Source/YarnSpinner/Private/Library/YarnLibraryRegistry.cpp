﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "Library/YarnLibraryRegistry.h"

#include "DialogueRunner.h"
#include "Library/YarnCommandLibrary.h"
#include "Library/YarnFunctionLibrary.h"
#include "Library/YarnSpinnerLibraryData.h"
#include "Misc/YarnAssetHelpers.h"
#include "Misc/YSLogging.h"


UYarnLibraryRegistry::UYarnLibraryRegistry()
{
    YS_LOG_FUNCSIG

    FWorldDelegates::OnStartGameInstance.AddUObject(this, &UYarnLibraryRegistry::OnStartGameInstance);

    LoadStdFunctions();
    LoadStdCommands();
}


void UYarnLibraryRegistry::BeginDestroy()
{
    UObject::BeginDestroy();
}


bool UYarnLibraryRegistry::HasFunction(const FName& Name) const
{
    if (StdFunctions.Contains(Name))
        return true;

    if (!AllFunctions.Contains(Name))
    {
        FString S = TEXT("Could not find function '") + Name.ToString() + TEXT("'.  Known functions: ");
        for (auto Func : AllFunctions)
        {
            S += TEXT("'") + Func.Key.ToString() + TEXT("', ");
        }
        YS_WARN("%s", *S)
    }

    return AllFunctions.Contains(Name);
}


bool UYarnLibraryRegistry::HasCommand(const FName& Name) const
{
    if (StdCommands.Contains(Name))
        return true;

    if (!AllCommands.Contains(Name))
    {
        FString S = TEXT("Could not find command '") + Name.ToString() + TEXT("'.  Known commands: ");
        for (auto Cmd : AllCommands)
        {
            S += TEXT("'") + Cmd.Key.ToString() + TEXT("', ");
        }
        YS_WARN("%s", *S)
    }

    return AllCommands.Contains(Name);
}


int32 UYarnLibraryRegistry::GetExpectedFunctionParamCount(const FName& Name) const
{
    if (StdFunctions.Contains(Name))
        return StdFunctions[Name].ExpectedParamCount;

    if (!AllFunctions.Contains(Name))
    {
        YS_WARN("Could not find function '%s' in registry.", *Name.ToString())
        return 0;
    }

    return AllFunctions[Name].InParams.Num();
}


Yarn::FValue UYarnLibraryRegistry::CallFunction(const FName& Name, TArray<Yarn::FValue> Parameters) const
{
    if (StdFunctions.Contains(Name))
    {
        return StdFunctions[Name].Function(Parameters);
    }

    if (!AllFunctions.Contains(Name))
    {
        YS_WARN("Attempted to call non-existent function '%s'", *Name.ToString())
        return Yarn::FValue();
    }

    auto FuncDetail = AllFunctions[Name];

    if (FuncDetail.InParams.Num() != Parameters.Num())
    {
        YS_WARN("Attempted to call function '%s' with incorrect number of arguments (expected %d).", *Name.ToString(), FuncDetail.InParams.Num())
        return Yarn::FValue();
    }

    auto Lib = UYarnFunctionLibrary::FromBlueprint(FuncDetail.Library);

    if (!Lib)
    {
        YS_WARN("Couldn't create library for Blueprint containing function '%s'", *Name.ToString())
        return Yarn::FValue();
    }

    TArray<FYarnBlueprintParam> InParams;
    for (auto Param : FuncDetail.InParams)
    {
        FYarnBlueprintParam InParam = Param;
        InParam.Value = Parameters[InParams.Num()];
        InParams.Add(InParam);
    }

    TOptional<FYarnBlueprintParam> OutParam = FuncDetail.OutParam;

    auto Result = Lib->CallFunction(Name, InParams, OutParam);
    if (Result.IsSet())
    {
        return Result.GetValue();
    }
    YS_WARN("Function '%s' returned an invalid value.", *Name.ToString())
    return Yarn::FValue();
}


void UYarnLibraryRegistry::CallCommand(const FName& Name, TSoftObjectPtr<ADialogueRunner> DialogueRunner, TArray<FString> UnprocessedParamStrings) const
{
    if (StdCommands.Contains(Name))
    {
        return StdCommands[Name].Command(DialogueRunner, UnprocessedParamStrings);
    }

    if (!AllCommands.Contains(Name))
    {
        YS_WARN("Attempted to call non-existent command '%s'", *Name.ToString())
        return;
    }

    FYarnBlueprintLibFunction CmdDetail = AllCommands[Name];

    if (CmdDetail.InParams.Num() != UnprocessedParamStrings.Num())
    {
        YS_WARN("Attempted to call command '%s' with incorrect number of arguments (expected %d).", *Name.ToString(), CmdDetail.InParams.Num())
        return;
    }

    UYarnCommandLibrary* Lib = UYarnCommandLibrary::FromBlueprint(CmdDetail.Library);

    if (!Lib)
    {
        YS_WARN("Couldn't create library for Blueprint containing command '%s'", *Name.ToString())
        return;
    }

    // Convert strings to expected params
    TArray<FYarnBlueprintParam> InParams;
    for (int I = 0; I < CmdDetail.InParams.Num(); I++)
    {
        FYarnBlueprintParam InParam = CmdDetail.InParams[I];
        if (UnprocessedParamStrings.Num() > I)
        {
            const FString& ParamString = UnprocessedParamStrings[I];
            switch (InParam.Value.GetType())
            {
            case Yarn::FValue::EValueType::Number:
                InParam.Value = FCString::Atof(*ParamString);
            case Yarn::FValue::EValueType::Bool:
                InParam.Value = ParamString.ToLower() == "true";
            case Yarn::FValue::EValueType::String:
            default:
                InParam.Value = ParamString;
            }
        }
        InParams.Add(InParam);
    }

    Lib->CallCommand(Name, DialogueRunner, InParams);

    YS_LOG("Command '%s' called.", *Name.ToString())
}


UBlueprint* UYarnLibraryRegistry::GetYarnFunctionLibraryBlueprint(const FAssetData& AssetData)
{
    UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset());

    if (!BP)
    {
        // YS_LOG_FUNC("Could not cast asset to UBlueprint")
        return nullptr;
    }
    if (!BP->ParentClass->IsChildOf<UYarnFunctionLibrary>())
    {
        // YS_LOG_FUNC("Blueprint is not a child of UYarnFunctionLibrary")
        return nullptr;
    }
    if (!BP->GeneratedClass)
    {
        // YS_LOG_FUNC("Blueprint has no generated class")
        return nullptr;
    }

    return BP;
}


UBlueprint* UYarnLibraryRegistry::GetYarnCommandLibraryBlueprint(const FAssetData& AssetData)
{
    UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset());

    if (!BP)
    {
        // YS_LOG_FUNC("Could not cast asset to UBlueprint")
        return nullptr;
    }
    if (!BP->ParentClass->IsChildOf<UYarnCommandLibrary>())
    {
        // YS_LOG_FUNC("Blueprint is not a child of UYarnCommandLibrary")
        return nullptr;
    }
    if (!BP->GeneratedClass)
    {
        // YS_LOG_FUNC("Blueprint has no generated class")
        return nullptr;
    }

    return BP;
}


void UYarnLibraryRegistry::FindFunctionsAndCommands()
{
    YS_LOG_FUNCSIG
    YS_LOG("Project content dir: %s", *FPaths::ProjectContentDir());

    FString YSLSFileData;
    FFileHelper::LoadFileToString(YSLSFileData, *FYarnAssetHelpers::YSLSFilePath());

    if (auto YSLSData = FYarnSpinnerLibraryData::FromJsonString(YSLSFileData))
    {
        for (auto Func : YSLSData->Functions)
        {
            AddFunction(Func);
        }
        for (auto Cmd : YSLSData->Commands)
        {
            AddCommand(Cmd);
        }
    }
}


void UYarnLibraryRegistry::AddFunction(const FYSLSAction& Func)
{
    // Find the blueprint
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
    auto Asset = FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(Func.FileName));
#else
    auto Asset = FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FName(Func.FileName));
#endif

    YS_LOG_FUNC("Found asset %s for function %s (%s)", *Asset.GetFullName(), *Func.DefinitionName, *Func.FileName)
    Asset.PrintAssetData();

    auto BP = GetYarnFunctionLibraryBlueprint(Asset);
    if (!BP)
    {
        YS_WARN("Could not load Blueprint for Yarn function '%s'", *Func.DefinitionName)
        return;
    }

    FunctionLibraries.Add(BP);

    FYarnBlueprintLibFunction FuncDetail{BP, FName(Func.DefinitionName)};

    for (auto InParam : Func.Parameters)
    {
        FYarnBlueprintParam Param{FName(InParam.Name)};
        if (InParam.Type == "boolean")
        {
            Param.Value = Yarn::FValue(InParam.DefaultValue == "true");
        }
        else if (InParam.Type == "number")
        {
            Param.Value = Yarn::FValue(FCString::Atof(*InParam.DefaultValue));
        }
        else
        {
            Param.Value = Yarn::FValue(InParam.DefaultValue);
        }

        FuncDetail.InParams.Add(Param);
    }

    FYarnBlueprintParam Param{GYSFunctionReturnParamName};
    if (Func.ReturnType == "boolean")
    {
        Param.Value = Yarn::FValue(false);
    }
    else if (Func.ReturnType == "number")
    {
        Param.Value = Yarn::FValue(0.0f);
    }
    else
    {
        Param.Value = Yarn::FValue("");
    }
    FuncDetail.OutParam = Param;

    AllFunctions.Add(FName(Func.DefinitionName), FuncDetail);
}


void UYarnLibraryRegistry::AddCommand(const FYSLSAction& Cmd)
{
    YS_LOG_FUNCSIG
    // Find the blueprint
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
    auto Asset = FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(Cmd.FileName));
#else
    auto Asset = FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FName(Cmd.FileName));
#endif

    YS_LOG_FUNC("Found asset %s for function %s (%s)", *Asset.GetFullName(), *Cmd.DefinitionName, *Cmd.FileName)
    Asset.PrintAssetData();

    auto BP = GetYarnCommandLibraryBlueprint(Asset);
    if (!BP)
    {
        YS_WARN("Could not load Blueprint for Yarn function '%s'", *Cmd.DefinitionName)
        return;
    }

    CommandLibraries.Add(BP);

    FYarnBlueprintLibFunction CmdDetail{BP, FName(Cmd.DefinitionName)};

    for (auto InParam : Cmd.Parameters)
    {
        FYarnBlueprintParam Param{FName(InParam.Name)};
        if (InParam.Type == "boolean")
        {
            Param.Value = Yarn::FValue(InParam.DefaultValue == "true");
        }
        else if (InParam.Type == "number")
        {
            Param.Value = Yarn::FValue(FCString::Atof(*InParam.DefaultValue));
        }
        else
        {
            Param.Value = InParam.DefaultValue;
        }

        CmdDetail.InParams.Add(Param);
    }

    FYarnBlueprintParam Param{GYSFunctionReturnParamName};
    if (Cmd.ReturnType == "boolean")
    {
        Param.Value = Yarn::FValue(false);
    }
    else if (Cmd.ReturnType == "number")
    {
        Param.Value = Yarn::FValue(0.0f);
    }
    else
    {
        Param.Value = Yarn::FValue("");
    }
    CmdDetail.OutParam = Param;

    AllCommands.Add(FName(Cmd.DefinitionName), CmdDetail);
}


void UYarnLibraryRegistry::OnStartGameInstance(UGameInstance* GameInstance)
{
    FindFunctionsAndCommands();
}


void UYarnLibraryRegistry::AddStdFunction(const FYarnStdLibFunction& Func)
{
    StdFunctions.Add(Func.Name, Func);
}


void UYarnLibraryRegistry::AddStdCommand(const FYarnStdLibCommand& Command)
{
    StdCommands.Add(Command.Name, Command);
}


void UYarnLibraryRegistry::LoadStdFunctions()
{
    AddStdFunction({
        TEXT("Number.EqualTo"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.EqualTo called with incorrect number of parameters (expected 2).")
                return Yarn::FValue();
            }
            if (Params[0].GetType() != Yarn::FValue::EValueType::Number || Params[1].GetType() != Yarn::FValue::EValueType::Number)
            {
                YS_WARN("Number.EqualTo called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<double>() == Params[1].GetValue<double>());
        }
    });

    AddStdFunction({
        TEXT("Number.NotEqualTo"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.NotEqualTo called with incorrect number of parameters (expected 2).")
                return Yarn::FValue();
            }
            if (Params[0].GetType() != Yarn::FValue::EValueType::Number || Params[1].GetType() != Yarn::FValue::EValueType::Number)
            {
                YS_WARN("Number.NotEqualTo called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<double>() != Params[1].GetValue<double>());
        }
    });

    AddStdFunction({
        TEXT("Number.Add"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.Add called with incorrect number of parameters (expected 2).")
                return Yarn::FValue();
            }
            if (Params[0].GetType() != Yarn::FValue::EValueType::Number || Params[1].GetType() != Yarn::FValue::EValueType::Number)
            {
                YS_WARN("Number.Add called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<double>() + Params[1].GetValue<double>());
        }
    });

    AddStdFunction({
        TEXT("Number.Minus"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.Minus called with incorrect number of parameters (expected 2).")
                return Yarn::FValue();
            }
            if (Params[0].GetType() != Yarn::FValue::EValueType::Number || Params[1].GetType() != Yarn::FValue::EValueType::Number)
            {
                YS_WARN("Number.Minus called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<double>() - Params[1].GetValue<double>());
        }
    });

    AddStdFunction({
        TEXT("Number.Divide"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.Divide called with incorrect number of parameters (expected 2).")
                return Yarn::FValue();
            }
            if (Params[0].GetType() != Yarn::FValue::EValueType::Number || Params[1].GetType() != Yarn::FValue::EValueType::Number)
            {
                YS_WARN("Number.Divide called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<double>() / Params[1].GetValue<double>());
        }
    });

    AddStdFunction({
        TEXT("Number.Multiply"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.Multiply called with incorrect number of parameters (expected 2).")
                return Yarn::FValue();
            }
            if (Params[0].GetType() != Yarn::FValue::EValueType::Number || Params[1].GetType() != Yarn::FValue::EValueType::Number)
            {
                YS_WARN("Number.Multiply called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<double>() * Params[1].GetValue<double>());
        }
    });

    AddStdFunction({
        TEXT("Number.Modulo"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.Modulo called with incorrect number of parameters (expected 2).")
                return Yarn::FValue();
            }
            if (Params[0].GetType() != Yarn::FValue::EValueType::Number || Params[1].GetType() != Yarn::FValue::EValueType::Number)
            {
                YS_WARN("Number.Modulo called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::FValue();
            }
            return Yarn::FValue(FMath::Fmod(Params[0].GetValue<double>(), Params[1].GetValue<double>()));
        }
    });

    AddStdFunction({
        TEXT("Number.UnaryMinus"), 1, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 1)
            {
                YS_WARN("Number.UnaryMinus called with incorrect number of parameters (expected 1).")
                return Yarn::FValue();
            }
            if (Params[0].GetType() != Yarn::FValue::EValueType::Number)
            {
                YS_WARN("Number.UnaryMinus called with incorrect parameter types (expected NUMBER).")
                return Yarn::FValue();
            }
            return Yarn::FValue(-Params[0].GetValue<double>());
        }
    });

    AddStdFunction({
        TEXT("Number.GreaterThan"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.GreaterThan called with incorrect number of parameters (expected 2).")
                return Yarn::FValue();
            }
            if (Params[0].GetType() != Yarn::FValue::EValueType::Number || Params[1].GetType() != Yarn::FValue::EValueType::Number)
            {
                YS_WARN("Number.GreaterThan called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<double>() > Params[1].GetValue<double>());
        }
    });

    AddStdFunction({
        TEXT("Number.GreaterThanOrEqualTo"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.GreaterThanOrEqualTo called with incorrect number of parameters (expected 2).")
                return Yarn::FValue();
            }
            if (Params[0].GetType() != Yarn::FValue::EValueType::Number || Params[1].GetType() != Yarn::FValue::EValueType::Number)
            {
                YS_WARN("Number.GreaterThanOrEqualTo called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<double>() >= Params[1].GetValue<double>());
        }
    });

    AddStdFunction({
        TEXT("Number.LessThan"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.LessThan called with incorrect number of parameters (expected 2).")
                return Yarn::FValue();
            }
            if (Params[0].GetType() != Yarn::FValue::EValueType::Number || Params[1].GetType() != Yarn::FValue::EValueType::Number)
            {
                YS_WARN("Number.LessThan called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<double>() < Params[1].GetValue<double>());
        }
    });

    AddStdFunction({
        TEXT("Number.LessThanOrEqualTo"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.LessThanOrEqualTo called with incorrect number of parameters (expected 2).")
                return Yarn::FValue();
            }
            if (Params[0].GetType() != Yarn::FValue::EValueType::Number || Params[1].GetType() != Yarn::FValue::EValueType::Number)
            {
                YS_WARN("Number.LessThanOrEqualTo called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<double>() <= Params[1].GetValue<double>());
        }
    });

    AddStdFunction({
        TEXT("Bool.EqualTo"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Bool.EqualTo called with incorrect number of parameters (expected 2).")
                return Yarn::FValue();
            }
            if (Params[0].GetType() != Yarn::FValue::EValueType::Bool || Params[1].GetType() != Yarn::FValue::EValueType::Bool)
            {
                YS_WARN("Bool.EqualTo called with incorrect parameter types (expected BOOL, BOOL).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<bool>() == Params[1].GetValue<bool>());
        }
    });

    AddStdFunction({
        TEXT("Bool.NotEqualTo"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Bool.NotEqualTo called with incorrect number of parameters (expected 2).")
                return Yarn::FValue();
            }
            if (Params[0].GetType() != Yarn::FValue::EValueType::Bool || Params[1].GetType() != Yarn::FValue::EValueType::Bool)
            {
                YS_WARN("Bool.NotEqualTo called with incorrect parameter types (expected BOOL, BOOL).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<bool>() != Params[1].GetValue<bool>());
        }
    });

    AddStdFunction({
        TEXT("Bool.And"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2 || Params[0].GetType() != Yarn::FValue::EValueType::Bool || Params[1].GetType() != Yarn::FValue::EValueType::Bool)
            {
                YS_WARN("Bool.And called with incorrect number of parameters (expected 2) or incorrect parameter types (expected BOOL, BOOL).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<bool>() && Params[1].GetValue<bool>());
        }
    });

    AddStdFunction({
        TEXT("Bool.Or"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2 || Params[0].GetType() != Yarn::FValue::EValueType::Bool || Params[1].GetType() != Yarn::FValue::EValueType::Bool)
            {
                YS_WARN("Bool.Or called with incorrect number of parameters (expected 2) or incorrect parameter types (expected BOOL, BOOL).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<bool>() || Params[1].GetValue<bool>());
        }
    });

    AddStdFunction({
        TEXT("Bool.Xor"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2 || Params[0].GetType() != Yarn::FValue::EValueType::Bool || Params[1].GetType() != Yarn::FValue::EValueType::Bool)
            {
                YS_WARN("Bool.Xor called with incorrect number of parameters (expected 2) or incorrect parameter types (expected BOOL, BOOL).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<bool>() != Params[1].GetValue<bool>());
        }
    });

    AddStdFunction({
        TEXT("Bool.Not"), 1, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 1 || Params[0].GetType() != Yarn::FValue::EValueType::Bool)
            {
                YS_WARN("Bool.Not called with incorrect number of parameters (expected 1) or incorrect parameter types (expected BOOL).")
                return Yarn::FValue();
            }
            return Yarn::FValue(!Params[0].GetValue<bool>());
        }
    });

    AddStdFunction({
        TEXT("String.EqualTo"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2 || Params[0].GetType() != Yarn::FValue::EValueType::String || Params[1].GetType() != Yarn::FValue::EValueType::String)
            {
                YS_WARN("String.EqualTo called with incorrect number of parameters (expected 2) or incorrect parameter types (expected STRING, STRING).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<FString>() == Params[1].GetValue<FString>());
        }
    });

    AddStdFunction({
        TEXT("String.NotEqualTo"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2 || Params[0].GetType() != Yarn::FValue::EValueType::String || Params[1].GetType() != Yarn::FValue::EValueType::String)
            {
                YS_WARN("String.NotEqualTo called with incorrect number of parameters (expected 2) or incorrect parameter types (expected STRING, STRING).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<FString>() != Params[1].GetValue<FString>());
        }
    });

    AddStdFunction({
        TEXT("String.Add"), 2, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 2 || Params[0].GetType() != Yarn::FValue::EValueType::String || Params[1].GetType() != Yarn::FValue::EValueType::String)
            {
                YS_WARN("String.Add called with incorrect number of parameters (expected 2) or incorrect parameter types (expected STRING, STRING).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].GetValue<FString>() + Params[1].GetValue<FString>());
        }
    });

    AddStdFunction({
        TEXT("string"), 1, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 1)
            {
                YS_WARN("string called with incorrect number of parameters (expected 1).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].ConvertToString());
        }
    });

    AddStdFunction({
        TEXT("number"), 1, [](TArray<Yarn::FValue> Params) -> Yarn::FValue
        {
            if (Params.Num() != 1)
            {
                YS_WARN("number called with incorrect number of parameters (expected 1).")
                return Yarn::FValue();
            }
            return Yarn::FValue(Params[0].ConvertToNumber());
        }
    });

    // missing:
    // format_invariant
    // ...
}


void UYarnLibraryRegistry::LoadStdCommands()
{
    AddStdCommand({
        TEXT("wait"), 1, [this](TSoftObjectPtr<ADialogueRunner> DialogueRunner, TArray<FString> Params)
        {
            YS_LOG_FUNCSIG
            float WaitTime = 0;
            if (Params.Num() != 1)
            {
                YS_WARN("wait called with incorrect number of parameters (expected 1).")
            }
            else if (!((WaitTime = FCString::Atof(*Params[0]))))
            {
                YS_WARN("wait called with incorrect parameter types (expected NUMBER).")
            }

            DialogueRunner->GetWorld()->GetTimerManager().SetTimer(CommandTimerHandle, [DialogueRunner]()
            {
                YS_LOG_FUNCSIG
                DialogueRunner->ContinueDialogue();
            }, WaitTime, false);
        }
    });
}

