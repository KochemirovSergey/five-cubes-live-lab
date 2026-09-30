#include "FiveCubesGameMode.h"
#include "LabScene.h"
#include "VoiceConsole.h"
#include "Engine/World.h"
#include "GameFramework/SpectatorPawn.h"

AFiveCubesGameMode::AFiveCubesGameMode() { DefaultPawnClass = ASpectatorPawn::StaticClass(); }
void AFiveCubesGameMode::BeginPlay()
{
    Super::BeginPlay();
    GetWorld()->SpawnActor<AVoiceConsole>();
    GetWorld()->SpawnActor<ALabScene>();
}
