#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"


class FYarnAssetHelpers
{
public:
    static FString YSLSFilePath()
    {
        return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("YarnSpinner"), TEXT("Library.ysls"));
    }
    
    template <class AssetClass>
    static TArray<FAssetData> FindAssetsInRegistry(const TSubclassOf<UObject> BaseClass = AssetClass::StaticClass());
    
    template <class AssetClass>
    static TArray<FAssetData> FindAssetsInRegistryByPackageName(const FName SearchPackage, const TSubclassOf<UObject> BaseClass = AssetClass::StaticClass());

    template <class AssetClass>
    static TArray<FAssetData> FindAssetsInRegistryByPackagePath(const FName SearchPackage, const TSubclassOf<UObject> BaseClass = AssetClass::StaticClass());

    template <class AssetClass>
    static TArray<FAssetData> FindAssetsInRegistryByPackagePath(const FString SearchPackage, const TSubclassOf<UObject> BaseClass = AssetClass::StaticClass());

    template <class AssetClass>
    static FARFilter GetClassPathFilter(const UClass* const Class = AssetClass::StaticClass());

    template <class AssetClass>
    static TArray<AssetClass*> FindBlueprintsOfClass(const UClass* const Class = AssetClass::StaticClass());

    template <class AssetClass>
    static TArray<UBlueprint*> FindBlueprintAssetsOfClass(const UClass* const Class = AssetClass::StaticClass());

    template <class AssetClass>
    static bool IsBlueprintAssetOfClass(const UBlueprint* const Blueprint, const UClass* const Class = AssetClass::StaticClass());

private:
    static TArray<FAssetData> FindAssets(const FARFilter& AssetFilter);
};


template <class AssetClass>
TArray<FAssetData> FYarnAssetHelpers::FindAssetsInRegistry(const TSubclassOf<UObject> BaseClass)
{
   return FindAssets(GetClassPathFilter<AssetClass>(BaseClass));
}


template <class AssetClass>
TArray<FAssetData> FYarnAssetHelpers::FindAssetsInRegistryByPackageName(const FName SearchPackage, const TSubclassOf<UObject> BaseClass)
{
    FARFilter AssetFilter = GetClassPathFilter<AssetClass>(BaseClass);
    AssetFilter.PackageNames.Add(SearchPackage);
    return FindAssets(AssetFilter);
}


template <class AssetClass>
TArray<FAssetData> FYarnAssetHelpers::FindAssetsInRegistryByPackagePath(const FName SearchPackage, const TSubclassOf<UObject> BaseClass)
{
    FARFilter AssetFilter = GetClassPathFilter<AssetClass>(BaseClass);
    AssetFilter.PackagePaths.Add(SearchPackage);
    return FindAssets(AssetFilter);
}


template <class AssetClass>
TArray<FAssetData> FYarnAssetHelpers::FindAssetsInRegistryByPackagePath(const FString SearchPackage, const TSubclassOf<UObject> BaseClass)
{
    return FindAssetsInRegistryByPackagePath<AssetClass>(FName(SearchPackage), BaseClass);
}

template <class AssetClass>
FARFilter FYarnAssetHelpers::GetClassPathFilter(const UClass* const Class)
{
    FARFilter AssetFilter; 
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    AssetFilter.ClassPaths.Add(Class->GetClassPathName());
#else
    Filter.ClassNames.Add(Class->GetFName());
#endif
    return AssetFilter;
}

template <class AssetClass>
TArray<AssetClass*> FYarnAssetHelpers::FindBlueprintsOfClass(const UClass* const Class)
{
    TArray<AssetClass*> Blueprints;
    FAssetRegistryModule::GetRegistry().EnumerateAssets(GetClassPathFilter<UBlueprint>(),
[&Blueprints](const FAssetData& AssetData) -> bool
    {
        if (const UBlueprint* const BlueprintObj = Cast<UBlueprint>(AssetData.GetAsset()))
        {
            if (IsBlueprintAssetOfClass<AssetClass>(BlueprintObj))
            {
                if (AssetClass* const BlueprintDefObj = BlueprintObj->GeneratedClass->GetDefaultObject<AssetClass>(); IsValid(BlueprintDefObj))
                {
                    Blueprints.Add(BlueprintDefObj);
                }
            }
        }
    
        return true;
    });
    return Blueprints;
}

template <class AssetClass>
TArray<UBlueprint*> FYarnAssetHelpers::FindBlueprintAssetsOfClass(const UClass* const Class)
{
    TArray<UBlueprint*> Blueprints;
    FAssetRegistryModule::GetRegistry().EnumerateAssets(GetClassPathFilter<UBlueprint>(),
[&Blueprints](const FAssetData& AssetData) -> bool
    {
        if (UBlueprint* const BlueprintObj = Cast<UBlueprint>(AssetData.GetAsset()))
        {
            if (IsBlueprintAssetOfClass<AssetClass>(BlueprintObj))
            {
                Blueprints.Add(BlueprintObj);
            }
        }
    
        return true;
    });
    return Blueprints;
}

template <class AssetClass>
bool FYarnAssetHelpers::IsBlueprintAssetOfClass(const UBlueprint* const Blueprint, const UClass* const Class)
{
    if (!IsValid(Blueprint) || !Blueprint->GeneratedClass || !Blueprint->ParentClass)
    {
        return false;
    }
            
    if (const UClass* const BlueprintParentClass = Blueprint->ParentClass; BlueprintParentClass->IsChildOf(Class))
    {
        return true;
    }

    return false;
}


inline TArray<FAssetData> FYarnAssetHelpers::FindAssets(const FARFilter& AssetFilter)
{
    TArray<FAssetData> ExistingAssets;
    FAssetRegistryModule::GetRegistry().GetAssets(AssetFilter, ExistingAssets);
    return ExistingAssets;
}


