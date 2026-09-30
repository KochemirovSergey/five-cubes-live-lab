#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LabPanel.generated.h"
class AVoiceConsole;
class UTextBlock;
class UButton;
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
    virtual void NativeTick(const FGeometry&, float) override;
private:
    UPROPERTY() TObjectPtr<UTextBlock> StateText;
    UPROPERTY() TObjectPtr<UTextBlock> Dialogue;
    UPROPERTY() TObjectPtr<UTextBlock> ResultText;
    UPROPERTY() TObjectPtr<UButton> StartButton;
    UFUNCTION() void Start(); UFUNCTION() void Stop(); UFUNCTION() void Clear();
    UFUNCTION() void Reset();
    UPROPERTY() TObjectPtr<UTextBlock> GoalText;
};
