// Copyright Epic Games, Inc. All Rights Reserved.

#include "YarnSpinnerEditor.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "IYarnSpinnerModuleInterface.h"
#include "YarnAssetActions.h"
#include "YarnAssetFactory.h"
#include "YarnProjectSynchronizer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/YSLogging.h"
#include "Serialization/Csv/CsvParser.h"


DEFINE_LOG_CATEGORY(LogYarnSpinnerEditor);


void FYarnSpinnerEditor::AddModuleListeners()
{
	// add tools later
}


void FYarnSpinnerEditor::StartupModule()
{
	YS_LOG_FUNCSIG

	// Register custom types:
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		// add custom category
		EAssetTypeCategories::Type YarnSpinnerAssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("YarnSpinner")), FText::FromString("YarnSpinner"));

		// register our custom asset with example category
		const TSharedRef<IAssetTypeActions> Action = MakeShared<FYarnAssetActions>(YarnSpinnerAssetCategory);
		AssetTools.RegisterAssetTypeActions(Action);

		// saved it here for unregister later
		CreatedAssetTypeActions.Add(Action);
	}

// 	LocFileImporter = NewObject<UCSVImportFactory>();
// 	LocFileImporter->AddToRoot();
// 	UDataTable* ImportOptions = NewObject<UDataTable>();
// 	ImportOptions->RowStruct = FDisplayLine::StaticStruct();
// #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
//     ImportOptions->RowStructPathName = FDisplayLine::StaticStruct()->GetStructPathName();
// #else
// 	ImportOptions->RowStructName = FDisplayLine::StaticStruct()->GetFName();
// #endif
// 	ImportOptions->bIgnoreExtraFields = true;
// 	ImportOptions->bIgnoreMissingFields = false;
// 	ImportOptions->ImportKeyField = "id"; // becomes the Name field of the DataTable
// 	LocFileImporter->DataTableImportOptions = ImportOptions;
// 	// The FCSVImportSettings struct is not part of the dll export so we have to use the JSON API to set it up
// 	TSharedRef<FJsonObject> CSVImportSettings = MakeShareable(new FJsonObject());
// 	CSVImportSettings->SetField("ImportType", MakeShareable(new FJsonValueNumber(static_cast<uint8>(ECSVImportType::ECSV_DataTable))));
// 	CSVImportSettings->SetField("ImportRowStruct", MakeShareable(new FJsonValueString(FDisplayLine::StaticStruct()->GetName())));
// 	
// 	LocFileImporter->ParseFromJson(CSVImportSettings);

	YarnProjectSynchronizer = MakeUnique<FYarnProjectSynchronizer>();
	// YarnProjectSynchronizer->SetLocFileImporter(LocFileImporter);


	FCsvParser Parser("col1,col2,col3\n1,2,3\n4,5,6");
	FCsvParser::FRows Rows = Parser.GetRows();
	for (auto R: Rows)
	{
		for (auto F : R)
		{
			UE_LOG(LogTemp, Log, TEXT("Field: %s"), F);
		}
	}

	IYarnSpinnerModuleInterface::StartupModule();
}


void FYarnSpinnerEditor::ShutdownModule()
{
	// Unregister all the asset types that we registered
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (int32 i = 0; i < CreatedAssetTypeActions.Num(); ++i)
		{
			AssetTools.UnregisterAssetTypeActions(CreatedAssetTypeActions[i]);
		}
	}
	CreatedAssetTypeActions.Empty();

	YarnProjectSynchronizer.Reset();

    // if (LocFileImporter && LocFileImporter->IsRooted() && !LocFileImporter->IsPendingKill())
    // {
    //     LocFileImporter->RemoveFromRoot();
    //     LocFileImporter = nullptr;
    // }

	IYarnSpinnerModuleInterface::ShutdownModule();
}


IMPLEMENT_MODULE(FYarnSpinnerEditor, YarnSpinnerEditor)
