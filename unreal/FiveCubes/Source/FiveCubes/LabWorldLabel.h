#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LabWorldLabel.generated.h"
class UTextBlock;
UCLASS()
class FIVECUBES_API ULabWorldLabel : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    void Ink(FLinearColor Color);
    void Caption(const FString& Value, bool Highlight);
private:
    UPROPERTY() TObjectPtr<UTextBlock> Text;
};
