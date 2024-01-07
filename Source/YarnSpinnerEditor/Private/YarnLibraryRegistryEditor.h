// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Library/YarnLibraryRegistry.h"
#include "Library/YarnSpinnerLibraryData.h"
#include "UObject/Object.h"
#include "YarnLibraryRegistryEditor.generated.h"

USTRUCT()
struct YARNSPINNEREDITOR_API FYarnLibFuncNames
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TArray<FName> Names;
};

/**
 * 
 */
UCLASS()
class YARNSPINNEREDITOR_API UYarnLibraryRegistryEditor : public UObject
{
    GENERATED_BODY()

public:
    UYarnLibraryRegistryEditor();
    virtual void BeginDestroy() override;

private:
    UPROPERTY(Transient)
    FYarnSpinnerLibraryData YSLSData;

    // Mapping of function library blueprints to function names
    UPROPERTY(Transient)
    TMap<UBlueprint*, FYarnLibFuncNames> LibFunctions;

    // Mapping of command library blueprints to function names
    UPROPERTY(Transient)
    TMap<UBlueprint*, FYarnLibFuncNames> LibCommands;
    
    // Mapping of custom function name to implementation details
    UPROPERTY(Transient)
    TMap<FName, FYarnBlueprintLibFunction> AllFunctions;
    
    // Mapping of standard function name to implementation details
    UPROPERTY(Transient)
    TMap<FName, FYarnStdLibFunction> StdFunctions;
    
    // Mapping of custom command name to implementation details
    UPROPERTY(Transient)
    TMap<FName, FYarnBlueprintLibFunction> AllCommands;

    // Delegate handles
    FDelegateHandle OnAssetRegistryFilesLoadedHandle;
    FDelegateHandle OnAssetAddedHandle;
    FDelegateHandle OnAssetRemovedHandle;
    FDelegateHandle OnAssetUpdatedHandle;
    FDelegateHandle OnAssetRenamedHandle;
    FDelegateHandle OnStartGameHandle;
    
    void FindFunctionsAndCommands();
    static void ExtractFunctionDataFromBlueprintGraph(UBlueprint* YarnFunctionLibrary, UEdGraph* Func,
        FYarnBlueprintLibFunction& FuncDetails, FYarnBlueprintLibFunctionMeta& FuncMeta,
        bool bExpectDialogueRunnerParam = false);

    void AddToYSLSData(FYarnBlueprintLibFunction FuncDetails);
    
    // Import functions for a given Blueprint
    void ImportFunctions(UBlueprint* YarnFunctionLibrary);
    // Import functions for a given Blueprint
    void ImportCommands(UBlueprint* YarnCommandLibrary);
    // Update functions for a given Blueprint
    void UpdateFunctions(UBlueprint* YarnFunctionLibrary);
    // Update functions for a given Blueprint
    void UpdateCommands(UBlueprint* YarnCommandLibrary);
    // Clear functions and references for a given Blueprint
    void RemoveFunctions(UBlueprint* YarnFunctionLibrary);
    // Clear functions and references for a given Blueprint
    void RemoveCommands(UBlueprint* YarnCommandLibrary);

    // Delegate callbacks
    UFUNCTION()
    void OnAssetRegistryFilesLoaded();
    
    UFUNCTION()
    void OnAssetAdded(const FAssetData& AssetData);
    
    UFUNCTION()
    void OnAssetRemoved(const FAssetData& AssetData);
    
    UFUNCTION()
    void OnAssetUpdated(const FAssetData& AssetData);
    
    UFUNCTION()
    void OnAssetRenamed(const FAssetData& AssetData, const FString& String);
    
    UFUNCTION()
    void OnStartGameInstance(UGameInstance* GameInstance);
};
