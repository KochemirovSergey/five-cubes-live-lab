#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LabScene.generated.h"

class IWebSocket;
class FJsonObject;
class UStaticMeshComponent;
class UStaticMesh;
class UWidgetComponent;
class ACameraActor;

UCLASS()
class FIVECUBES_API ALabScene : public AActor
{
    GENERATED_BODY()
public:
    ALabScene();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Cubes;
    UPROPERTY() TArray<TObjectPtr<UWidgetComponent>> Labels;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Frame;
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
    TSharedPtr<IWebSocket> Socket;
    FString SessionId, Token, SceneVersion, Url, ConnectionFile;
    FTimerHandle RetryTimer;
    bool bEnding = false;
    bool bCameraSet = false;
    void Connect();
    void Retry();
    void Receive(const FString& Raw);
    void Send(const TSharedRef<FJsonObject>& Message);
    void SendState();
    void ClearHighlight();
    UStaticMeshComponent* AddBox(FName Name, FVector Position, FVector Scale);
};
