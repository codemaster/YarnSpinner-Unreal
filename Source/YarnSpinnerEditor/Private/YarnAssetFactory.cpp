// Fill out your copyright notice in the Description page of Project Settings.

#include "YarnAssetFactory.h"

#include "ISourceControlModule.h"
#include "ISourceControlOperation.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "LocalizationCommandletExecution.h"
#include "LocalizationConfigurationScript.h"
#include "LocalizationSettings.h"
#include "LocalizationTargetTypes.h"
#include "LocTextHelper.h"
#include "YarnSpinnerEditor.h"

#include "Misc/FileHelper.h"
#include "EditorFramework/AssetImportData.h"
#include "Containers/UnrealString.h"

#include "ReimportYarnAssetFactory.h"
#include "SourceControlOperations.h"
#include "YarnProjectMeta.h"
#include "Misc/YSLogging.h"
#include "Serialization/Csv/CsvParser.h"

THIRD_PARTY_INCLUDES_START
#include "YarnSpinnerCore/yarn_spinner.pb.h"
#include "YarnSpinnerCore/compiler_output.pb.h"

#include <google/protobuf/util/json_util.h>
THIRD_PARTY_INCLUDES_END

// google::protobuf::Message &from_json(google::protobuf::Message &msg, const std::string &json);

UYarnAssetFactory::UYarnAssetFactory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    Formats.Add(FString(TEXT("yarnproject;")) + NSLOCTEXT("UYarnAssetFactory", "FormatTxt", "Yarn Project File").ToString());
    SupportedClass = UYarnProject::StaticClass();
    bCreateNew = true;
    bEditorImport = true;
}


UObject* UYarnAssetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
    UObject* Context, FFeedbackContext* Warn)
{
    YS_LOG_FUNCSIG

    return NewObject<UYarnProject>(InParent, InClass, InName, Flags);
}

UObject* UYarnAssetFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
{
    YS_LOG_FUNCSIG

    //    FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

    UYarnProject* YarnProject = nullptr;
    FString TextString;

    YarnProject = NewObject<UYarnProject>(InParent, InClass, InName, Flags);
    // YarnAsset->SourceFilePath = UAssetImportData::SanitizeImportFilename(CurrentFilename, YarnAsset->GetOutermost());

    const TCHAR* FileName = *CurrentFilename;

    Yarn::CompilerOutput CompilerOutput;

    // Record where this asset came from so we know how to update it
    if (!CurrentFilename.IsEmpty())
    {
        YarnProject->AssetImportData->Update(CurrentFilename);
    }

    bool bSuccess = GetCompiledDataForYarnProject(FileName, CompilerOutput);

    if (!bSuccess)
    {
        UE_LOG(LogYarnSpinnerEditor, Error, TEXT("Failed to get results from the compiler. Stopping import."));
        return nullptr;
    }

    bool bAnyErrors = false;
    for (auto Diagnostic : CompilerOutput.diagnostics())
    {
        if (Diagnostic.severity() == Yarn::Diagnostic_Severity::Diagnostic_Severity_Error)
        {
            UE_LOG(LogYarnSpinnerEditor, Error, TEXT("Error: %hs:%i %hs"),
                   UTF8_TO_TCHAR(Diagnostic.filename().c_str()),
                   Diagnostic.range().start().line(),
                   UTF8_TO_TCHAR(Diagnostic.message().c_str()));
            bAnyErrors = true;
        }
        else if (Diagnostic.severity() == Yarn::Diagnostic_Severity::Diagnostic_Severity_Warning)
        {
            UE_LOG(LogYarnSpinnerEditor, Warning, TEXT("Warning: %hs:%i %hs"),
                   UTF8_TO_TCHAR(Diagnostic.filename().c_str()),
                   Diagnostic.range().start().line(),
                   UTF8_TO_TCHAR(Diagnostic.message().c_str()));
        }
        else if (Diagnostic.severity() == Yarn::Diagnostic_Severity::Diagnostic_Severity_Info)
        {
            UE_LOG(LogYarnSpinnerEditor, Log, TEXT("%hs:%i %hs"),
                   UTF8_TO_TCHAR(Diagnostic.filename().c_str()),
                   Diagnostic.range().start().line(),
                   UTF8_TO_TCHAR(Diagnostic.message().c_str()));
        }
    }

    if (bAnyErrors)
    {
        UE_LOG(LogYarnSpinnerEditor, Error, TEXT("File contains errors; stopping import."));
        return nullptr;
    }
    
    YarnProject->SetProgram(CompilerOutput.program());

    // Convert the protocol buffer lines to Unreal formats
    TMap<FName, FString> LinesMap;
    Algo::Transform(CompilerOutput.strings(), LinesMap,
        [](const google::protobuf::MapPair<std::string, Yarn::StringInfo>& Pair) -> TPair<FName, FString>
    {
        return TPair<FName, FString>(
            FName(Pair.first.c_str()),
            FString(Pair.second.text().c_str())
        );
    });
    
    // And then we move the transformed values into the project
    YarnProject->SetLines(MoveTemp(LinesMap));

    // Record where this asset came from so we know how to update it
    if (!CurrentFilename.IsEmpty())
    {
        YarnProject->AssetImportData->Update(CurrentFilename);
    }

    // Store source file data on asset for future comparison
    TArray<FString> SourceFiles;
    bSuccess = GetSourcesForProject(FileName, SourceFiles);

    if (bSuccess)
    {
        YarnProject->SetYarnSources(SourceFiles);
    }

    BuildLocalizationTarget(YarnProject, CompilerOutput);

    //    YarnAsset->PostEditChange();
    //    YarnAsset->MarkPackageDirty();

    //    FEditorDelegates::OnAssetPostImport.Broadcast(this, YarnAsset);

    return YarnProject;
}


bool UYarnAssetFactory::FactoryCanImport(const FString& Filename)
{
    return FPaths::GetExtension(Filename).Equals(TEXT("yarnproject"));
}


EReimportResult::Type UYarnAssetFactory::Reimport(const UYarnProject* YarnProject)
{
    YS_LOG_FUNCSIG

    const FString Path = IsValid(YarnProject) ? YarnProject->AssetImportData->GetFirstFilename() : FString();

    if (Path.IsEmpty() == false)
    {
        const FString FilePath = IFileManager::Get().ConvertToRelativePath(*Path);

        TArray<uint8> Data;

        if (FFileHelper::LoadFileToArray(Data, *FilePath))
        {
            const uint8* Ptr = Data.GetData();
            CurrentFilename = FilePath; //not thread safe but seems to be how it is done..
            bool bWasCancelled = false;
            const UYarnProject* Result = Cast<UYarnProject>(FactoryCreateBinary(YarnProject->GetClass(), YarnProject->GetOuter(), YarnProject->GetFName(), YarnProject->GetFlags(), nullptr, *FPaths::GetExtension(FilePath), Ptr, Ptr + Data.Num(), GWarn));

            // if (bWasCancelled)
            // {
            // 	return EReimportResult::Cancelled;
            // }
            return Result ? EReimportResult::Succeeded : EReimportResult::Failed;
        }
    }
    return EReimportResult::Failed;
}


bool UYarnAssetFactory::GetCompiledDataForYarnProject(const TCHAR* InFilePath, Yarn::CompilerOutput& CompilerOutput)
{
    FString StdOut;
    FString StdErr;

    int32 ReturnCode;

    const FString Params = FString::Printf(TEXT("compile --stdout %s"), InFilePath);

    // Run ysc to get our compilation result
    UE_LOG(LogYarnSpinnerEditor, Log, TEXT("Calling ysc with %s"), *Params);
    FPlatformProcess::ExecProcess(YSC_PATH, *Params, &ReturnCode, &StdOut, &StdErr);

    UE_LOG(LogYarnSpinnerEditor, Log, TEXT("ysc returned %i;"), ReturnCode);
    UE_LOG(LogYarnSpinnerEditor, Log, TEXT("stdout:"));
    YS_LOG_CLEAN("%s", *StdOut);
    UE_LOG(LogYarnSpinnerEditor, Log, TEXT("stderr:"));
    YS_LOG_CLEAN("%s", *StdErr);

    if (ReturnCode != 0)
    {
        UE_LOG(LogYarnSpinnerEditor, Error, TEXT("Error compiling Yarn script: %s"), *StdErr);
        return false;
    }

    // Convert stdout from an FString to a std::string
    const std::string JSON(TCHAR_TO_UTF8(*StdOut));

    // Parse the incoming JSON into a Program message (to check that it's valid)
    const auto Status = google::protobuf::util::JsonStringToMessage(JSON, &CompilerOutput);

    if (!Status.ok())
    {
        // Whoa, we failed to parse a CompilerOutput struct from the compiler.
        UE_LOG(LogYarnSpinnerEditor, Error, TEXT("Error importing result from ysc: %s"), *FString(Status.ToString().c_str()));

        return false;
    }

    return true;
}


bool UYarnAssetFactory::GetSourcesForProject(const TCHAR* InFilePath, TArray<FString>& SourceFiles)
{
    // Run ysc again to get the list of source yarn files used in the compilation
    FString StdOut;
    FString StdErr;
    int32 ReturnCode;

    const FString Params = FString::Printf(TEXT("list-sources %s"), InFilePath);
    FPlatformProcess::ExecProcess(YSC_PATH, *Params, &ReturnCode, &StdOut, &StdErr);

    if (ReturnCode != 0)
    {
        YS_ERR("Error getting .yarn source list: %s", *StdErr);
        return false;
    }

    StdOut.ParseIntoArrayLines(SourceFiles);
    return true;
}


bool UYarnAssetFactory::GetSourcesForProject(const UYarnProject* YarnProjectAsset, TArray<FString>& SourceFiles)
{
    if (!YarnProjectAsset->AssetImportData)
    {
        YS_ERR("YarnProjectAsset has no AssetImportData");
        return false;
    }
    return GetSourcesForProject(*YarnProjectAsset->AssetImportData->GetFirstFilename(), SourceFiles);
}


void UYarnAssetFactory::BuildLocalizationTarget(const UYarnProject* YarnProject, const Yarn::CompilerOutput& CompilerOutput) const
{

    TOptional<FYarnProjectMetaData> ProjectMeta = FYarnProjectMetaData::FromAsset(YarnProject);
    if (!ProjectMeta.IsSet())
    {
        YS_ERR("Failed to get project metadata from .yarnproject file.  Could not create/update localisation target.");
        return;
    }

    const FString LocTargetName = YarnProject->GetName();
    const FString LocTargetPath = FPaths::ProjectContentDir() / TEXT("Localization") / LocTargetName;

    TArray<FString> Cultures;
    ProjectMeta->localisation.GetKeys(Cultures);

    // Find/create localisation target config, ensuring we add it to the correct target set
    ULocalizationTarget* LocTarget = nullptr;
    for (ULocalizationTarget* Target : ULocalizationSettings::GetGameTargetSet()->TargetObjects)
    {
        if (Target && Target->Settings.Name == LocTargetName)
        {
            YS_LOG("Found existing localisation target, updating...")
            LocTarget = Target;
            break;
        }
    }
    if (!LocTarget)
    {
        YS_LOG("Did not find existing localisation target for '%s', creating new one", *LocTargetName)
        LocTarget = NewObject<ULocalizationTarget>(ULocalizationSettings::GetGameTargetSet());
        // Register with game target set
        ULocalizationSettings::GetGameTargetSet()->TargetObjects.Add(LocTarget);
    }

    if (!LocTarget)
    {
        YS_ERR("Failed to create localisation target object for '%s'", *LocTargetName);
        return;
    }

    // Set up config
    FLocalizationTargetSettings& Settings = LocTarget->Settings;
    Settings.Name = LocTargetName;
    Settings.NativeCultureIndex = 0;
    Settings.SupportedCulturesStatistics.Reset();
    Settings.SupportedCulturesStatistics.Add({ProjectMeta->baseLanguage});
    for (auto Culture : Cultures)
    {
        if (Culture != ProjectMeta->baseLanguage)
        {
            Settings.SupportedCulturesStatistics.Add({Culture});
        }
    }
    Settings.CompileSettings.SkipSourceCheck = true;
    Settings.GatherFromPackages.IsEnabled = false;
    Settings.GatherFromMetaData.IsEnabled = false;
    Settings.GatherFromTextFiles.IsEnabled = false;

    // Generate config files (Config/Localization/MyTarget_*.ini)
    LocTarget->SaveConfig();
    LocalizationConfigurationScript::GenerateAllConfigFiles(LocTarget);

    // Set loading policy & register in DefaultEngine.ini
    SetLoadingPolicy(LocTarget, ELocalizationTargetLoadingPolicy::Game);

    // Notify parent of change, which triggers loading the target settings in relevant caches and updating editor config and DefaultEditor.ini
    FProperty* SettingsProp = LocTarget->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(ULocalizationTarget, Settings));
    FPropertyChangedEvent ChangeEvent(SettingsProp, EPropertyChangeType::ValueSet);
    LocTarget->PostEditChangeProperty(ChangeEvent);

    // Set up localisation data (keys, source text and translations) as a localisation target manifest (source) and archives (translations)

    FLocTextHelper LocTextHelper(LocTargetPath, FString::Printf(TEXT("%s.manifest"), *LocTargetName), FString::Printf(TEXT("%s.archive"), *LocTargetName), ProjectMeta->baseLanguage, Cultures, nullptr);

    FText OutError;
    // if (!LocTextHelper.LoadManifest(ELocTextHelperLoadFlags::LoadOrCreate, &OutError))
    if (!LocTextHelper.LoadAll(ELocTextHelperLoadFlags::Create, &OutError))
    {
        YS_ERR("Could not create manifest & archive files for localisation target '%s': %s", *LocTargetName, *OutError.ToString());
        return;
    }
    
    FLocKey NamespaceKey{LocTargetName};

    // Add lines to source text manifest
    for (auto Pair : CompilerOutput.strings())
    {
        FString LineID = FString(Pair.first.c_str());
        FString LineText = FString(Pair.second.text().c_str());
        FManifestContext ManifestContext{FLocKey(LineID)};
        ManifestContext.SourceLocation = YarnProject->GetPathName();
        LocTextHelper.AddSourceText(NamespaceKey, FLocItem(LineText), ManifestContext);
    }

    // Add translations to archives
    for (auto Loc : ProjectMeta->localisation)
    {
        FString Culture = Loc.Key;
        FYarnProjectLocalizationData LocData = Loc.Value;
        FString LocFile = FPaths::Combine(YarnProject->GetPath(), LocData.strings);
        FPaths::NormalizeFilename(LocFile);
        FPaths::CollapseRelativeDirectories(LocFile);
        FPaths::RemoveDuplicateSlashes(LocFile);
        FString LocFileData;

        if (!FFileHelper::LoadFileToString(LocFileData, *LocFile))
        {
            YS_WARN("Couldn't load strings file '%s' for locale '%s'", *LocFile, *Culture)
            continue;
        }

        const FCsvParser Parser(LocFileData);
        const FCsvParser::FRows& Rows = Parser.GetRows();
        if (Rows.Num() < 2)
        {
            YS_WARN("Empty strings file: %s", *LocFile)
            continue;
        }

        const auto& HeaderRow = Rows[0];
        TMap<FString, int32> HeaderMap;
        for (int32 I = 0; I < HeaderRow.Num(); ++I)
        {
            HeaderMap.Add(WCHAR_TO_TCHAR(HeaderRow[I]), I);
        }
        // Test for required columns
        if (!HeaderMap.Contains(TEXT("text")) || !HeaderMap.Contains(TEXT("id")))
        {
            YS_ERR("Missing required column 'id' or 'text' in strings file: %s", *LocFile)
            continue;
        }

        for (int32 I = 1; I < Rows.Num(); ++I)
        {
            const auto& Row = Rows[I];
            const FString& LineID = WCHAR_TO_TCHAR(Row[HeaderMap[TEXT("id")]]);
            const FString& LineText = WCHAR_TO_TCHAR(Row[HeaderMap[TEXT("text")]]);
            const FString& LineCharacter = HeaderMap.Contains(TEXT("character")) ? WCHAR_TO_TCHAR(Row[HeaderMap[TEXT("character")]]) : TEXT("");

            auto Source = LocTextHelper.FindSourceText(NamespaceKey, FLocKey(LineID));

            FLocItem SourceText = Source.IsValid() ? Source->Source : FLocItem();
            FLocItem Translation((!LineCharacter.IsEmpty() ? LineCharacter + TEXT(": ") : TEXT("")) + LineText);

            const auto LocEntry = MakeShared<FArchiveEntry>(NamespaceKey, FLocKey(LineID), SourceText, Translation, nullptr, false);

            LocTextHelper.AddTranslation(Culture, LocEntry);
        }
    }

    FText OutErr;
    if (!LocTextHelper.SaveAll(&OutErr))
    {
        YS_ERR("Could not save localization target text data '%s': %s", *LocTargetName, *OutErr.ToString());
        return;
    }

    // Update word count
    auto TimeStamp = FDateTime::UtcNow();
    FLocTextWordCounts WordCountReport = LocTextHelper.GetWordCountReport(TimeStamp);
    LocTextHelper.SaveWordCountReport(TimeStamp, LocalizationConfigurationScript::GetWordCountCSVPath(LocTarget));
    LocTarget->UpdateWordCountsFromCSV();

    CompileTexts(LocTarget);
    
    // Done!
    YS_LOG("Localisation target '%s' created successfully", *LocTargetName)
}


namespace
{
    struct FLocalizationTargetLoadingPolicyConfig
    {
        FLocalizationTargetLoadingPolicyConfig(ELocalizationTargetLoadingPolicy InLoadingPolicy, FString InSectionName, FString InKeyName, FString InConfigName, FString InConfigPath)
            : LoadingPolicy(InLoadingPolicy)
              , SectionName(MoveTemp(InSectionName))
              , KeyName(MoveTemp(InKeyName))
              , BaseConfigName(MoveTemp(InConfigName))
              , ConfigPath(MoveTemp(InConfigPath))
        {
            DefaultConfigName = FString::Printf(TEXT("Default%s"), *BaseConfigName);
            DefaultConfigFilePath = FString::Printf(TEXT("%s%s.ini"), *FPaths::SourceConfigDir(), *DefaultConfigName);
        }


        ELocalizationTargetLoadingPolicy LoadingPolicy;
        FString SectionName;
        FString KeyName;
        FString BaseConfigName;
        FString DefaultConfigName;
        FString DefaultConfigFilePath;
        FString ConfigPath;
    };


    static const TArray<FLocalizationTargetLoadingPolicyConfig> LoadingPolicyConfigs = []()
    {
        TArray<FLocalizationTargetLoadingPolicyConfig> Array;
        Array.Emplace(ELocalizationTargetLoadingPolicy::Always, TEXT("Internationalization"), TEXT("LocalizationPaths"), TEXT("Engine"), GEngineIni);
        Array.Emplace(ELocalizationTargetLoadingPolicy::Editor, TEXT("Internationalization"), TEXT("LocalizationPaths"), TEXT("Editor"), GEditorIni);
        Array.Emplace(ELocalizationTargetLoadingPolicy::Game, TEXT("Internationalization"), TEXT("LocalizationPaths"), TEXT("Game"), GGameIni);
        Array.Emplace(ELocalizationTargetLoadingPolicy::PropertyNames, TEXT("Internationalization"), TEXT("PropertyNameLocalizationPaths"), TEXT("Editor"), GEditorIni);
        Array.Emplace(ELocalizationTargetLoadingPolicy::ToolTips, TEXT("Internationalization"), TEXT("ToolTipLocalizationPaths"), TEXT("Editor"), GEditorIni);
        return Array;
    }();
}


// SetLoadingPolicy and FLocalizationTargetLoadingPolicyConfig are verbatim copies from LocalizationTargetDetailCustomization.cpp in the engine's LocalizationDashboard module.
void UYarnAssetFactory::SetLoadingPolicy(const TWeakObjectPtr<ULocalizationTarget> LocalizationTarget, const ELocalizationTargetLoadingPolicy LoadingPolicy) const
{
    const FString DataDirectory = LocalizationConfigurationScript::GetDataDirectory(LocalizationTarget.Get());
    const FString CollapsedDataDirectory = FConfigValue::CollapseValue(DataDirectory);

    enum class EDefaultConfigOperation : uint8
    {
        AddExclusion,
        RemoveExclusion,
        AddAddition,
        RemoveAddition,
    };

    ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

    auto ProcessDefaultConfigOperation = [&](const FLocalizationTargetLoadingPolicyConfig& LoadingPolicyConfig, const EDefaultConfigOperation OperationToPerform)
    {
        // We test the coalesced config data first, as we may be inheriting this target path from a base config.
        TArray<FString> LocalizationPaths;
        GConfig->GetArray(*LoadingPolicyConfig.SectionName, *LoadingPolicyConfig.KeyName, LocalizationPaths, LoadingPolicyConfig.ConfigPath);
        const bool bHasTargetPath = LocalizationPaths.Contains(DataDirectory);

        // Work out whether we need to do work with the default config...
        switch (OperationToPerform)
        {
        case EDefaultConfigOperation::AddExclusion:
        case EDefaultConfigOperation::RemoveAddition:
            if (!bHasTargetPath)
            {
                return; // No point removing a target that doesn't exist
            }
            break;
        case EDefaultConfigOperation::AddAddition:
        case EDefaultConfigOperation::RemoveExclusion:
            if (bHasTargetPath)
            {
                return; // No point adding a target that already exists
            }
            break;
        default:
            break;
        }

        FConfigFile IniFile;
        FConfigCacheIni::LoadLocalIniFile(IniFile, *LoadingPolicyConfig.DefaultConfigName, /*bIsBaseIniName*/false);

        FConfigSection* IniSection = IniFile.Find(*LoadingPolicyConfig.SectionName);
        if (!IniSection)
        {
            IniSection = &IniFile.Add(*LoadingPolicyConfig.SectionName);
        }

        switch (OperationToPerform)
        {
        case EDefaultConfigOperation::AddExclusion:
            IniSection->Add(*FString::Printf(TEXT("-%s"), *LoadingPolicyConfig.KeyName), FConfigValue(*CollapsedDataDirectory));
            break;
        case EDefaultConfigOperation::RemoveExclusion:
            IniSection->RemoveSingle(*FString::Printf(TEXT("-%s"), *LoadingPolicyConfig.KeyName), FConfigValue(*CollapsedDataDirectory));
            break;
        case EDefaultConfigOperation::AddAddition:
            IniSection->Add(*FString::Printf(TEXT("+%s"), *LoadingPolicyConfig.KeyName), FConfigValue(*CollapsedDataDirectory));
            break;
        case EDefaultConfigOperation::RemoveAddition:
            IniSection->RemoveSingle(*FString::Printf(TEXT("+%s"), *LoadingPolicyConfig.KeyName), FConfigValue(*CollapsedDataDirectory));
            break;
        default:
            break;
        }

        // Make sure the file is checked out (if needed).
        if (SourceControlProvider.IsEnabled())
        {
            FSourceControlStatePtr ConfigFileState = SourceControlProvider.GetState(LoadingPolicyConfig.DefaultConfigFilePath, EStateCacheUsage::Use);
            if (!ConfigFileState.IsValid() || ConfigFileState->IsUnknown())
            {
                ConfigFileState = SourceControlProvider.GetState(LoadingPolicyConfig.DefaultConfigFilePath, EStateCacheUsage::ForceUpdate);
            }
            if (ConfigFileState.IsValid() && ConfigFileState->IsSourceControlled() && !(ConfigFileState->IsCheckedOut() || ConfigFileState->IsAdded()) && ConfigFileState->CanCheckout())
            {
                SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), LoadingPolicyConfig.DefaultConfigFilePath);
            }
        }
        else
        {
            IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
            if (PlatformFile.FileExists(*LoadingPolicyConfig.DefaultConfigFilePath) && PlatformFile.IsReadOnly(*LoadingPolicyConfig.DefaultConfigFilePath))
            {
                PlatformFile.SetReadOnly(*LoadingPolicyConfig.DefaultConfigFilePath, false);
            }
        }

        // Write out the new config.
        IniFile.Dirty = true;
        IniFile.UpdateSections(*LoadingPolicyConfig.DefaultConfigFilePath);

        // Make sure to add the file now (if needed).
        if (SourceControlProvider.IsEnabled())
        {
            FSourceControlStatePtr ConfigFileState = SourceControlProvider.GetState(LoadingPolicyConfig.DefaultConfigFilePath, EStateCacheUsage::Use);
            if (ConfigFileState.IsValid() && !ConfigFileState->IsSourceControlled() && ConfigFileState->CanAdd())
            {
                SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), LoadingPolicyConfig.DefaultConfigFilePath);
            }
        }

        // Reload the updated file into the config system.
        FString FinalIniFileName;
        GConfig->LoadGlobalIniFile(FinalIniFileName, *LoadingPolicyConfig.BaseConfigName, nullptr, /*bForceReload*/true);
    };

    for (const FLocalizationTargetLoadingPolicyConfig& LoadingPolicyConfig : LoadingPolicyConfigs)
    {
        if (LoadingPolicyConfig.LoadingPolicy == LoadingPolicy)
        {
            // We need to remove any exclusions for this path, and add the path if needed.
            ProcessDefaultConfigOperation(LoadingPolicyConfig, EDefaultConfigOperation::RemoveExclusion);
            ProcessDefaultConfigOperation(LoadingPolicyConfig, EDefaultConfigOperation::AddAddition);
        }
        else
        {
            // We need to remove any additions for this path, and exclude the path is needed.
            ProcessDefaultConfigOperation(LoadingPolicyConfig, EDefaultConfigOperation::RemoveAddition);
            ProcessDefaultConfigOperation(LoadingPolicyConfig, EDefaultConfigOperation::AddExclusion);
        }
    }
}


void UYarnAssetFactory::CompileTexts(const ULocalizationTarget* LocalizationTarget)
{
    // This version launches the text compilation commandlet in a separate process.  To implement this directly instead, see GenerateTextLocalizationResourceCommandlet.cpp
    
    YS_LOG("Compiling texts for localization target %s...", *LocalizationTarget->Settings.Name);
    
    const FString ConfigFilePath = FPaths::Combine(FPaths::SourceConfigDir(), TEXT("Localization"), FString::Printf(TEXT("%s_Compile.ini"), *LocalizationTarget->Settings.Name));

    TSharedPtr<FLocalizationCommandletProcess> CommandletProcessHandle = FLocalizationCommandletProcess::Execute(ConfigFilePath, !LocalizationTarget->IsMemberOfEngineTargetSet());

    if (!CommandletProcessHandle.IsValid())
    {
        YS_ERR("Failed to launch GenerateTextLocalizationResource commandlet");
        return;
    }

    FString Out;
    // Wait for the process to complete
    for(;;)
    {
        // Read from pipe.
        const FString PipeString = FPlatformProcess::ReadPipe(CommandletProcessHandle->GetReadPipe());

        // Process buffer.
        if (!PipeString.IsEmpty())
        {
            Out += PipeString;
        }

        // If the process isn't running and there's no data in the pipe, we're done.
        if (!FPlatformProcess::IsProcRunning(CommandletProcessHandle->GetHandle()) && PipeString.IsEmpty())
        {
            break;
        }

        // Sleep.
        FPlatformProcess::Sleep(0.0f);
    }
    
    YS_LOG("GenerateTextLocalizationResourceCommandlet output log:\n%s", *Out)
    
    int32 ReturnCode = 0;
    if (!FPlatformProcess::GetProcReturnCode(CommandletProcessHandle->GetHandle(), &ReturnCode) || ReturnCode != 0)
    {
        YS_ERR("Failed to compile texts for %s", *LocalizationTarget->Settings.Name);
        return;
    }
    
    YS_LOG("%s texts compiled successfully", *LocalizationTarget->Settings.Name);
}

