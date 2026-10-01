#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LabPanel.generated.h"
class AVoiceConsole;
class UTextBlock;
class UButton;
class UBorder;
class UVerticalBox;
class ULabNotebook;
UCLASS()
class FIVECUBES_API ULabPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<AVoiceConsole> Console;
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseMove(const FGeometry&,const FPointerEvent&) override;
    virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent&) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry&,const FKeyEvent&) override;
    virtual void NativeTick(const FGeometry&, float) override;
private:
    UPROPERTY() TObjectPtr<UTextBlock> StateText;
    UPROPERTY() TObjectPtr<UTextBlock> Dialogue;
    UPROPERTY() TObjectPtr<UTextBlock> ResultText;
    UPROPERTY() TObjectPtr<UButton> StartButton;
    UFUNCTION() void Start(); UFUNCTION() void Stop(); UFUNCTION() void Clear();
    UFUNCTION() void Reset();
    UPROPERTY() TObjectPtr<UTextBlock> GoalText;
    UPROPERTY() TObjectPtr<UBorder> MenuBox;
    UPROPERTY() TObjectPtr<UBorder> ExperimentBox;
    UPROPERTY() TObjectPtr<UTextBlock> ExperimentReadout;
    UPROPERTY() TObjectPtr<UTextBlock> MenuCaption;
    UPROPERTY() TObjectPtr<UButton> MenuButton;
    UPROPERTY() TObjectPtr<UButton> ResumeButton;
    UPROPERTY() TObjectPtr<UButton> CancelMenuButton;
    UPROPERTY() TObjectPtr<UBorder> NotebookBox;
    UFUNCTION() void ChooseFull(); UFUNCTION() void ContinueFull();
    UFUNCTION() void ChoosePanel(); UFUNCTION() void ChooseOberbeck();
    UFUNCTION() void Menu(); UFUNCTION() void CancelMenu();
    UFUNCTION() void NewTrial(); UFUNCTION() void Resume();

};
