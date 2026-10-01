#include "LabScene.h"
#include "VoiceConsole.h"
#include "LabWorldLabel.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

int ALabScene::Step()const{return ActiveLab==TEXT("oberbeck")?(FullMode?Notebook.Task():Experiment.Step):ActiveLab==TEXT("menu")?0:Progress.Step;}
bool ALabScene::Done()const{return ActiveLab==TEXT("oberbeck")?(FullMode?Notebook.Task()==5:Experiment.Step==2):ActiveLab==TEXT("menu")?false:Progress.Done();}
bool ALabScene::HasWork()const{return ActiveLab==TEXT("oberbeck")?(Experiment.H>0||Experiment.Step>0||Experiment.TimerUsed):ActiveLab==TEXT("training_panel")&&(Progress.Step>0||Progress.Angle!=0);}
void ALabScene::LoadCatalog(const FString& Prefix){
    StageIds.Empty();Targets.Empty();Titles.Empty();Goals.Empty();CatalogIds.Empty();CatalogSections.Empty();
    FString Raw;TSharedPtr<FJsonObject> Spec;
    if(!FFileHelper::LoadFileToString(Raw,*(FPaths::ProjectContentDir()/TEXT("Lab/")/(Prefix+TEXT("scenario.json"))))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Spec))return;
    for(auto& V:Spec->GetArrayField(TEXT("steps"))){auto O=V->AsObject();StageIds.Add(O->GetStringField(TEXT("id")));Targets.Add(O->GetStringField(TEXT("target")));Titles.Add(O->GetStringField(TEXT("title")));Goals.Add(O->GetStringField(TEXT("goal")));}
    if(!FFileHelper::LoadFileToString(Raw,*(FPaths::ProjectContentDir()/TEXT("Lab/")/(Prefix+TEXT("targets.json"))))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Spec))return;
    const TArray<FString> Sections={TEXT("rotary"),TEXT("slider"),TEXT("keypad"),TEXT("toggle")};
    for(auto& V:Spec->GetArrayField(TEXT("targets"))){auto O=V->AsObject();const FString Id=O->GetStringField(TEXT("id"));CatalogIds.Add(Id);CatalogSections.Add(Id,Prefix.IsEmpty()?Sections.IndexOfByKey(O->GetStringField(TEXT("section"))):0);}
}
void ALabScene::SelectLab(const FString& Id){
    if(Id!=TEXT("menu")&&Id!=TEXT("training_panel")&&Id!=TEXT("oberbeck"))return;
    if(Id==TEXT("oberbeck")&&!bOberbeckValid){Notice=TEXT("Неверный профиль физики; запуск Обербека заблокирован.");return;}
    ClearHighlight();Dragging.Empty();bConfirmMenu=false;Notice.Empty();
    if(auto* Voice=Cast<AVoiceConsole>(UGameplayStatics::GetActorOfClass(this,AVoiceConsole::StaticClass()))){Voice->StopVoice(false,TEXT("scene_changed"));Voice->ToolStatus.Empty();Voice->Transcript.Empty();}
    ActiveLab=Id;FullMode=false;Replay=false;
    auto Visibility=[](const TArray<TObjectPtr<USceneComponent>>& Parts,bool Show){for(auto C:Parts){C->SetVisibility(Show);if(auto* M=Cast<UStaticMeshComponent>(C))M->SetCollisionEnabled(Show&&M->ComponentTags.Num()?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);}};
    Visibility(PanelParts,Id==TEXT("training_panel"));Visibility(OberbeckParts,Id==TEXT("oberbeck"));
    if(Id==TEXT("menu")){StageIds={TEXT("select")};Targets.Empty();Titles.Empty();Goals={TEXT("Выберите лабораторную")};CatalogIds.Empty();CatalogSections.Empty();}
    else LoadCatalog(Id==TEXT("oberbeck")?TEXT("oberbeck-"):TEXT(""));
    Progress.Reset();SliderPreview=0;Experiment.Reset();LastClock=FPlatformTime::Seconds();
    SceneVersion=FGuid::NewGuid().ToString();StateSeq=0;
    if(Id==TEXT("oberbeck")){Camera->SetActorLocation(FVector(350,22,60));Camera->SetActorRotation(FRotator(-3,180,0));Camera->GetCameraComponent()->SetFieldOfView(55);RefreshOberbeck();}
    else {Camera->SetActorLocation(FVector(1150,0,70));Camera->SetActorRotation(FRotator(0,180,0));Camera->GetCameraComponent()->SetFieldOfView(60);if(Id==TEXT("training_panel"))Refresh();}
    bCameraSet=false;SendState();
}
void ALabScene::RequestMenu(){if(ActiveLab==TEXT("menu"))return;if(HasWork()&&!bConfirmMenu){bConfirmMenu=true;return;}SelectLab(TEXT("menu"));}
void ALabScene::BuildOberbeck(){
    FString Raw;TSharedPtr<FJsonObject> O;
    if(FFileHelper::LoadFileToString(Raw,*(FPaths::ProjectContentDir()/TEXT("Lab/oberbeck-physics.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),O)){
        auto& P=Experiment.P;bool Valid=true;
        auto Get=[&](const TCHAR* K,double& V){double N=0;if(!O->TryGetNumberField(K,N))Valid=false;else V=N;};
        Get(TEXT("g"),P.G);Get(TEXT("m0"),P.M0);Get(TEXT("m"),P.M);Get(TEXT("width"),P.Width);Get(TEXT("height"),P.Height);Get(TEXT("depth"),P.Depth);Get(TEXT("j0"),P.J0);Get(TEXT("small"),P.Small);Get(TEXT("large"),P.Large);Get(TEXT("r_min"),P.RMin);Get(TEXT("r_max"),P.RMax);Get(TEXT("r_default"),P.RDefault);Get(TEXT("h_min"),P.HMin);Get(TEXT("h_max"),P.HMax);Get(TEXT("h_default"),P.HDefault);Get(TEXT("spoke"),P.Spoke);bOberbeckValid=Valid&&P.Valid();
    }
    if(!bOberbeckValid){Notice=TEXT("Неверный профиль физики. Запуск заблокирован.");return;}
    auto Add=[&](const FString& Id,FVector Pos,FVector Size,UMaterialInstanceDynamic* Mat){auto* M=Box(Id,Pos,Size/100,Mat,Id);OberbeckMeshes.Add(Id,M);OberbeckOriginal.Add(Id,Mat);return M;};
    Box(TEXT("OBBase"),FVector(0,0,0),FVector(.6,.75,.04),DarkMaterial);
    Box(TEXT("OBStand"),FVector(0,0,58),FVector(.07,.05,1.16),BaseMaterial);
    auto* Hub=Add(TEXT("oberbeck.flywheel"),FVector(12,0,95),FVector(8,8,3),AccentMaterial);Hub->SetStaticMesh(CylinderMesh);Hub->SetRelativeRotation(FRotator(90,0,0));
    for(int I=1;I<=4;I++){
        Add(FString::Printf(TEXT("oberbeck.spoke.%d"),I),FVector::ZeroVector,FVector(1.2,25,1.2),BaseMaterial);
        Add(FString::Printf(TEXT("oberbeck.mass.%d"),I),FVector::ZeroVector,FVector(3,4,3),AccentMaterial);
    }
    auto* Large=Add(TEXT("oberbeck.pulley.large"),FVector(18,0,95),FVector(6,6,1.5),BaseMaterial);Large->SetStaticMesh(CylinderMesh);Large->SetRelativeRotation(FRotator(90,0,0));
    auto* Small=Add(TEXT("oberbeck.pulley.small"),FVector(20,0,95),FVector(3,3,1.5),AccentMaterial);Small->SetStaticMesh(CylinderMesh);Small->SetRelativeRotation(FRotator(90,0,0));
    Add(TEXT("oberbeck.hanger"),FVector(20,-3,4),FVector(3,3,6),BaseMaterial);
    Add(TEXT("oberbeck.scale"),FVector(10,-10,43),FVector(.5,3,82),BaseMaterial);
    for(int I=0;I<=80;I++){
        Box(FString::Printf(TEXT("Tick%d"),I),FVector(10.5,-10,I+2),FVector(.005,I%10==0?.03:.015,.002),DarkMaterial);
        if(I%10==0)Label(FString::FromInt(I),FVector(11,-14,I+2),FVector2D(120,70),.035);
    }
    SelectionMark=Box(TEXT("SelectedPulley"),FVector(22,0,95),FVector(.004,.008,.05),DarkMaterial);
    Rope=Box(TEXT("OBRope"),FVector(20,-3,50),FVector(.002,.002,.8),AccentMaterial);
    const TCHAR* Ids[]={TEXT("oberbeck.release"),TEXT("oberbeck.timer.toggle"),TEXT("oberbeck.timer.reset")};
    const TCHAR* Captions[]={TEXT("Отпустить"),TEXT("Старт / стоп"),TEXT("Сброс часов")};
    for(int I=0;I<3;I++){Add(Ids[I],FVector(20,44,77-I*11),FVector(2,26,8),BaseMaterial);auto* W=Label(Captions[I],FVector(21.5,44,77-I*11),FVector2D(450,100),.055);if(auto* L=Cast<ULabWorldLabel>(W->GetUserWidgetObject()))L->Ink(FLinearColor(.02,.04,.05));}

    OberbeckCaption=Label(TEXT(""),FVector(24,45,96),FVector2D(750,160),.065);
    BuildFullTools();
    TArray<USceneComponent*> All;GetComponents<USceneComponent>(All);
    for(auto* C:All)if(C!=RootComponent&&!PanelParts.Contains(C)){
        OberbeckParts.Add(C);
        // Hard directional shadows obscure the narrow stand and measuring scale.
        // Keep surface lighting for depth, but leave the apparatus unobstructed.
        if(auto* Mesh=Cast<UStaticMeshComponent>(C))Mesh->SetCastShadow(false);
    }
}
void ALabScene::RefreshOberbeck(){
    if(!bOberbeckValid)return;
    for(int I=0;I<4;I++){
        const double A=Experiment.Angle+PI/4+I*PI/2;
        auto Spoke=OberbeckMeshes[FString::Printf(TEXT("oberbeck.spoke.%d"),I+1)];auto Mass=OberbeckMeshes[FString::Printf(TEXT("oberbeck.mass.%d"),I+1)];
        Spoke->SetRelativeLocation(FVector(12,FMath::Cos(A)*12.5,95+FMath::Sin(A)*12.5));Spoke->SetRelativeRotation(FQuat(FVector(1,0,0),A));
        Mass->SetRelativeLocation(FVector(14,FMath::Cos(A)*(FullMode?Experiment.MassR[I]:Experiment.R)*100,95+FMath::Sin(A)*(FullMode?Experiment.MassR[I]:Experiment.R)*100));Mass->SetRelativeRotation(FQuat(FVector(1,0,0),A));
        if(FullMode&&!Experiment.Installed[I]){Mass->SetRelativeLocation(StoragePosition(I));Mass->SetRelativeRotation(FRotator::ZeroRotator);}
    }
    double Height=Experiment.CurrentHeight(),Angle=Experiment.Angle;
    if(FullMode&&Replay&&ReplaySeries>=0&&ReplaySeries<(int)Notebook.Series.size()){
        const auto& S=Notebook.Series[ReplaySeries];double T=FMath::Min(ReplayTime,FMath::Sqrt(2*S.H/S.Acceleration));Height=S.H-S.Acceleration*T*T/2;Angle=(S.Attempts.empty()?0:S.Attempts.front().InitialAngle)-S.Acceleration*T*T/(2*S.Radius);
        for(int I=0;I<4;I++){double A=Angle+PI/4+I*PI/2;auto M=OberbeckMeshes[FString::Printf(TEXT("oberbeck.mass.%d"),I+1)];auto P=OberbeckMeshes[FString::Printf(TEXT("oberbeck.spoke.%d"),I+1)];P->SetRelativeLocation(FVector(12,FMath::Cos(A)*12.5,95+FMath::Sin(A)*12.5));P->SetRelativeRotation(FQuat(FVector(1,0,0),A));M->SetRelativeLocation(S.Slot>=2?FVector(14,FMath::Cos(A)*S.R*100,95+FMath::Sin(A)*S.R*100):StoragePosition(I));M->SetRelativeRotation(FQuat(FVector(1,0,0),A));}
    }
    const double Y=-Experiment.Radius()*100,Z=2+Height*100;
    OberbeckMeshes[TEXT("oberbeck.hanger")]->SetRelativeLocation(FVector(20,Y,Z+3));
    const double Top=Z+6,Length=95-Top;
    Rope->SetRelativeLocation(FVector(20,Y,(95+Top)/2));Rope->SetRelativeScale3D(FVector(.002,.002,Length/100));
    SelectionMark->SetRelativeLocation(FVector(22,Experiment.Pulley?2.5:1,95));
    for(int I=0;I<MassLabels.Num();I++)MassLabels[I]->SetRelativeLocation(OberbeckMeshes[FString::Printf(TEXT("oberbeck.mass.%d"),I+1)]->GetRelativeLocation()+FVector(3,0,0));
    RefreshFullTools();
}
void ALabScene::UpdateExperimentClock(){
    const double Now=FPlatformTime::Seconds(),D=FMath::Max(0.0,Now-LastClock);LastClock=Now;
    if(!bOberbeckValid)return;
    if(FullMode&&Replay){ReplayTime+=D*ReplaySpeed;return;}
    if(!FSlateApplication::Get().IsActive()&&(Experiment.Falling||Experiment.TimerRunning)){Experiment.Paused=true;Notice=TEXT("Пауза: окно неактивно. Нажмите «Продолжить».");}
    const bool WasFalling=Experiment.Falling;Experiment.Advance(D);
    if(WasFalling&&!Experiment.Falling){Notice=TEXT("Опыт завершён. Остановите секундомер.");SendState();}
}
void ALabScene::ExperimentAction(const FString& Id){
    if(ActiveLab!=TEXT("oberbeck")||!bOberbeckValid)return;
    if(FullMode&&bSaveBlocked){Notice=TEXT("Создайте новую работу из меню: старое сохранение не прочитано.");return;}
    UpdateExperimentClock();
    if(FullMode&&Replay){Notice=TEXT("Завершите просмотр записи перед измерением.");return;}
    if(Id==TEXT("resume")){Experiment.Paused=false;LastClock=FPlatformTime::Seconds();Notice.Empty();}
    else if(Id==TEXT("new")){if(FullMode&&(Experiment.Falling||Experiment.TimerRunning)){Notebook.Capture(Experiment,"interrupted");}Experiment.NewTrial();Notice=TEXT("Поднимите подвес вращением маховика.");}
    else if(Id==TEXT("oberbeck.release")){if(FullMode&&!Notebook.CanRelease(Experiment)){Notice=UTF8_TO_TCHAR(Notebook.Error.c_str());LastAction=Id;LastInput.Empty();LastUnit.Empty();SendState();return;}if(!Experiment.Release())Notice=TEXT("Поднимите подвес минимум на 20 см. Для повтора нажмите «Новый опыт».");else Notice=TEXT("Подвес падает. Секундомер запускается отдельно.");}
    else if(Id==TEXT("oberbeck.timer.reset")){Experiment.ResetTimer();}
    else if(Id==TEXT("oberbeck.timer.toggle")){
        const bool WasRunning=Experiment.TimerRunning;const bool Accepted=Experiment.TimerToggle();
        if(FullMode&&WasRunning&&!Experiment.Paused){Notebook.Capture(Experiment,Experiment.Early?"early":"pending");}
        if(WasRunning&&!Experiment.Paused){Notice=Accepted?TEXT("Измерение записано."):Experiment.Early?TEXT("Секундомер остановлен до касания пола. Повторите опыт."):TEXT("Для сравнения нужны тот же шкив и высота ±5 мм, а R должен отличаться минимум на 2 см.");}
        else if(!Accepted)Notice=TEXT("Для нового измерения нажмите «Новый опыт».");
    }
    RefreshOberbeck();if(FullMode)FullChanged();else SendState();
}
FString ALabScene::ExperimentText()const{
    if(!bOberbeckValid)return TEXT("Ошибка загрузки физического профиля");
    FString S=FString::Printf(TEXT("Секундомер   %.2f с%s\nR = %.1f см   •   h = %.1f см\nШкив: %s   •   подвес 100 г\n\n"),Experiment.Stopwatch(),Experiment.TimerRunning?TEXT("  •  идёт"):TEXT(""),Experiment.R*100,Experiment.H*100,Experiment.Pulley?TEXT("3 см"):TEXT("1,5 см"));
    int I=0;for(const auto& R:Experiment.Records)S+=FString::Printf(TEXT("Опыт %d: %.2f с\nR %.1f см, h %.1f см, шкив %s\n\n"),++I,R.Time,R.R*100,R.H*100,R.Pulley?TEXT("3 см"):TEXT("1,5 см"));
    if(Experiment.Step==2)S+=TEXT("Сравнение выполнено. При большем R\nидеальная модель падает медленнее.\nРучной отсчёт может отличаться.\n");
    return S+TEXT("\n")+Notice;
}
