#pragma once

#include "CoreMinimal.h"
#include "IAssetTypeActions.h"
#include "IYarnSpinnerModuleInterface.h"


YARNSPINNEREDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogYarnSpinnerEditor, Log, All);

class FYarnSpinnerEditor final : public IYarnSpinnerModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    virtual void AddModuleListeners() override;

    UE_NODISCARD FORCEINLINE static FYarnSpinnerEditor& Get()
    {
        return FModuleManager::LoadModuleChecked<FYarnSpinnerEditor>("YarnSpinnerEditor");
    }

    UE_NODISCARD FORCEINLINE static bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("YarnSpinnerEditor");
    }

    TArray<TSharedRef<IAssetTypeActions>> CreatedAssetTypeActions;

	
private:
	class UCSVImportFactory* LocFileImporter = nullptr;
	TUniquePtr<class FYarnProjectSynchronizer> YarnProjectSynchronizer;
};
