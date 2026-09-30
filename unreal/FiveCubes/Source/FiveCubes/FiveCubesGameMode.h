#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FiveCubesGameMode.generated.h"

UCLASS()
class FIVECUBES_API AFiveCubesGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AFiveCubesGameMode();
    virtual void BeginPlay() override;
};
