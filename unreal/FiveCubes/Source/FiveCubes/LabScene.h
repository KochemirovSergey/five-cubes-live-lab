#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PanelProgress.h"
#include "OberbeckModel.h"
#include "OberbeckLab.h"
#include "OberbeckGuidance.h"
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
    FString ActiveLab=TEXT("menu"), Notice;
    bool bConfirmMenu=false;
    void SelectLab(const FString& Id);
    void RequestMenu();
    void ExperimentAction(const FString& Id);
    void UpdateExperimentClock();
    FString ExperimentText() const;
    bool IsPaused() const {return Experiment.Paused;}
    bool HasWork() const;
    float InteractionX() const {return ActiveLab==TEXT("oberbeck")?20.f:45.f;}
    FString Goal() const;
    FString ProgressText() const;
    bool FullMode=false, ShowForces=false, Replay=false;
    int CalculationTask=0;
    FString SelectedField, LastInput, LastUnit, LastAction;
    TSet<FString> VisibleUiTargets;
    UPROPERTY() TArray<TObjectPtr<UWidgetComponent>> MassLabels;
    static FVector StoragePosition(int I){return FVector(20,66-I*10,14);}
    FObHint NextHint()const;
    FString TargetVisibilityError(const FString& Id)const;
    FString ActiveHighlight()const{return HighlightTarget;}
    FString ActiveHighlightCaption()const{return HighlightText;}
    int NotebookView=0, Repeats=3, ReplaySeries=0, CompareSeries=-1;
    double Caliper=0, RCursor=0, HCursor=0, ReplayTime=0, ReplaySpeed=1;
    FOberbeckLab Notebook;
    FString SceneIdentity()const {return SceneVersion;}
    const FOberbeckModel& Physics()const {return Experiment;}
    FString FullHighlight()const {return bHighlightLit?HighlightTarget:FString();}
    void SelectFull(bool Continue);
    void FullAction(const FString& Id,const FString& Text=TEXT(""),const FString& Unit=TEXT(""));
    void FullChanged();
    FString FullStatus()const;
    bool SaveLab(); bool LoadLab(); void ExportLab();
    TSharedRef<FJsonObject> LabDocument()const;
    TSharedRef<FJsonObject> FullPublicState()const;
    FString SaveDirectory()const;
    void SetInstrument(const FString& Id,double Value);
    void BuildFullTools(); void RefreshFullTools();
    bool bSaveBlocked=false;
    bool RunFullSelfTest();
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> FullParts;

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
    FOberbeckModel Experiment;
    double LastClock=0;
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> PanelParts;
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> OberbeckParts;
    UPROPERTY() TMap<FString,TObjectPtr<UStaticMeshComponent>> OberbeckMeshes;
    UPROPERTY() TMap<FString,TObjectPtr<UMaterialInterface>> OberbeckOriginal;
    UPROPERTY() TObjectPtr<UWidgetComponent> OberbeckCaption;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Rope;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> SelectionMark;
    bool bOberbeckValid=false;
    void BuildOberbeck(); void RefreshOberbeck();
    void LoadCatalog(const FString& Prefix);
    int Step() const;
    bool Done() const;
    void Connect(); void Retry(); void Receive(const FString& Raw);
    void Send(const TSharedRef<FJsonObject>& Message); void SendState(); void ClearHighlight();
    void Refresh(); void ActionChanged(int PreviousStep);
    UStaticMeshComponent* Box(FString Name,FVector Position,FVector Scale,UMaterialInstanceDynamic* Material,const FString& Control=TEXT(""));
    UWidgetComponent* Label(FString Text,FVector Position,FVector2D Size,float Scale=0.32);
};
