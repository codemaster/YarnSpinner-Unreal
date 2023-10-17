﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "Library/YarnLibraryRegistry.h"

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


Yarn::Value UYarnLibraryRegistry::CallFunction(const FName& Name, TArray<Yarn::Value> Parameters) const
{
    if (StdFunctions.Contains(Name))
    {
        return StdFunctions[Name].Function(Parameters);
    }

    if (!AllFunctions.Contains(Name))
    {
        YS_WARN("Attempted to call non-existent function '%s'", *Name.ToString())
        return Yarn::Value();
    }

    auto FuncDetail = AllFunctions[Name];

    if (FuncDetail.InParams.Num() != Parameters.Num())
    {
        YS_WARN("Attempted to call function '%s' with incorrect number of arguments (expected %d).", *Name.ToString(), FuncDetail.InParams.Num())
        return Yarn::Value();
    }

    auto Lib = UYarnFunctionLibrary::FromBlueprint(FuncDetail.Library);

    if (!Lib)
    {
        YS_WARN("Couldn't create library for Blueprint containing function '%s'", *Name.ToString())
        return Yarn::Value();
    }

    TArray<FYarnBlueprintFuncParam> InParams;
    for (auto Param : FuncDetail.InParams)
    {
        FYarnBlueprintFuncParam InParam = Param;
        InParam.Value = Parameters[InParams.Num()];
        InParams.Add(InParam);
    }

    TOptional<FYarnBlueprintFuncParam> OutParam = FuncDetail.OutParam;

    auto Result = Lib->CallFunction(Name, InParams, OutParam);
    if (Result.IsSet())
    {
        return Result.GetValue();
    }
    YS_WARN("Function '%s' returned an invalid value.", *Name.ToString())
    return Yarn::Value();
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
    auto Asset = FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FName(Func.FileName));

    YS_LOG_FUNC("Found asset %s for function %s (%s)", *Asset.GetFullName(), *Func.DefinitionName, *Func.FileName)
    Asset.PrintAssetData();

    auto BP = GetYarnFunctionLibraryBlueprint(Asset);
    if (!BP)
    {
        YS_WARN("Could not load Blueprint for Yarn function '%s", *Func.DefinitionName)
        return;
    }

    FunctionLibraries.Add(BP);

    FYarnBlueprintLibFunction FuncDetail{BP, FName(Func.DefinitionName)};

    for (auto InParam : Func.Parameters)
    {
        FYarnBlueprintFuncParam Param{FName(InParam.Name)};
        if (InParam.Type == "boolean")
        {
            Param.Value = Yarn::Value(InParam.DefaultValue == "true");
        }
        else if (InParam.Type == "number")
        {
            Param.Value = Yarn::Value(FCString::Atof(*InParam.DefaultValue));
        }
        else
        {
            Param.Value = Yarn::Value(TCHAR_TO_UTF8(*InParam.DefaultValue));
        }

        FuncDetail.InParams.Add(Param);
    }

    FYarnBlueprintFuncParam Param{GYSFunctionReturnParamName};
    if (Func.ReturnType == "boolean")
    {
        Param.Value = Yarn::Value(false);
    }
    else if (Func.ReturnType == "number")
    {
        Param.Value = Yarn::Value(0.0f);
    }
    else
    {
        Param.Value = Yarn::Value("");
    }
    FuncDetail.OutParam = Param;

    AllFunctions.Add(FName(Func.DefinitionName), FuncDetail);
}


void UYarnLibraryRegistry::AddCommand(const FYSLSAction& Cmd)
{
    // TODO
}


void UYarnLibraryRegistry::OnStartGameInstance(UGameInstance* GameInstance)
{
    FindFunctionsAndCommands();
}


void UYarnLibraryRegistry::AddStdFunction(const FYarnStdLibFunction& Func)
{
    StdFunctions.Add(Func.Name, Func);
}


void UYarnLibraryRegistry::LoadStdFunctions()
{
    AddStdFunction({
        TEXT("Number.EqualTo"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.EqualTo called with incorrect number of parameters (expected 2).")
                return Yarn::Value();
            }
            if (Params[0].GetType() != Yarn::Value::ValueType::NUMBER || Params[1].GetType() != Yarn::Value::ValueType::NUMBER)
            {
                YS_WARN("Number.EqualTo called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetNumberValue() == Params[1].GetNumberValue());
        }
    });

    AddStdFunction({
        TEXT("Number.NotEqualTo"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.NotEqualTo called with incorrect number of parameters (expected 2).")
                return Yarn::Value();
            }
            if (Params[0].GetType() != Yarn::Value::ValueType::NUMBER || Params[1].GetType() != Yarn::Value::ValueType::NUMBER)
            {
                YS_WARN("Number.NotEqualTo called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetNumberValue() != Params[1].GetNumberValue());
        }
    });

    AddStdFunction({
        TEXT("Number.Add"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.Add called with incorrect number of parameters (expected 2).")
                return Yarn::Value();
            }
            if (Params[0].GetType() != Yarn::Value::ValueType::NUMBER || Params[1].GetType() != Yarn::Value::ValueType::NUMBER)
            {
                YS_WARN("Number.Add called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetNumberValue() + Params[1].GetNumberValue());
        }
    });

    AddStdFunction({
        TEXT("Number.Minus"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.Minus called with incorrect number of parameters (expected 2).")
                return Yarn::Value();
            }
            if (Params[0].GetType() != Yarn::Value::ValueType::NUMBER || Params[1].GetType() != Yarn::Value::ValueType::NUMBER)
            {
                YS_WARN("Number.Minus called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetNumberValue() - Params[1].GetNumberValue());
        }
    });

    AddStdFunction({
        TEXT("Number.Divide"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.Divide called with incorrect number of parameters (expected 2).")
                return Yarn::Value();
            }
            if (Params[0].GetType() != Yarn::Value::ValueType::NUMBER || Params[1].GetType() != Yarn::Value::ValueType::NUMBER)
            {
                YS_WARN("Number.Divide called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetNumberValue() / Params[1].GetNumberValue());
        }
    });

    AddStdFunction({
        TEXT("Number.Multiply"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.Multiply called with incorrect number of parameters (expected 2).")
                return Yarn::Value();
            }
            if (Params[0].GetType() != Yarn::Value::ValueType::NUMBER || Params[1].GetType() != Yarn::Value::ValueType::NUMBER)
            {
                YS_WARN("Number.Multiply called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetNumberValue() * Params[1].GetNumberValue());
        }
    });

    AddStdFunction({
        TEXT("Number.Modulo"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.Modulo called with incorrect number of parameters (expected 2).")
                return Yarn::Value();
            }
            if (Params[0].GetType() != Yarn::Value::ValueType::NUMBER || Params[1].GetType() != Yarn::Value::ValueType::NUMBER)
            {
                YS_WARN("Number.Modulo called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::Value();
            }
            return Yarn::Value(FMath::Fmod(Params[0].GetNumberValue(), Params[1].GetNumberValue()));
        }
    });

    AddStdFunction({
        TEXT("Number.UnaryMinus"), 1, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 1)
            {
                YS_WARN("Number.UnaryMinus called with incorrect number of parameters (expected 1).")
                return Yarn::Value();
            }
            if (Params[0].GetType() != Yarn::Value::ValueType::NUMBER)
            {
                YS_WARN("Number.UnaryMinus called with incorrect parameter types (expected NUMBER).")
                return Yarn::Value();
            }
            return Yarn::Value(-Params[0].GetNumberValue());
        }
    });

    AddStdFunction({
        TEXT("Number.GreaterThan"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.GreaterThan called with incorrect number of parameters (expected 2).")
                return Yarn::Value();
            }
            if (Params[0].GetType() != Yarn::Value::ValueType::NUMBER || Params[1].GetType() != Yarn::Value::ValueType::NUMBER)
            {
                YS_WARN("Number.GreaterThan called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetNumberValue() > Params[1].GetNumberValue());
        }
    });

    AddStdFunction({
        TEXT("Number.GreaterThanOrEqualTo"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.GreaterThanOrEqualTo called with incorrect number of parameters (expected 2).")
                return Yarn::Value();
            }
            if (Params[0].GetType() != Yarn::Value::ValueType::NUMBER || Params[1].GetType() != Yarn::Value::ValueType::NUMBER)
            {
                YS_WARN("Number.GreaterThanOrEqualTo called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetNumberValue() >= Params[1].GetNumberValue());
        }
    });

    AddStdFunction({
        TEXT("Number.LessThan"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.LessThan called with incorrect number of parameters (expected 2).")
                return Yarn::Value();
            }
            if (Params[0].GetType() != Yarn::Value::ValueType::NUMBER || Params[1].GetType() != Yarn::Value::ValueType::NUMBER)
            {
                YS_WARN("Number.LessThan called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetNumberValue() < Params[1].GetNumberValue());
        }
    });

    AddStdFunction({
        TEXT("Number.LessThanOrEqualTo"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Number.LessThanOrEqualTo called with incorrect number of parameters (expected 2).")
                return Yarn::Value();
            }
            if (Params[0].GetType() != Yarn::Value::ValueType::NUMBER || Params[1].GetType() != Yarn::Value::ValueType::NUMBER)
            {
                YS_WARN("Number.LessThanOrEqualTo called with incorrect parameter types (expected NUMBER, NUMBER).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetNumberValue() <= Params[1].GetNumberValue());
        }
    });

    AddStdFunction({
        TEXT("Bool.EqualTo"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Bool.EqualTo called with incorrect number of parameters (expected 2).")
                return Yarn::Value();
            }
            if (Params[0].GetType() != Yarn::Value::ValueType::BOOL || Params[1].GetType() != Yarn::Value::ValueType::BOOL)
            {
                YS_WARN("Bool.EqualTo called with incorrect parameter types (expected BOOL, BOOL).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetBooleanValue() == Params[1].GetBooleanValue());
        }
    });

    AddStdFunction({
        TEXT("Bool.NotEqualTo"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2)
            {
                YS_WARN("Bool.NotEqualTo called with incorrect number of parameters (expected 2).")
                return Yarn::Value();
            }
            if (Params[0].GetType() != Yarn::Value::ValueType::BOOL || Params[1].GetType() != Yarn::Value::ValueType::BOOL)
            {
                YS_WARN("Bool.NotEqualTo called with incorrect parameter types (expected BOOL, BOOL).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetBooleanValue() != Params[1].GetBooleanValue());
        }
    });

    AddStdFunction({
        TEXT("Bool.And"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2 || Params[0].GetType() != Yarn::Value::ValueType::BOOL || Params[1].GetType() != Yarn::Value::ValueType::BOOL)
            {
                YS_WARN("Bool.And called with incorrect number of parameters (expected 2) or incorrect parameter types (expected BOOL, BOOL).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetBooleanValue() && Params[1].GetBooleanValue());
        }
    });

    AddStdFunction({
        TEXT("Bool.Or"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2 || Params[0].GetType() != Yarn::Value::ValueType::BOOL || Params[1].GetType() != Yarn::Value::ValueType::BOOL)
            {
                YS_WARN("Bool.Or called with incorrect number of parameters (expected 2) or incorrect parameter types (expected BOOL, BOOL).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetBooleanValue() || Params[1].GetBooleanValue());
        }
    });

    AddStdFunction({
        TEXT("Bool.Xor"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2 || Params[0].GetType() != Yarn::Value::ValueType::BOOL || Params[1].GetType() != Yarn::Value::ValueType::BOOL)
            {
                YS_WARN("Bool.Xor called with incorrect number of parameters (expected 2) or incorrect parameter types (expected BOOL, BOOL).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetBooleanValue() != Params[1].GetBooleanValue());
        }
    });

    AddStdFunction({
        TEXT("Bool.Not"), 1, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 1 || Params[0].GetType() != Yarn::Value::ValueType::BOOL)
            {
                YS_WARN("Bool.Not called with incorrect number of parameters (expected 1) or incorrect parameter types (expected BOOL).")
                return Yarn::Value();
            }
            return Yarn::Value(!Params[0].GetBooleanValue());
        }
    });

    AddStdFunction({
        TEXT("String.EqualTo"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2 || Params[0].GetType() != Yarn::Value::ValueType::STRING || Params[1].GetType() != Yarn::Value::ValueType::STRING)
            {
                YS_WARN("String.EqualTo called with incorrect number of parameters (expected 2) or incorrect parameter types (expected STRING, STRING).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetStringValue() == Params[1].GetStringValue());
        }
    });

    AddStdFunction({
        TEXT("String.NotEqualTo"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2 || Params[0].GetType() != Yarn::Value::ValueType::STRING || Params[1].GetType() != Yarn::Value::ValueType::STRING)
            {
                YS_WARN("String.NotEqualTo called with incorrect number of parameters (expected 2) or incorrect parameter types (expected STRING, STRING).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetStringValue() != Params[1].GetStringValue());
        }
    });

    AddStdFunction({
        TEXT("String.Add"), 2, [](TArray<Yarn::Value> Params) -> Yarn::Value
        {
            if (Params.Num() != 2 || Params[0].GetType() != Yarn::Value::ValueType::STRING || Params[1].GetType() != Yarn::Value::ValueType::STRING)
            {
                YS_WARN("String.Add called with incorrect number of parameters (expected 2) or incorrect parameter types (expected STRING, STRING).")
                return Yarn::Value();
            }
            return Yarn::Value(Params[0].GetStringValue() + Params[1].GetStringValue());
        }
    });
}
