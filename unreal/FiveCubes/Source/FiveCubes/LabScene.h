#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PanelProgress.h"
#include "LabScene.generated.h"
class UMaterialInterface;
class IWebSocket; class FJsonObject; class UStaticMeshComponent; class UStaticMesh;
class UWidgetComponent; class ACameraActor; class UMaterialInstanceDynamic;
UCLASS()
class FIVECUBES_API ALabScene : public AActor {
    GENERATED_BODY()
public:
    ALabScene();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void PressControl(const FString& Id,const FVector& Point);
    void DragControl(const FVector& Point);
    void ReleaseControl();
    void ResetExercise();
    FString Goal() const;
    FString ProgressText() const;
private:
    UPROPERTY() TObjectPtr<UMaterialInterface> Surface;
    FPanelProgress Progress;
    TArray<FString> StageIds,Targets,Titles,Goals,CatalogIds;
    TMap<FString,int32> CatalogSections;
    UPROPERTY() TArray<TObjectPtr<UWidgetComponent>> Labels;
    UPROPERTY() TMap<FString,TObjectPtr<UStaticMeshComponent>> Keys;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> HighlightMaterial;
    UPROPERTY() TArray<TObjectPtr<UWidgetComponent>> Indicators;
    UPROPERTY() TObjectPtr<UWidgetComponent> Display;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Knob;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Pointer;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Slider;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Lever;
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BaseMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DarkMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> AccentMaterial;
    TSharedPtr<IWebSocket> Socket;
    FString SessionId,Token,SceneVersion,Url,ConnectionFile,Dragging,HighlightText,HighlightTarget;
    FTimerHandle RetryTimer;
    uint64 StateSeq=0;
    int HighlightIndex=-1;
    double HighlightStartedAt=0;
    bool bHighlightLit=false;
    void ApplyHighlightPhase(bool Lit);
    float LastAngle=0, SliderPreview=0;
    bool bEnding=false,bCameraSet=false,bStateDirty=false,bConfigured=false;
    double LastStateAt=0;
    void Connect(); void Retry(); void Receive(const FString& Raw);
    void Send(const TSharedRef<FJsonObject>& Message); void SendState(); void ClearHighlight();
    void Refresh(); void ActionChanged(int PreviousStep);
    UStaticMeshComponent* Box(FString Name,FVector Position,FVector Scale,UMaterialInstanceDynamic* Material,const FString& Control=TEXT(""));
    UWidgetComponent* Label(FString Text,FVector Position,FVector2D Size,float Scale=0.32);
};
