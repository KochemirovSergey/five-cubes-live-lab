#include "FiveCubesGameMode.h"
#include "LabScene.h"
#include "PanelController.h"
#include "VoiceConsole.h"
#include "Engine/World.h"
#include "GameFramework/SpectatorPawn.h"

AFiveCubesGameMode::AFiveCubesGameMode() { DefaultPawnClass = nullptr; PlayerControllerClass=APanelController::StaticClass(); }
void AFiveCubesGameMode::BeginPlay()
{
    Super::BeginPlay();
    GetWorld()->SpawnActor<AVoiceConsole>();
    GetWorld()->SpawnActor<ALabScene>();
}
