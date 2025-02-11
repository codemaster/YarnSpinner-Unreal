// Fill out your copyright notice in the Description page of Project Settings.


#include "DialogueRunner.h"

#include "Line.h"
#include "Option.h"
#include "YarnSubsystem.h"
#include "Misc/YSLogging.h"

THIRD_PARTY_INCLUDES_START
#include "YarnSpinnerCore/VirtualMachine.h"
THIRD_PARTY_INCLUDES_END


// Sets default values
ADialogueRunner::ADialogueRunner()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;
}

void ADialogueRunner::BeginPlay()
{
    Super::BeginPlay();

    if (!YarnProject)
    {
        UE_LOG(LogYarnSpinner, Error, TEXT("DialogueRunner can't initialize, because it doesn't have a Yarn Asset."));
        return;
    }
    
    const TSharedPtr<Yarn::Program> Program = YarnProject->GetProgram();
    if (!Program.IsValid())
    {
        UE_LOG(LogYarnSpinner, Error, TEXT("DialogueRunner can't initialize, because its Yarn Asset failed to load."));
        return;
    }

    // Create the Library
    Library = MakeUnique<Yarn::Library>();

    // Create the VirtualMachine, supplying it with the loaded Program and
    // configuring it to use our library, plus use this ADialogueRunner as the
    // logger and the variable storage
    VirtualMachine = MakeUnique<Yarn::VirtualMachine>(Program.ToSharedRef(), *Library, *this);

    VirtualMachine->OnLine.AddLambda([this](const Yarn::Line& Line)
    {
        YS_LOG("Received line %s", *Line.LineID.ToString());

        // Get the Yarn line struct, and make a ULine out of it to use
        ULine* LineObject = NewObject<ULine>(this);
        LineObject->LineID = Line.LineID;

        UpdateDisplayTextForLine(LineObject, Line);

        const TArray<TSoftObjectPtr<UObject>> LineAssets = YarnProject->GetLineAssets(LineObject->LineID);
        YS_LOG_FUNC("Got %d line assets for line '%s'", LineAssets.Num(), *LineObject->LineID.ToString())

        OnRunLine(LineObject, LineAssets);
    });

    VirtualMachine->OnOptions.AddLambda([this](const Yarn::OptionSet& OptionSet)
    {
        YS_LOG("Received %llu options", OptionSet.Options.Num());

        // Build a TArray for every option in this OptionSet
        TArray<UOption*> Options;

        for (const Yarn::Option& Option : OptionSet.Options)
        {
            YS_LOG("- %i: %s", Option.ID, *Option.Line.LineID.ToString());

            UOption* Opt = NewObject<UOption>(this);
            Opt->OptionID = Option.ID;

            Opt->Line = NewObject<ULine>(Opt);
            Opt->Line->LineID = Option.Line.LineID;

            UpdateDisplayTextForLine(Opt->Line, Option.Line);

            Opt->bIsAvailable = Option.IsAvailable;

            Opt->SourceDialogueRunner = this;

            Options.Add(Opt);
        }

        OnRunOptions(Options);
    });

    VirtualMachine->OnCheckFunctionExist.BindLambda([this](const FString& FunctionName) -> bool
    {
        return YarnSubsystem()->GetYarnLibraryRegistry()->HasFunction(FName(*FunctionName));
    });

    VirtualMachine->OnGetFunctionParamNum.BindLambda([this](const FString& FunctionName) -> int
    {
        return YarnSubsystem()->GetYarnLibraryRegistry()->GetExpectedFunctionParamCount(FName(*FunctionName));
    });

    VirtualMachine->OnCallFunction.BindLambda([this](const FString& FunctionName, const TArray<Yarn::FValue>& Parameters) -> Yarn::FValue
    {
        return YarnSubsystem()->GetYarnLibraryRegistry()->CallFunction(
            FName(*FunctionName),
            Parameters
        );
    });

    VirtualMachine->OnCommand.AddLambda([this](const Yarn::Command& Command)
    {
        YS_LOG("Received command \"%s\"", *Command.Text);

        const FString& CommandText = Command.Text;

        TArray<FString> CommandElements;
        CommandText.ParseIntoArray(CommandElements, TEXT(" "));

        if (CommandElements.Num() == 0)
        {
            UE_LOG(LogYarnSpinner, Error, TEXT("Command received, but was unable to parse it."));
            OnRunCommand(FString("(unknown)"), TArray<FString>());
            return;
        }

        const FName CommandName = FName(CommandElements[0]);
        CommandElements.RemoveAt(0);

        const UYarnLibraryRegistry* const Lib = YarnSubsystem()->GetYarnLibraryRegistry();

        if (Lib->HasCommand(CommandName))
        {
            return Lib->CallCommand(
                CommandName,
                this,
                CommandElements
            );
        }

        // Haven't handled the function yet, so call the DialogueRunner's handler
        OnRunCommand(CommandName.ToString(), CommandElements);
    });

    VirtualMachine->OnNodeStart.AddLambda( [this](const FString& NodeName)
    {
        UE_LOG(LogYarnSpinner, Log, TEXT("Received node start \"%s\""), *NodeName);
    });

    VirtualMachine->OnNodeComplete.AddLambda([this](const FString& NodeName)
    {
        UE_LOG(LogYarnSpinner, Log, TEXT("Received node complete \"%s\""), *NodeName);
    });

    VirtualMachine->OnDialogueComplete.AddLambda([this]()
    {
        UE_LOG(LogYarnSpinner, Log, TEXT("Received dialogue complete"));
        OnDialogueEnded();
    });
}


void ADialogueRunner::OnDialogueStarted_Implementation()
{
    // default = no-op
}


void ADialogueRunner::OnDialogueEnded_Implementation()
{
    // default = no-op
}


void ADialogueRunner::OnRunLine_Implementation(ULine* Line, const TArray<TSoftObjectPtr<UObject>>& LineAssets)
{
    // default = log and immediately continue
    UE_LOG(LogYarnSpinner, Warning, TEXT("DialogueRunner received line with ID \"%s\". Implement OnRunLine to customise its behaviour."), *Line->LineID.ToString());
    ContinueDialogue();
}


void ADialogueRunner::OnRunOptions_Implementation(const TArray<class UOption*>& Options)
{
    // default = log and choose the first option
    UE_LOG(LogYarnSpinner, Warning, TEXT("DialogueRunner received %i options. Choosing the first one by default. Implement OnRunOptions to customise its behaviour."), Options.Num());

    SelectOption(Options[0]);
}


void ADialogueRunner::OnRunCommand_Implementation(const FString& Command, const TArray<FString>& Parameters)
{
    // default = no-op
    UE_LOG(LogYarnSpinner, Warning, TEXT("DialogueRunner received command \"%s\". Implement OnRunCommand to customise its behaviour."), *Command);
    ContinueDialogue();
}


/** Starts running dialogue from the given node name. */
void ADialogueRunner::StartDialogue(FName NodeName)
{
    if (VirtualMachine.IsValid() == false)
    {
        UE_LOG(LogYarnSpinner, Error, TEXT("DialogueRunner can't start node %s, because it failed to load a Yarn asset."), *NodeName.ToString());
        return;
    }

    bool bNodeSelected = VirtualMachine->SetNode(NodeName.ToString());

    if (bNodeSelected)
    {
        OnDialogueStarted();
        ContinueDialogue();
    }
    else
    {
        UE_LOG(LogYarnSpinner, Error, TEXT("DialogueRunner can't start node %s, because a node with that name was not found."), *NodeName.ToString());
        return;
    }
}


/** Continues running the current dialogue, producing either lines, options, commands, or a dialogue-end signal. */
void ADialogueRunner::ContinueDialogue()
{
    YS_LOG_FUNCSIG

    if (VirtualMachine->GetCurrentExecutionState() == Yarn::VirtualMachine::ExecutionState::ERROR)
    {
        UE_LOG(LogYarnSpinner, Error, TEXT("VirtualMachine is in an error state and cannot continue running."));
        return;
    }

    VirtualMachine->Continue();

    Yarn::VirtualMachine::ExecutionState State = VirtualMachine->GetCurrentExecutionState();

    if (State == Yarn::VirtualMachine::ExecutionState::ERROR)
    {
        UE_LOG(LogYarnSpinner, Error, TEXT("VirtualMachine encountered an error."));
        return;
    }
}


/** Indicates to the dialogue runner that an option was selected. */
void ADialogueRunner::SelectOption(UOption* Option)
{
    Yarn::VirtualMachine::ExecutionState State = this->VirtualMachine->GetCurrentExecutionState();

    if (State != Yarn::VirtualMachine::ExecutionState::WAITING_ON_OPTION_SELECTION)
    {
        UE_LOG(LogYarnSpinner, Error, TEXT("Dialogue Runner received a call to SelectOption but it wasn't expecting a selection!"));
        return;
    }

    UE_LOG(LogYarnSpinner, Log, TEXT("Selected option %i (%s)"), Option->OptionID, *Option->Line->LineID.ToString());

    VirtualMachine->SetSelectedOption(Option->OptionID);

    if (bRunLinesForSelectedOptions)
    {
        const TArray<TSoftObjectPtr<UObject>> LineAssets = YarnProject->GetLineAssets(Option->Line->LineID);
        YS_LOG_FUNC("Got %d line assets for line '%s'", LineAssets.Num(), *Option->Line->LineID.ToString())

        OnRunLine(Option->Line, LineAssets);
    }
    else
    {
        ContinueDialogue();
    }
}

void ADialogueRunner::SetValue(const FString& Name, bool bValue)
{
    YS_LOG("Setting variable %s to bool %i", *Name, bValue);
    YarnSubsystem()->SetValue(Name, bValue);
}


void ADialogueRunner::SetValue(const FString& Name, float Value)
{
    YS_LOG("Setting variable %s to float %f", *Name, Value);
    YarnSubsystem()->SetValue(Name, Value);
}


void ADialogueRunner::SetValue(const FString& Name, const FString& Value)
{
    YS_LOG("Setting variable %s to string %s", *Name, *Value);
    YarnSubsystem()->SetValue(Name, Value);
}


bool ADialogueRunner::HasValue(const FString& Name)
{
    return YarnSubsystem()->HasValue(Name);
}


Yarn::FValue ADialogueRunner::GetValue(const FString& Name)
{
    Yarn::FValue Value = YarnSubsystem()->GetValue(Name);
    YS_LOG("Retrieving variable %s with value %s", *Name, *Value.ConvertToString());
    return Value;
}


void ADialogueRunner::ClearValue(const FString& Name)
{
    YS_LOG("Clearing variable %s", *Name);
    YarnSubsystem()->ClearValue(Name);
}


UYarnSubsystem* ADialogueRunner::YarnSubsystem() const
{
    if (!GetGameInstance())
    {
        YS_WARN("Could not retrieve YarnSubsystem because GetGameInstance() returned null")
        return nullptr;
    }
    return GetGameInstance()->GetSubsystem<UYarnSubsystem>();
}


void ADialogueRunner::UpdateDisplayTextForLine(ULine* const Line, const Yarn::Line& YarnLine) const
{
    const FName LineID = YarnLine.LineID;

    // This assumes that we only ever care about lines that actually exist in .yarn files (rather than allowing extra lines in .csv files)
    if (!IsValid(YarnProject) || !YarnProject->HasLine(LineID))
    {
        Line->DisplayText = FText::FromString(TEXT("(missing line!)"));
        return;
    }

    // Try to find the localized string. If not, use the non-localized one from the project itself.
    const FTextConstDisplayStringPtr FindDisplayStr = FTextLocalizationManager::Get()
        .FindDisplayString(YarnProject->GetName(), LineID.ToString());
    const FTextFormat FormatText = FText::FromString(FindDisplayStr.IsValid() ? *FindDisplayStr : YarnProject->GetLine(LineID));

    // Log if we weren't able to find a localized version
    if (!FindDisplayStr.IsValid())
    {
        YS_LOG("Using non-localized version of line with ID '%s' because a localized version was not found.", *LineID.ToString());
    }

    // Apply Substitutions
    const FText TextWithSubstitutions = FText::Format(FormatText, YarnLine.Substitutions);

    // TODO: add support for markup & context (speaker, target)

    YS_LOG_FUNC("Setting line %s to display text '%s'", *LineID.ToString(), *TextWithSubstitutions.ToString())

    Line->DisplayText = TextWithSubstitutions;
}
