// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "YarnSpinnerCore/yarn_spinner.pb.h"
#include "YarnProject.generated.h"


USTRUCT()
struct FYarnSourceMeta
{
	GENERATED_BODY()

	// Filename of the source file
	UPROPERTY(VisibleDefaultsOnly)
	FString Filename;

	// Timestamp of the file when it was imported.
	UPROPERTY()
	FDateTime Timestamp;

	// MD5 hash of the file when it was imported.
	UPROPERTY()
	FString FileHash;
};


/**
 * Data representing a Yarn Project (.yarnproject file)
 */
UCLASS(AutoExpandCategories = "ImportOptions")
class YARNSPINNER_API UYarnProject : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Yarn Spinner")
	FORCEINLINE bool HasLine(const FName& LineId) const { return Lines.Contains(LineId); }

	UFUNCTION(BlueprintCallable, Category="Yarn Spinner")
	bool FindLine(const FName& LineId, FString& Line) const;

	UFUNCTION(BlueprintPure, Category="Yarn Spinner")
	FORCEINLINE FString& GetLine(const FName& LineId) { return Lines[LineId]; }

	UFUNCTION(BlueprintCallable, Category="Yarn Spinner")
	void SetLines(const TMap<FName, FString>& NewLines);
	void SetLines(TMap<FName, FString>&& NewLines);

	UFUNCTION(BlueprintCallable, Category="Yarn Spinner")
    FString GetLocAssetPackage(const FString& Language) const;

	UFUNCTION(BlueprintCallable, Category="Yarn Spinner")
	UDataTable* GetLocTextDataTable(const FString& Language) const;

	UFUNCTION(BlueprintCallable, Category="Yarn Spinner")
    TArray<TSoftObjectPtr<UObject>> GetLineAssets(const FName& Name) const;

	virtual void PostInitProperties() override;

#if WITH_EDITORONLY_DATA
	void SetYarnSources(const TArray<FString>& NewYarnSources);
	bool ShouldRecompile(const TArray<FString>& LatestYarnSources) const;
	void SetProgram(const Yarn::Program& NewProgram);

	UFUNCTION(BlueprintPure)
	FORCEINLINE FString GetPath() const;

    /** The file this data table was imported from, may be empty */
	UPROPERTY(VisibleAnywhere, Instanced, Category=ImportSource)
	UAssetImportData* AssetImportData;
#endif
	
	UE_NODISCARD TSharedPtr<Yarn::Program> GetProgram();

protected:
	UPROPERTY()
	TArray<uint8> ProgramData;

	// UPROPERTY(EditDefaultsOnly, Category = "Yarn Spinner")
	// TArray<TSubclassOf<class AYarnFunctionLibrary>> FunctionLibraries;
	
	UFUNCTION(BlueprintPure, Category="Yarn Spinner")
	FString GetBaseLocAssetPackage() const;
	
	// All of the lines throughout the yarn project
	UPROPERTY(VisibleAnywhere, Category="Yarn Spinner")
	TMap<FName, FString> Lines;
	
	// Source yarn files that were added tot he project
	// Relative to the .yarnproject file, if imported
	UPROPERTY(EditDefaultsOnly, Category="Yarn Spinner|Files")
	TArray<FYarnSourceMeta> SourceFiles;

	// Re-hydrated project instance
	TSharedPtr<Yarn::Program> Program = nullptr;

	// Assets that are utilized in lines
	// Map is from Line Id -> Soft Object Ptr
    TMap<FName, TArray<TSoftObjectPtr<>>> LineAssets;
};
