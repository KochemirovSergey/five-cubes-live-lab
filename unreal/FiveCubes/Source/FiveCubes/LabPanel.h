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
    virtual void NativeTick(const FGeometry&, float) override;
private:
    UPROPERTY() TObjectPtr<UTextBlock> StateText;
    UPROPERTY() TObjectPtr<UTextBlock> Dialogue;
    UPROPERTY() TObjectPtr<UTextBlock> ResultText;
    UPROPERTY() TObjectPtr<UButton> StartButton;
    UFUNCTION() void Start(); UFUNCTION() void Stop(); UFUNCTION() void Clear();
    UFUNCTION() void One(); UFUNCTION() void Two(); UFUNCTION() void Three(); UFUNCTION() void Four(); UFUNCTION() void Five();
};
