#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "LabNotebook.generated.h"
UCLASS()
class FIVECUBES_API ULabNotebook : public UWidget {
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
