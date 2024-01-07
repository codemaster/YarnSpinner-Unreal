// Fill out your copyright notice in the Description page of Project Settings.


#include "YarnSubsystem.h"

#include "Engine/ObjectLibrary.h"
#include "Library/YarnCommandLibrary.h"
#include "Library/YarnFunctionLibrary.h"
#include "Library/YarnLibraryRegistry.h"
#include "Misc/YarnAssetHelpers.h"
#include "Misc/YSLogging.h"


UYarnSubsystem::UYarnSubsystem()
{
    YS_LOG_FUNCSIG

    // TODO: move to editor?
    YarnFunctionObjectLibrary = UObjectLibrary::CreateLibrary(UYarnFunctionLibrary::StaticClass(), true, true);
    YarnCommandObjectLibrary = UObjectLibrary::CreateLibrary(UYarnCommandLibrary::StaticClass(), true, true);
    YarnFunctionObjectLibrary->AddToRoot();
    YarnCommandObjectLibrary->AddToRoot();
    YarnFunctionObjectLibrary->bRecursivePaths = true;
    YarnCommandObjectLibrary->bRecursivePaths = true;
    YarnFunctionObjectLibrary->LoadAssetDataFromPath(TEXT("/Game"));
    YarnCommandObjectLibrary->LoadAssetDataFromPath(TEXT("/Game"));

    YarnFunctionRegistry = NewObject<UYarnLibraryRegistry>(this, "YarnFunctionRegistry");
}


void UYarnSubsystem::SetValue(const FString& name, bool value)
{
    Variables.FindOrAdd(name) = value;
}


void UYarnSubsystem::SetValue(const FString& name, float value)
{
    Variables.FindOrAdd(name) = value;
}


void UYarnSubsystem::SetValue(const FString& name, const FString& value)
{
    Variables.FindOrAdd(name) = value;
}


bool UYarnSubsystem::HasValue(const FString& name)
{
    return Variables.Contains(name);
}


Yarn::FValue UYarnSubsystem::GetValue(const FString& name)
{
    return Variables.FindOrAdd(name);
}


void UYarnSubsystem::ClearValue(const FString& name)
{
    Variables.Remove(name);
}




