// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YarnProject.h"

THIRD_PARTY_INCLUDES_START
#include "YarnSpinnerCore/VirtualMachine.h"
#include "YarnSpinnerCore/Library.h"
#include "YarnSpinnerCore/Common.h"
THIRD_PARTY_INCLUDES_END

#include "DialogueRunner.generated.h"

DECLARE_DELEGATE(FYarnDialogueRunnerContinueDelegate);

UCLASS(BlueprintType)
class YARNSPINNER_API ADialogueRunner : public AActor, public Yarn::IVariableStorage
{
    GENERATED_BODY()
    
public:
    // Sets default values for this actor's properties
    ADialogueRunner();

protected:
    virtual void BeginPlay() override;
    
public:
    UFUNCTION(BlueprintNativeEvent, Category="Dialogue Runner")
    void OnDialogueStarted();
    
    UFUNCTION(BlueprintNativeEvent, Category="Dialogue Runner")
    void OnDialogueEnded();
    
    UFUNCTION(BlueprintNativeEvent, Category="Dialogue Runner")
    void OnRunLine(class ULine* Line, const TArray<TSoftObjectPtr<UObject>>& LineAssets);

    UFUNCTION(BlueprintNativeEvent, Category="Dialogue Runner")
    void OnRunOptions(const TArray<class UOption*>& Options);

    UFUNCTION(BlueprintNativeEvent, Category="Dialogue Runner")
    void OnRunCommand(const FString& Command, const TArray<FString>& Parameters);
    
    UFUNCTION(BlueprintCallable, Category="Dialogue Runner")
    void StartDialogue(FName NodeName);
    
    UFUNCTION(BlueprintCallable, Category="Dialogue Runner")
    void ContinueDialogue();

    // TODO: add StopDialogue() blueprint callback
    
    UFUNCTION(BlueprintCallable, Category="Dialogue Runner")
    void SelectOption(UOption* Option);
    
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Dialogue Runner")
    TObjectPtr<UYarnProject> YarnProject = nullptr;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Dialogue Runner")
    bool bRunLinesForSelectedOptions = true;

private:
    TUniquePtr<Yarn::VirtualMachine> VirtualMachine;

    TUniquePtr<Yarn::Library> Library;

    FYarnDialogueRunnerContinueDelegate ContinueDelegate;

    // IVariableStorage
    virtual void SetValue(const FString& Name, bool bValue) override;
    virtual void SetValue(const FString& Name, float Value) override;
    virtual void SetValue(const FString& Name, const FString& Value) override;

    virtual bool HasValue(const FString& Name) override;
    virtual Yarn::FValue GetValue(const FString& Name) override;

    virtual void ClearValue(const FString& Name) override;

    UPROPERTY()
    FString Blah;

    class UYarnSubsystem* YarnSubsystem() const;
    
    void UpdateDisplayTextForLine(ULine* const Line, const Yarn::Line& YarnLine) const;
};
