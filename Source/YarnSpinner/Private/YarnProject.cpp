// Fill out your copyright notice in the Description page of Project Settings.


#include "YarnProject.h"

#include "EditorFramework/AssetImportData.h"
#include "Engine/DataTable.h"
#include "Misc/YarnAssetHelpers.h"
#include "Misc/YSLogging.h"

bool UYarnProject::FindLine(const FName& LineId, FString& Line) const
{
	if (const FString* const FindStr = Lines.Find(LineId))
	{
		Line = *FindStr;
		return true;
	}

	return false;
}

void UYarnProject::SetLines(const TMap<FName, FString>& NewLines)
{
	Lines = NewLines;
}

void UYarnProject::SetLines(TMap<FName, FString>&& NewLines)
{
	Lines = MoveTemp(NewLines);
}

TSharedPtr<Yarn::Program> UYarnProject::GetProgram()
{
	if (!Program.IsValid())
	{
		// Re-hydrate the program as needed
		if (const TSharedRef<Yarn::Program> ProgramRef = MakeShared<Yarn::Program>();
			ProgramRef->ParsePartialFromArray(ProgramData.GetData(), ProgramData.Num()))
		{
			Program = ProgramRef;
		}
		else
		{
			YS_ERR("Failed to parse Yarn Spinner program from serialized data.");
		}
	}

	return Program;
}

FString UYarnProject::GetBaseLocAssetPackage() const
{
    return FPaths::Combine(FPaths::GetPath(GetPathName()), GetName() + TEXT("_Loc"));
}


FString UYarnProject::GetLocAssetPackage(const FString& Language) const
{
    return FPaths::Combine(GetBaseLocAssetPackage(), Language);
}


UDataTable* UYarnProject::GetLocTextDataTable(const FString& Language) const
{
    const FString LocalisedAssetPackage = GetLocAssetPackage(Language);
    TArray<FAssetData> AssetData = FYarnAssetHelpers::FindAssetsInRegistryByPackagePath<UDataTable>(LocalisedAssetPackage);
    return AssetData.Num() == 0 ? nullptr : Cast<UDataTable>(AssetData[0].GetAsset());
}


TArray<TSoftObjectPtr<UObject>> UYarnProject::GetLineAssets(const FName& Name) const
{
    if (!LineAssets.Contains(Name))
        return {};
    return LineAssets[Name];
}


void UYarnProject::PostInitProperties()
{
	Super::PostInitProperties();

	// We only want these properties for actual instances, not the default object
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
#if WITH_EDITORONLY_DATA
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
#endif // WITH_EDITORONLY_DATA

		// Find related line assets
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.PackagePaths.Add("/Game");
		TArray<FAssetData> AssetData;
		AssetRegistryModule.Get().GetAssets(Filter, AssetData);
		for (const FAssetData& Asset : AssetData)
		{
			YS_LOG_FUNC("Found asset: %s", *Asset.AssetName.ToString());
			const FName LineId = FName(FString::Printf(TEXT("line:%s"), *Asset.AssetName.ToString()));
			if (HasLine(LineId))
			{
				// TODO: if path contains a loc identifier, add to loc table
				LineAssets.FindOrAdd(LineId).Add(TSoftObjectPtr<UObject>(Asset.ToSoftObjectPath()));
			}
		}
	}
}

#if WITH_EDITORONLY_DATA

void UYarnProject::SetYarnSources(const TArray<FString>& NewYarnSources)
{
	const FString ProjectPath = GetPath();
	SourceFiles.Reset();

	YS_LOG("Setting yarn project asset sources for project: %s", *ProjectPath);
	for (const FString& SourceFile : NewYarnSources)
	{
		const FString FullPath = FPaths::IsRelative(SourceFile) ? FPaths::Combine(ProjectPath, SourceFile) : SourceFile;
		if (!FPaths::FileExists(FullPath))
		{
			YS_WARN("- Yarn source file '%s' does not exist, skipping.", *FullPath);
			continue;
		}

		FYarnSourceMeta MetaData = FYarnSourceMeta {
			SourceFile,
			IFileManager::Get().GetTimeStamp(*FullPath),
			LexToString(FMD5Hash::HashFile(*FullPath)) 
		};

		YS_LOG("- Adding source file %s (timestamp: %s, hash: %s, full path: %s)",
			*SourceFile,
			*MetaData.Timestamp.ToString(),
			*MetaData.FileHash,
			*FullPath);

		SourceFiles.Emplace(MoveTemp(MetaData));
	}
}


bool UYarnProject::ShouldRecompile(const TArray<FString>& LatestYarnSources) const
{
	const FString ProjectPath = GetPath();

	YS_LOG("Checking if should recompile for project: %s", *ProjectPath);
	YS_LOG("Sources included in last compile:");
	for (const FYarnSourceMeta& OriginalSource : SourceFiles)
	{
		YS_LOG("--> %s", *OriginalSource.Filename);
	}
	YS_LOG("Latest yarn sources:");
	for (const FString& NewSource : LatestYarnSources)
	{
		YS_LOG("--> %s", *NewSource);
	}

	if (SourceFiles.Num() != LatestYarnSources.Num())
	{
		return true;
	}

	for (const FYarnSourceMeta& OriginalSource : SourceFiles)
	{
		if (!LatestYarnSources.Contains(OriginalSource.Filename))
		{
			YS_LOG("Original source file %s not in latest yarn sources", *OriginalSource.Filename);
			return true;
		}
	}

	// it's all the same files, so compare timestamps and hashes
	for (const FYarnSourceMeta& OriginalSource : SourceFiles)
	{
		const FString FullPath = FPaths::IsRelative(OriginalSource.Filename) ? FPaths::Combine(ProjectPath, OriginalSource.Filename) : OriginalSource.Filename;
		if (!FPaths::FileExists(FullPath))
		{
			YS_WARN("Yarn source file '%s' does not exist.", *FullPath);
			return true;
		}
		
		if (IFileManager::Get().GetTimeStamp(*FullPath) != OriginalSource.Timestamp)
		{
			if (LexToString(FMD5Hash::HashFile(*FullPath)) != OriginalSource.FileHash)
			{
				YS_LOG("Source file %s has changed", *OriginalSource.Filename);
				return true;
			}
		}
	}
	
	return false;
}


FString UYarnProject::GetPath() const
{
	return FPaths::GetPath(AssetImportData->GetFirstFilename());
}

void UYarnProject::SetProgram(const Yarn::Program& NewProgram)
{
	// Convert the Program into binary wire format for saving
	const std::string Data = NewProgram.SerializeAsString();
	// And convert THAT into a TArray of bytes for storage
	ProgramData = TArray(reinterpret_cast<const uint8*>(Data.c_str()), Data.size());
}

#endif
