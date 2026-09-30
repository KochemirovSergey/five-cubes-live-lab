#include "LabScene.h"
#include "VoiceConsole.h"
#include "IWebSocket.h"
#include "WebSocketsModule.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "LabWorldLabel.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
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

ALabScene::ALabScene()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    CubeMesh = Mesh.Object;
}

UStaticMeshComponent* ALabScene::AddBox(FName Name, FVector Position, FVector Scale)
{
    auto* Box = NewObject<UStaticMeshComponent>(this, Name);
    Box->SetupAttachment(RootComponent);
    Box->SetStaticMesh(CubeMesh);
    Box->SetRelativeLocation(Position);
    Box->SetRelativeScale3D(Scale);
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Box->RegisterComponent();
    return Box;
}

void ALabScene::BeginPlay()
{
    Super::BeginPlay();
    SceneVersion = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    AddBox(TEXT("Floor"), FVector(0,0,-65), FVector(14,16,0.2));
    for (int32 I=0; I<5; ++I)
    {
        Cubes.Add(AddBox(*FString::Printf(TEXT("cube_%d"), I+1), FVector(0,(2-I)*170,0), FVector(1)));
        auto* Label = NewObject<UWidgetComponent>(this);
        Label->SetupAttachment(RootComponent);
        Label->SetRelativeLocation(FVector(58,(2-I)*170,82));
        Label->SetWidgetClass(ULabWorldLabel::StaticClass());
        Label->SetDrawSize(FVector2D(400,120)); Label->SetRelativeScale3D(FVector(0.45));
        Label->SetTwoSided(true); Label->RegisterComponent(); Label->InitWidget();
        if(auto* W=Cast<ULabWorldLabel>(Label->GetUserWidgetObject())) W->Caption(FString::Printf(TEXT("Кубик %d"),I+1),false);
        Labels.Add(Label);
    }
    // A real mesh frame works in packaged builds; no editor-only debug drawing.
    for (int32 Axis=0; Axis<3; ++Axis)
        for (int32 A=-1; A<=1; A+=2)
            for (int32 B=-1; B<=1; B+=2)
            {
                FVector Position(0), Scale(0.035);
                Scale[Axis]=1.12;
                Position[(Axis+1)%3]=A*56;
                Position[(Axis+2)%3]=B*56;
                auto* Edge=AddBox(*FString::Printf(TEXT("Edge%d"), Frame.Num()), Position, Scale);
                Edge->SetVisibility(false); Frame.Add(Edge);
            }
    auto* Light = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,700), FRotator(-50,145,0));
    Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Light->GetLightComponent()->SetIntensity(7);
    auto* Sky = GetWorld()->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(1);
    Camera = GetWorld()->SpawnActor<ACameraActor>(FVector(1000,0,490), FRotator(-25,180,0));
    Camera->GetCameraComponent()->SetFieldOfView(60);
    ConnectionFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../../.runtime/connection.json"));
    FParse::Value(FCommandLine::Get(), TEXT("LabConnectionFile="), ConnectionFile);
    if (!AVoiceConsole::ConnectionPath.IsEmpty()) ConnectionFile=AVoiceConsole::ConnectionPath;
    Connect();
}

void ALabScene::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if (PC && !bCameraSet && Camera) { PC->SetViewTarget(Camera); bCameraSet=true; }
    if (PC && PC->PlayerCameraManager) for (const auto& Label : Labels)
        Label->SetWorldRotation((PC->PlayerCameraManager->GetCameraLocation()-Label->GetComponentLocation()).Rotation());
}

void ALabScene::ClearHighlight()
{
    for (const auto& Edge : Frame) Edge->SetVisibility(false);
    for (int32 I=0; I<Labels.Num(); ++I)
    {
        if(auto* W=Cast<ULabWorldLabel>(Labels[I]->GetUserWidgetObject())) W->Caption(FString::Printf(TEXT("Кубик %d"),I+1),false);
    }
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
void ALabScene::SendState()
{
    auto State=MakeShared<FJsonObject>();
    State->SetStringField(TEXT("session_id"),SessionId); State->SetStringField(TEXT("scene_version"),SceneVersion);
    State->SetStringField(TEXT("stage_id"),TEXT("five_cubes")); State->SetNumberField(TEXT("stage"),1);
    State->SetNumberField(TEXT("stages_total"),1);
    TArray<TSharedPtr<FJsonValue>> Allowed, Completed;
    for (int32 I=1; I<=5; ++I) Allowed.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("cube_%d"),I)));
    State->SetArrayField(TEXT("allowed_targets"),Allowed); State->SetArrayField(TEXT("completed"),Completed);
    auto Message=MakeShared<FJsonObject>(); Message->SetStringField(TEXT("type"),TEXT("state"));
    Message->SetObjectField(TEXT("state"),State); Send(Message);
}
void ALabScene::Receive(const FString& Raw)
{
    TSharedPtr<FJsonObject> Message;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Message) || !Message.IsValid()) return;
    FString Type, CommandId, Session, Version, Stage;
    if (!Message->TryGetStringField(TEXT("type"),Type)) return;
    if (Type==TEXT("hello_ack")) { SendState(); return; }
    if (!Message->TryGetStringField(TEXT("command_id"),CommandId) || !Message->TryGetStringField(TEXT("session_id"),Session) ||
        !Message->TryGetStringField(TEXT("scene_version"),Version) || !Message->TryGetStringField(TEXT("stage_id"),Stage)) return;
    FString Error;
    if (Session!=SessionId || Version!=SceneVersion || Stage!=TEXT("five_cubes")) Error=TEXT("STALE_SCENE");
    else if (Type==TEXT("clear")) ClearHighlight();
    else if (Type==TEXT("get_state")) SendState();
    else if (Type==TEXT("highlight"))
    {
        FString Id, Text; int32 Index=INDEX_NONE;
        Message->TryGetStringField(TEXT("target_id"),Id); Message->TryGetStringField(TEXT("text"),Text);
        for (int32 I=0; I<5; ++I) if (Id==FString::Printf(TEXT("cube_%d"),I+1)) Index=I;
        if (Index==INDEX_NONE || Text.Len()>120) Error=TEXT("TARGET_OR_TEXT_NOT_ALLOWED");
        else
        {
            ClearHighlight(); const FVector Center=Cubes[Index]->GetRelativeLocation();
            int32 N=0;
            for (int32 Axis=0; Axis<3; ++Axis) for (int32 A=-1; A<=1; A+=2) for (int32 B=-1; B<=1; B+=2)
            {
                FVector P=Center; P[(Axis+1)%3]+=A*56; P[(Axis+2)%3]+=B*56;
                Frame[N]->SetRelativeLocation(P); Frame[N++]->SetVisibility(true);
            }
            if(auto* W=Cast<ULabWorldLabel>(Labels[Index]->GetUserWidgetObject())) W->Caption(Text.IsEmpty()?Id:Text,true);
        }
    }
    else Error=TEXT("UNKNOWN_COMMAND");
    auto Ack=MakeShared<FJsonObject>(); Ack->SetStringField(TEXT("type"),TEXT("ack"));
    Ack->SetStringField(TEXT("command_id"),CommandId); Ack->SetStringField(TEXT("session_id"),SessionId);
    Ack->SetStringField(TEXT("scene_version"),Version); Ack->SetBoolField(TEXT("ok"),Error.IsEmpty());
    if (!Error.IsEmpty()) Ack->SetStringField(TEXT("error"),Error);
    Send(Ack);
}
void ALabScene::EndPlay(const EEndPlayReason::Type Reason)
{
    bEnding=true; GetWorldTimerManager().ClearTimer(RetryTimer);
    if (Socket) { Socket->OnConnected().Clear(); Socket->OnConnectionError().Clear(); Socket->OnClosed().Clear(); Socket->OnMessage().Clear(); Socket->Close(); Socket.Reset(); }
    Super::EndPlay(Reason);
}
