#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AudioCaptureCore.h"
#include "PcmResampler.h"
#include "VoiceTiming.h"
#include "HAL/PlatformProcess.h"
#include "VoiceConsole.generated.h"
class IWebSocket;
class ULabPlaybackWave;
class UAudioComponent;
class ULabPanel;
UCLASS()
class FIVECUBES_API AVoiceConsole : public AActor
{
    GENERATED_BODY()
public:
    AVoiceConsole();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void StartVoice();
    void StopVoice(bool Clear=true,const FString& Reason=TEXT("user"));
    void ClearHighlight();
    static FString ConnectionPath;
    FString Status = TEXT("Запуск сервера…"), Transcript, ToolStatus;
    bool bConnected=false, bReady=false, bStarting=false;
    float Level=0;
private:
    UPROPERTY() TObjectPtr<ULabPlaybackWave> Wave;
    UPROPERTY() TObjectPtr<UAudioComponent> Speaker;
    UPROPERTY() TObjectPtr<ULabPanel> Panel;
    TSharedPtr<IWebSocket> Socket;
    TUniquePtr<Audio::FAudioCapture> Capture;
    FProcHandle Server;
    FString Session, Token, LastSpeaker;
    FString StopMessage;
    FCriticalSection AudioLock;
    FLabPcmResampler Resampler;
    FVoiceTiming Timing;
    double LastMeasuredEnd=0;
    void Metric(const FString& Kind,double Milliseconds,bool Success=true);
    double LastCapture=0;
    uint32 PermissionGeneration=0;
    TArray<uint8> AudioFragments;
    bool bSceneReady=false, bEnding=false;
    double NextConnect=0;
    bool bPanelInputReady=false;
    void Connect();
    void Receive(const FString& Raw);
    void Send(const FString& Type);
    void StopAudio();
    void OpenMicrophone();
};
