#include "LabScene.h"
#include "VoiceConsole.h"
#include "IWebSocket.h"
#include "WebSocketsModule.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "LabWorldLabel.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
static float SectionY(int I){return 375-I*250;}
ALabScene::ALabScene() {
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube")); CubeMesh=Mesh.Object;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); CylinderMesh=Cylinder.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")); Surface=Mat.Object;
}
UStaticMeshComponent* ALabScene::Box(FString Name,FVector Position,FVector Scale,UMaterialInstanceDynamic* Material,const FString& Control) {
    auto* Mesh=NewObject<UStaticMeshComponent>(this,*Name); Mesh->SetupAttachment(RootComponent);
    Mesh->SetStaticMesh(CubeMesh); Mesh->SetRelativeLocation(Position); Mesh->SetRelativeScale3D(Scale);
    if(Material)Mesh->SetMaterial(0,Material);
    Mesh->SetCollisionEnabled(Control.IsEmpty()?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryOnly);
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore); Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    if(!Control.IsEmpty())Mesh->ComponentTags.Add(*Control);
    Mesh->RegisterComponent(); return Mesh;
}
UWidgetComponent* ALabScene::Label(FString Text,FVector Position,FVector2D Size,float Scale) {
    auto* W=NewObject<UWidgetComponent>(this); W->SetupAttachment(RootComponent);
    W->SetRelativeLocation(Position); W->SetRelativeRotation(FRotator(0,0,0));
    W->SetWidgetClass(ULabWorldLabel::StaticClass());W->SetDrawSize(Size);W->SetRelativeScale3D(FVector(Scale));
    W->SetCastShadow(false);W->SetCollisionEnabled(ECollisionEnabled::NoCollision);W->SetTwoSided(true);W->RegisterComponent();W->InitWidget();
    if(auto* L=Cast<ULabWorldLabel>(W->GetUserWidgetObject()))L->Caption(Text,false);
    return W;
}
void ALabScene::BeginPlay() {
    Super::BeginPlay();
    FString Json;TSharedPtr<FJsonObject> Spec;
    const FString SpecPath=FPaths::ProjectContentDir()/TEXT("Lab/scenario.json");
    if(!FFileHelper::LoadFileToString(Json,*SpecPath)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Spec)||!Spec.IsValid()) {
        UE_LOG(LogTemp,Error,TEXT("Panel scenario missing: %s"),*SpecPath);return;
    }
    for(const auto& Value:Spec->GetArrayField(TEXT("steps"))) {
        auto S=Value->AsObject();StageIds.Add(S->GetStringField(TEXT("id")));Targets.Add(S->GetStringField(TEXT("target")));
        Titles.Add(S->GetStringField(TEXT("title")));Goals.Add(S->GetStringField(TEXT("goal")));
    }
    if(StageIds.Num()!=4)return;
    TSharedPtr<FJsonObject> Catalog;
    if(!FFileHelper::LoadFileToString(Json,*(FPaths::ProjectContentDir()/TEXT("Lab/targets.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Catalog)||!Catalog.IsValid())return;
    const TArray<FString> Sections={TEXT("rotary"),TEXT("slider"),TEXT("keypad"),TEXT("toggle")};
    for(const auto& Value:Catalog->GetArrayField(TEXT("targets"))){
        auto Entry=Value->AsObject();const FString Id=Entry->GetStringField(TEXT("id"));const int32 Index=Sections.IndexOfByKey(Entry->GetStringField(TEXT("section")));
        if(Index==INDEX_NONE||CatalogSections.Contains(Id))return;
        CatalogIds.Add(Id);CatalogSections.Add(Id,Index);
    }

    Progress.Threshold=Spec->GetNumberField(TEXT("rotation_threshold"));Progress.Code=TCHAR_TO_UTF8(*Spec->GetStringField(TEXT("code")));bConfigured=true;
    BaseMaterial=UMaterialInstanceDynamic::Create(Surface,this);BaseMaterial->SetVectorParameterValue(TEXT("Color"),FLinearColor(0.55,0.65,0.68));
    DarkMaterial=UMaterialInstanceDynamic::Create(Surface,this);DarkMaterial->SetVectorParameterValue(TEXT("Color"),FLinearColor(0.035,0.075,0.095));
    AccentMaterial=UMaterialInstanceDynamic::Create(Surface,this);AccentMaterial->SetVectorParameterValue(TEXT("Color"),FLinearColor(1,0.7,0.03));
    HighlightMaterial=UMaterialInstanceDynamic::Create(Surface,this);HighlightMaterial->SetVectorParameterValue(TEXT("Color"),FLinearColor(0.15,4,0.4));
    Box(TEXT("Cabinet"),FVector(-15,0,30),FVector(0.4,10.4,4.1),BaseMaterial);
    for(int I=0;I<4;I++) {
        const float Y=SectionY(I);
        Box(FString::Printf(TEXT("Section%d"),I),FVector(8,Y,25),FVector(0.1,2.35,3.7),DarkMaterial);
        Labels.Add(Label(FString::Printf(TEXT("%02d  %s"),I+1,*Titles[I]),FVector(25,Y,180),FVector2D(640,180),0.31));
        Indicators.Add(Label(TEXT("Ожидает"),FVector(30,Y,-126),FVector2D(550,100),0.32));
    }
    Knob=Box(TEXT("Rotary"),FVector(40,375,35),FVector(1.25,1.25,0.35),BaseMaterial,Targets[0]);
    Knob->SetStaticMesh(CylinderMesh);Knob->SetRelativeRotation(FRotator(90,0,0));
    Pointer=Box(TEXT("Pointer"),FVector(60,375,64),FVector(0.05,0.08,0.5),AccentMaterial);
    Label(TEXT("Зажмите и поверните"),FVector(35,375,-60),FVector2D(600,100),0.31);
    Box(TEXT("Track"),FVector(28,125,35),FVector(0.12,1.8,0.22),BaseMaterial,Targets[1]);
    Slider=Box(TEXT("Slider"),FVector(50,195,35),FVector(0.35,0.42,0.8),AccentMaterial,Targets[1]);
    Label(TEXT("ЛЕВО          ПРАВО"),FVector(35,125,-35),FVector2D(600,100),0.3);
    Display=Label(TEXT("_ _ _"),FVector(40,-125,107),FVector2D(600,100),0.3);
    const TCHAR* KeyNames[]={TEXT("7"),TEXT("8"),TEXT("9"),TEXT("4"),TEXT("5"),TEXT("6"),TEXT("1"),TEXT("2"),TEXT("3"),TEXT("C"),TEXT("0"),TEXT("E")};
    for(int I=0;I<12;I++) {
        float Y=-125+(1-I%3)*64,Z=58-(I/3)*43;
        this->Keys.Add(FString(TEXT("panel.keypad."))+KeyNames[I],Box(FString::Printf(TEXT("Key%d"),I),FVector(40,Y,Z),FVector(0.23,0.57,0.36),BaseMaterial,FString(TEXT("key."))+KeyNames[I]));
        FString Caption=I==9?TEXT("Очистить"):I==11?TEXT("Enter"):KeyNames[I];
        auto* KeyText=Label(Caption,FVector(54,Y,Z),FVector2D(260,80),I==9?0.2:0.25);
        if(auto* W=Cast<ULabWorldLabel>(KeyText->GetUserWidgetObject()))W->Ink(FLinearColor(0.02,0.04,0.05));
    }
    Box(TEXT("ToggleMount"),FVector(30,-375,35),FVector(0.12,0.6,1.4),BaseMaterial,Targets[3]);
    Lever=Box(TEXT("Toggle"),FVector(53,-375,10),FVector(0.32,0.38,0.7),AccentMaterial,Targets[3]);
    Label(TEXT("Нажмите на рычаг"),FVector(35,-375,-65),FVector2D(600,100),0.31);

    auto* Light=GetWorld()->SpawnActor<ADirectionalLight>(FVector(700,0,300),FRotator(-15,180,0));
    Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);Light->GetLightComponent()->SetIntensity(5);
    Camera=GetWorld()->SpawnActor<ACameraActor>(FVector(1150,0,70),FRotator(0,180,0));Camera->GetCameraComponent()->SetFieldOfView(60);
    ResetExercise();
    ConnectionFile=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../../.runtime/connection.json"));
    FParse::Value(FCommandLine::Get(),TEXT("LabConnectionFile="),ConnectionFile);
    if(!AVoiceConsole::ConnectionPath.IsEmpty())ConnectionFile=AVoiceConsole::ConnectionPath;
    Connect();
}
FString ALabScene::Goal() const {return !bConfigured?TEXT("Ошибка загрузки сценария"):Progress.Done()?TEXT("Упражнение выполнено"):Goals[Progress.Step];}
FString ALabScene::ProgressText() const {return Progress.Done()?TEXT("Выполнено 4 из 4"):FString::Printf(TEXT("Шаг %d из 4"),Progress.Step+1);}
void ALabScene::Refresh() {
    float A=FMath::DegreesToRadians(Progress.Angle);
    Pointer->SetRelativeLocation(FVector(60,375-FMath::Sin(A)*28,35+FMath::Cos(A)*28));
    Pointer->SetRelativeRotation(FRotator(0,0,Progress.Angle));
    Slider->SetRelativeLocation(FVector(50,125+(1-2*SliderPreview)*70,35));
    Lever->SetRelativeLocation(FVector(53,-375,Progress.Up?65:10));Lever->SetRelativeRotation(FRotator(Progress.Up?-20:20,0,0));
    FString Digits=UTF8_TO_TCHAR(Progress.Digits.c_str());if(Digits.IsEmpty())Digits=TEXT("_ _ _");
    if(Progress.CodeError)Digits=TEXT("Неверный код • Очистить");
    if(auto* W=Cast<ULabWorldLabel>(Display->GetUserWidgetObject()))W->Caption(Digits,Progress.CodeError);
    for(int I=0;I<4;I++) {
        if(auto* W=Cast<ULabWorldLabel>(Indicators[I]->GetUserWidgetObject()))W->Caption(I<Progress.Step?TEXT("Выполнено"):I==Progress.Step?TEXT("Текущий шаг"):TEXT("Ожидает"),I==Progress.Step);
    }
}
void ALabScene::ResetExercise(){if(!bConfigured)return;Dragging.Empty();Progress.Reset();SliderPreview=0;SceneVersion=FGuid::NewGuid().ToString();StateSeq=0;ClearHighlight();Refresh();SendState();}
void ALabScene::ApplyHighlightPhase(bool Lit){
    bHighlightLit=Lit;
    UMaterialInterface* Plain=BaseMaterial;
    UMaterialInterface* Accent=AccentMaterial;
    UMaterialInterface* Bright=HighlightMaterial;
    if(HighlightTarget==Targets[0])Knob->SetMaterial(0,Lit?Bright:Plain);
    else if(HighlightTarget==Targets[1])Slider->SetMaterial(0,Lit?Bright:Accent);
    else if(HighlightTarget==Targets[3])Lever->SetMaterial(0,Lit?Bright:Accent);
    else if(Keys.Contains(HighlightTarget))Keys[HighlightTarget]->SetMaterial(0,Lit?Bright:Plain);
    else if(HighlightTarget==Targets[2])for(auto& Key:Keys)Key.Value->SetMaterial(0,Lit?Bright:Plain);
}
void ALabScene::ClearHighlight(){
    HighlightIndex=-1;HighlightTarget.Empty();bHighlightLit=false;
    Knob->SetMaterial(0,BaseMaterial);Slider->SetMaterial(0,AccentMaterial);Lever->SetMaterial(0,AccentMaterial);
    for(auto& Key:Keys)Key.Value->SetMaterial(0,BaseMaterial);
    for(int I=0;I<Labels.Num();I++)if(auto* W=Cast<ULabWorldLabel>(Labels[I]->GetUserWidgetObject()))W->Caption(FString::Printf(TEXT("%02d  %s"),I+1,*Titles[I]),false);
}
void ALabScene::ActionChanged(int Before){Refresh();bStateDirty=true;if(Before!=Progress.Step){SendState();}UE_LOG(LogTemp,Display,TEXT("Panel action: stage=%d finished=%d"),Progress.Step+1,Progress.Done());}
void ALabScene::PressControl(const FString& Id,const FVector& P){
    if(!bConfigured)return;int Before=Progress.Step;
    if(Id==Targets[0]){Dragging=Id;LastAngle=FMath::RadiansToDegrees(FMath::Atan2(375-P.Y,P.Z-35));}
    else if(Id==Targets[1]){Dragging=Id;DragControl(P);}
    else if(Id==Targets[3]){Progress.Toggle();ActionChanged(Before);SendState();}
    else if(Id.StartsWith(TEXT("key."))&&Id.Len()==5){Progress.Key((char)Id[4]);ActionChanged(Before);SendState();}
}
void ALabScene::DragControl(const FVector& P){
    if(Dragging.IsEmpty())return;
    if(Dragging==Targets[0]){
        if(FVector2D(375-P.Y,P.Z-35).Size()<12)return;
        float A=FMath::RadiansToDegrees(FMath::Atan2(375-P.Y,P.Z-35));float Delta=FMath::FindDeltaAngleDegrees(LastAngle,A);LastAngle=A;
        if(FMath::Abs(Delta)>0.02){int Before=Progress.Step;Progress.Rotate(Delta);ActionChanged(Before);}
    }else if(Dragging==Targets[1]){SliderPreview=FMath::Clamp((195-P.Y)/140.f,0.f,1.f);Refresh();}
}
void ALabScene::ReleaseControl(){if(Dragging.IsEmpty())return;int Before=Progress.Step;if(Dragging==Targets[1]){Progress.Slide(SliderPreview>=0.5);SliderPreview=Progress.Right?1:0;}Dragging.Empty();ActionChanged(Before);SendState();}
void ALabScene::Tick(float D){
    Super::Tick(D);if(!bConfigured)return;
    if(!HighlightTarget.IsEmpty()){
        const bool Lit=FMath::Fmod(FPlatformTime::Seconds()-HighlightStartedAt,1.0)<0.5;
        if(Lit!=bHighlightLit)ApplyHighlightPhase(Lit);
    }
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0);PC&&!bCameraSet&&Camera){PC->SetViewTarget(Camera);bCameraSet=true;}
    if(bStateDirty&&FPlatformTime::Seconds()-LastStateAt>0.1)SendState();
}
void ALabScene::Connect()
{
    if (bEnding) return;
    FString ConfigText;
    TSharedPtr<FJsonObject> Config;
    if (!FFileHelper::LoadFileToString(ConfigText,*ConnectionFile) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ConfigText),Config) || !Config.IsValid() ||
        !Config->TryGetStringField(TEXT("url"),Url) || !Config->TryGetStringField(TEXT("session_id"),SessionId) ||
        !Config->TryGetStringField(TEXT("token"),Token))
    {
        UE_LOG(LogTemp, Warning, TEXT("Lab: start npm server; connection file not ready")); Retry(); return;
    }
    if (Socket) { Socket->OnConnected().Clear(); Socket->OnConnectionError().Clear(); Socket->OnClosed().Clear(); Socket->OnMessage().Clear(); Socket->Close(); }
    Socket = FWebSocketsModule::Get().CreateWebSocket(Url);
    Socket->OnConnected().AddWeakLambda(this,[this]() {
        ClearHighlight();
        auto Hello=MakeShared<FJsonObject>(); Hello->SetStringField(TEXT("type"),TEXT("hello"));
        Hello->SetStringField(TEXT("adapter"),TEXT("unreal")); Hello->SetStringField(TEXT("session_id"),SessionId);
        Hello->SetStringField(TEXT("token"),Token); Send(Hello);
    });
    Socket->OnMessage().AddWeakLambda(this,[this](const FString& Raw) { Receive(Raw); });
    Socket->OnConnectionError().AddWeakLambda(this,[this](const FString&) { Retry(); });
    Socket->OnClosed().AddWeakLambda(this,[this](int32,const FString&,bool) { ClearHighlight(); Retry(); });
    Socket->Connect();
}
void ALabScene::Retry()
{
    if (!bEnding) GetWorldTimerManager().SetTimer(RetryTimer,this,&ALabScene::Connect,3.0f,false);
}
void ALabScene::Send(const TSharedRef<FJsonObject>& Message)
{
    FString Raw; FJsonSerializer::Serialize(Message,TJsonWriterFactory<>::Create(&Raw));
    if (Socket.IsValid() && Socket->IsConnected()) Socket->Send(Raw);
}
void ALabScene::SendState(){
    if(!bConfigured||SessionId.IsEmpty())return;
    bStateDirty=false;LastStateAt=FPlatformTime::Seconds();
    auto S=MakeShared<FJsonObject>();
    S->SetStringField(TEXT("session_id"),SessionId);S->SetStringField(TEXT("scene_version"),SceneVersion);S->SetNumberField(TEXT("state_seq"),++StateSeq);
    S->SetStringField(TEXT("stage_id"),Progress.Done()?TEXT("complete"):StageIds[Progress.Step]);
    S->SetNumberField(TEXT("stage"),FMath::Min(Progress.Step+1,4));S->SetNumberField(TEXT("stages_total"),4);S->SetBoolField(TEXT("finished"),Progress.Done());
    TArray<TSharedPtr<FJsonValue>> Allowed,Completed;
    for(const auto& Id:CatalogIds)Allowed.Add(MakeShared<FJsonValueString>(Id));
    for(int I=0;I<Progress.Step;I++)Completed.Add(MakeShared<FJsonValueString>(StageIds[I]));
    S->SetArrayField(TEXT("allowed_targets"),Allowed);S->SetArrayField(TEXT("completed"),Completed);
    auto V=MakeShared<FJsonObject>();V->SetNumberField(TEXT("rotary_angle"),Progress.Angle);V->SetStringField(TEXT("slider"),Progress.Right?TEXT("right"):TEXT("left"));
    V->SetBoolField(TEXT("toggle_up"),Progress.Up);V->SetStringField(TEXT("keypad_input"),UTF8_TO_TCHAR(Progress.Digits.c_str()));V->SetBoolField(TEXT("code_error"),Progress.CodeError);S->SetObjectField(TEXT("instrument_state"),V);
    auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("type"),TEXT("state"));M->SetObjectField(TEXT("state"),S);Send(M);
}
void ALabScene::Receive(const FString& Raw){
    TSharedPtr<FJsonObject> M;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),M)||!M.IsValid())return;
    FString Type,Id,Session,Version,Stage;if(!M->TryGetStringField(TEXT("type"),Type))return;
    if(Type==TEXT("hello_ack")){SendState();return;}
    if(!M->TryGetStringField(TEXT("command_id"),Id)||!M->TryGetStringField(TEXT("session_id"),Session)||!M->TryGetStringField(TEXT("scene_version"),Version)||!M->TryGetStringField(TEXT("stage_id"),Stage))return;
    FString Error;FString Current=Progress.Done()?TEXT("complete"):StageIds[Progress.Step];
    if(Session!=SessionId||Version!=SceneVersion||Stage!=Current)Error=TEXT("STALE_SCENE");
    else if(Type==TEXT("clear"))ClearHighlight();
    else if(Type==TEXT("get_state"))SendState();
    else if(Type==TEXT("highlight")){
        FString Target,Text;M->TryGetStringField(TEXT("target_id"),Target);M->TryGetStringField(TEXT("text"),Text);
        if(!CatalogSections.Contains(Target)||Text.Len()>120)Error=TEXT("TARGET_OR_TEXT_NOT_ALLOWED");
        else {
            ClearHighlight();HighlightIndex=CatalogSections[Target];HighlightText=Text;
            HighlightTarget=Target;HighlightStartedAt=FPlatformTime::Seconds();ApplyHighlightPhase(true);
            if(auto* W=Cast<ULabWorldLabel>(Labels[HighlightIndex]->GetUserWidgetObject()))W->Caption(Text.IsEmpty()?Titles[HighlightIndex]:Text,true);
        }
    }else Error=TEXT("UNKNOWN_COMMAND");
    auto Ack=MakeShared<FJsonObject>();Ack->SetStringField(TEXT("type"),TEXT("ack"));Ack->SetStringField(TEXT("command_id"),Id);Ack->SetStringField(TEXT("session_id"),SessionId);Ack->SetStringField(TEXT("scene_version"),Version);Ack->SetBoolField(TEXT("ok"),Error.IsEmpty());if(!Error.IsEmpty())Ack->SetStringField(TEXT("error"),Error);Send(Ack);
}
void ALabScene::EndPlay(const EEndPlayReason::Type Reason){bEnding=true;GetWorldTimerManager().ClearTimer(RetryTimer);if(Socket){Socket->OnConnected().Clear();Socket->OnConnectionError().Clear();Socket->OnClosed().Clear();Socket->OnMessage().Clear();Socket->Close();Socket.Reset();}Super::EndPlay(Reason);}
