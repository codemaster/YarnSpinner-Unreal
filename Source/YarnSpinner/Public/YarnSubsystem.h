// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Library/YarnLibraryRegistry.h"

#include "Engine/DataTable.h"
#include "Engine/ObjectLibrary.h"
#include "YarnSpinnerCore/VirtualMachine.h"

#include "YarnSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class YARNSPINNER_API UYarnSubsystem : public UGameInstanceSubsystem, public Yarn::IVariableStorage
{
    GENERATED_BODY()
public:
    UYarnSubsystem();

    virtual void SetValue(const FString& name, bool value) override;
    virtual void SetValue(const FString& name, float value) override;
    virtual void SetValue(const FString& name, const FString& value) override;

    virtual bool HasValue(const FString& name) override;
    virtual Yarn::FValue GetValue(const FString& name) override;

    virtual void ClearValue(const FString& name) override;

    UE_NODISCARD FORCEINLINE const UYarnLibraryRegistry* GetYarnLibraryRegistry() const { return YarnFunctionRegistry; }

private:
    // UPROPERTY()
    // TMap<UYarnProjectAsset*, TMap<FName, UDataTable*>> LocTextDataTables;

    UPROPERTY()
    UYarnLibraryRegistry* YarnFunctionRegistry;

    UPROPERTY()
    UObjectLibrary* YarnFunctionObjectLibrary;
    UPROPERTY()
    UObjectLibrary* YarnCommandObjectLibrary;
    
    TMap<FString, Yarn::FValue> Variables;
    
    FDelegateHandle OnAssetRegistryFilesLoadedHandle;
    FDelegateHandle OnLevelAddedToWorldHandle;
    FDelegateHandle OnWorldInitializedActorsHandle;
    
};


