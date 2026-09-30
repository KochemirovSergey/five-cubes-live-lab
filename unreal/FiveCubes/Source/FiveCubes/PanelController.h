#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PanelController.generated.h"
class ALabScene;
class IInputProcessor;
UCLASS()
class FIVECUBES_API APanelController : public APlayerController {
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaSeconds) override;
    void Press(FVector2D Position);
    void Release();
    void Drag(FVector2D Position);
private:
    TSharedPtr<IInputProcessor> InputProbe;
    TWeakObjectPtr<ALabScene> DragScene;
    bool PanelPoint(FVector2D Position,FVector& Point) const;
};
