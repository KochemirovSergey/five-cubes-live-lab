#include "VoiceConsole.h"
#include "LabPanel.h"
#include "IWebSocket.h"
#include "WebSocketsModule.h"
#include "LabPlaybackWave.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Async/Async.h"
void RequestLabMicrophone(TFunction<void(bool)> Callback);
FString AVoiceConsole::ConnectionPath;
static FString Json(const TSharedRef<FJsonObject>& O){FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
AVoiceConsole::AVoiceConsole(){PrimaryActorTick.bCanEverTick=true;}
void AVoiceConsole::BeginPlay()
{
    Super::BeginPlay();
    if (!FParse::Value(FCommandLine::Get(),TEXT("LabConnectionFile="),ConnectionPath))
    {
        const FString SettingsDir=FString(FPlatformProcess::UserHomeDir())/TEXT("Library/Application Support/FiveCubes");
        const FString Runtime=SettingsDir/TEXT("Sessions")/FString::FromInt(FPlatformProcess::GetCurrentProcessId());
        IFileManager::Get().MakeDirectory(*Runtime,true); ConnectionPath=Runtime/TEXT("connection.json");
        FString Resources=FPaths::ConvertRelativePathToFull(FPaths::GetPath(FPlatformProcess::ExecutablePath())/TEXT("../Resources/Lab"));
        const FString Node=Resources/TEXT("node");
        const FString Args=FString::Printf(TEXT("\"%s\" --runtime \"%s\" --settings \"%s\" --parent %u"),*(Resources/TEXT("server/desktop.js")),*Runtime,*(SettingsDir/TEXT("settings.json")),FPlatformProcess::GetCurrentProcessId());
        if (FPaths::FileExists(Node)) Server=FPlatformProcess::CreateProc(*Node,*Args,false,true,true,nullptr,0,*Runtime,nullptr);
        if (!Server.IsValid()) Status=TEXT("Не удалось запустить встроенный сервер. Проверьте сборку приложения.");
    }
    Wave=NewObject<ULabPlaybackWave>(this); Wave->SetSampleRate(24000); Wave->NumChannels=1;
    Wave->Duration=INDEFINITELY_LOOPING_DURATION; Wave->bLooping=false;
    Speaker=NewObject<UAudioComponent>(this); Speaker->bAutoActivate=false; Speaker->bIsUISound=true; Speaker->RegisterComponent(); Speaker->SetSound(Wave);
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(PC){Panel=CreateWidget<ULabPanel>(PC,ULabPanel::StaticClass());Panel->Console=this;Panel->SetIsFocusable(true);Panel->AddToViewport(100);PC->bShowMouseCursor=true;}
    Connect();
}
void AVoiceConsole::Connect()
{
    if(bEnding)return;NextConnect=FPlatformTime::Seconds()+2;
    FString Text;TSharedPtr<FJsonObject> C;
    if(!FFileHelper::LoadFileToString(Text,*ConnectionPath)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),C)||!C.IsValid())return;
    FString Url;if(!C->TryGetStringField(TEXT("url"),Url)||!C->TryGetStringField(TEXT("session_id"),Session)||!C->TryGetStringField(TEXT("token"),Token))return;
    Url.RemoveFromEnd(TEXT("/scene"));Url+=TEXT("/voice");
    if(Socket){Socket->OnClosed().Clear();Socket->OnConnectionError().Clear();Socket->OnMessage().Clear();Socket->OnRawMessage().Clear();Socket->Close();}
    Socket=FWebSocketsModule::Get().CreateWebSocket(Url);
    Socket->OnConnected().AddWeakLambda(this,[this](){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("type"),TEXT("hello"));O->SetStringField(TEXT("session_id"),Session);O->SetStringField(TEXT("token"),Token);Socket->Send(Json(O));});
    Socket->OnMessage().AddWeakLambda(this,[this](const FString& S){Receive(S);});
    Socket->OnBinaryMessage().AddWeakLambda(this,[this](const void* Data,SIZE_T Size,bool Last){
        if(!bReady||!Wave)return;
        AudioFragments.Append((const uint8*)Data,(int32)Size);
        // Allow short network/render bursts; 2 s is a hard ceiling, not a target delay.
        if(AudioFragments.Num()>96000||Wave->GetAvailableAudioByteCount()+AudioFragments.Num()>96000){StopMessage=TEXT("Воспроизведение отстало более чем на 2 секунды. Начните разговор снова.");UE_LOG(LogTemp,Warning,TEXT("Voice output backlog: queued=%u fragment=%d"),Wave->GetAvailableAudioByteCount(),AudioFragments.Num());StopVoice(true,TEXT("output_backpressure"));return;}
        if(!Last)return;
        if(AudioFragments.Num()%2){StopMessage=TEXT("Получен повреждённый звуковой пакет. Начните разговор снова.");StopVoice(true,TEXT("invalid_output_pcm"));return;}
        Wave->QueueAudio(AudioFragments.GetData(),AudioFragments.Num());AudioFragments.Reset();if(!Speaker->IsPlaying())Speaker->Play();
    });
    Socket->OnConnectionError().AddWeakLambda(this,[this](const FString&){bConnected=false;StopAudio();bStarting=false;Status=TEXT("Нет связи с сервером. Переподключение…");});
    Socket->OnClosed().AddWeakLambda(this,[this](int32,const FString&,bool){bConnected=false;StopAudio();bStarting=false;Status=TEXT("Соединение закрыто. Переподключение…");});
    Socket->Connect();
}
void AVoiceConsole::Send(const FString& Type){if(Socket&&Socket->IsConnected()){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("type"),Type);Socket->Send(Json(O));}}
void AVoiceConsole::Receive(const FString& Raw)
{
    TSharedPtr<FJsonObject> O;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),O)||!O.IsValid())return;
    FString Type;if(!O->TryGetStringField(TEXT("type"),Type))return;
    if(Type==TEXT("hello_ack")||Type==TEXT("lab.state")){
        const bool WasSceneReady=bSceneReady; bConnected=true;const TSharedPtr<FJsonObject>* State;if(O->TryGetObjectField(TEXT("state"),State)){(*State)->TryGetBoolField(TEXT("connected"),bSceneReady);}
        if(!bReady&&!bStarting&&(Type==TEXT("hello_ack")||WasSceneReady!=bSceneReady))Status=bSceneReady?TEXT("Готово. Нажмите «Начать разговор»."):TEXT("Ожидание сцены…");
    }else if(Type==TEXT("voice.reset_output")){AudioFragments.Reset();if(Speaker)Speaker->Stop();if(Wave)Wave->ResetAudio();}
    else if(Type==TEXT("voice.ready")){bStarting=false;bReady=true;Status=TEXT("Разговор подключён. Говорите свободно.");}
    else if(Type==TEXT("voice.closed")){StopAudio();bStarting=false;bool Final=false;O->TryGetBoolField(TEXT("finalized"),Final);FString Reason;O->TryGetStringField(TEXT("reason"),Reason);UE_LOG(LogTemp,Display,TEXT("Voice closed: finalized=%d reason=%s"),Final,*Reason);Status=!StopMessage.IsEmpty()?StopMessage:Reason==TEXT("provider_closed")?TEXT("Голосовой сервис завершил сессию. Начните разговор снова."):Final?TEXT("Разговор завершён."):TEXT("Соединение завершено без подтверждения GPT-Live.");}
    else if(Type==TEXT("voice.error")){FString Code;O->TryGetStringField(TEXT("code"),Code);StopAudio();bStarting=false;Status=Code.Contains(TEXT("TIMEOUT"))?TEXT("Сервис не ответил вовремя. Начните разговор снова; лабораторная доступна."):Code.Contains(TEXT("CONNECTION"))?TEXT("Нет связи с голосовым сервисом. Проверьте сеть и начните разговор снова."):Code==TEXT("API_KEY_MISSING")?TEXT("Не настроен ключ голосового сервиса."):Code==TEXT("ALREADY_STARTED")?TEXT("Разговор уже запущен."):Code==TEXT("AUDIO_BACKPRESSURE")?TEXT("Звук отстаёт. Начните разговор снова."):TEXT("Ошибка голосового сервиса. Начните разговор снова. Код: ")+Code;StopMessage=Status;}
    else if(Type==TEXT("voice.transcript")){FString Who,Text;O->TryGetStringField(TEXT("speaker"),Who);O->TryGetStringField(TEXT("text"),Text);if(Who!=LastSpeaker){Transcript+=Who==TEXT("user")?TEXT("\nВы: "):TEXT("\nАссистент: ");LastSpeaker=Who;}Transcript+=Text;Transcript=Transcript.Right(4000);}
    else if(Type==TEXT("tool.result")){const TSharedPtr<FJsonObject>* Out;if(O->TryGetObjectField(TEXT("output"),Out)){bool Error=false;(*Out)->TryGetBoolField(TEXT("isError"),Error);ToolStatus=Error?TEXT("Команда не подтверждена сценой."):TEXT("MCP: команда подтверждена.");
        FString Name;O->TryGetStringField(TEXT("name"),Name);const double Now=FPlatformTime::Seconds();
        if(Name==TEXT("lab_highlight")&&!Timing.Speaking&&Timing.End>LastMeasuredEnd&&Now-Timing.End<30){Metric(TEXT("speech_end_to_highlight_ack"),(Now-Timing.End)*1000,!Error);LastMeasuredEnd=Timing.End;}
}}
}
void AVoiceConsole::StartVoice()
{
    UE_LOG(LogTemp,Display,TEXT("Lab UI: StartVoice clicked; connected=%d starting=%d ready=%d"),bConnected,bStarting,bReady);
    if(!bConnected){Status=TEXT("Нет связи с сервером. Ожидайте подключения.");return;}
    if(bStarting||bReady)return;if(!bSceneReady){Status=TEXT("Сцена пока не подключена.");return;}
    StopMessage.Empty();bStarting=true;Status=TEXT("Проверка микрофона…");
    TWeakObjectPtr<AVoiceConsole> Weak(this);
    const uint32 Generation=++PermissionGeneration;
    RequestLabMicrophone([Weak,Generation](bool Granted){if(!Weak.IsValid()||Weak->bEnding||!Weak->bStarting||Weak->PermissionGeneration!=Generation)return;if(!Granted){Weak->bStarting=false;Weak->Status=TEXT("Разрешите микрофон: Настройки macOS → Конфиденциальность → Микрофон.");return;}Weak->OpenMicrophone();});
}
void AVoiceConsole::OpenMicrophone()
{
    Timing=FVoiceTiming();LastMeasuredEnd=0;Wave->LastNonSilent.store(0);
    LastCapture=FPlatformTime::Seconds();
    Capture=MakeUnique<Audio::FAudioCapture>();Audio::FAudioCaptureDeviceParams Params;
    if(!Capture->OpenAudioCaptureStream(Params,[this](const void* Data,int32 Frames,int32 Channels,int32 Rate,double,bool Overflow){
        FScopeLock Lock(&AudioLock);LastCapture=FPlatformTime::Seconds();
        Resampler.Push((const float*)Data,Frames,Channels,Rate); Resampler.Overflow|=Overflow;
    },1024)||!Capture->StartStream()){
        StopAudio();bStarting=false;Status=TEXT("Микрофон недоступен. Проверьте устройство и разрешения.");return;
    }
    Transcript.Empty();LastSpeaker.Empty();Status=TEXT("Подключение GPT-Live…");Send(TEXT("voice.start"));
}
void AVoiceConsole::StopAudio()
{
    bReady=false;AudioFragments.Reset();if(Capture){Capture->StopStream();Capture->CloseStream();Capture.Reset();}
    if(Speaker)Speaker->Stop();if(Wave)Wave->ResetAudio();
    FScopeLock Lock(&AudioLock);Resampler.Reset();Level=0;
}
void AVoiceConsole::StopVoice(bool Clear,const FString& Reason){++PermissionGeneration;const bool Active=bStarting||bReady;StopAudio();if(Active){UE_LOG(LogTemp,Display,TEXT("Voice stop requested: %s"),*Reason);bStarting=true;if(Socket&&Socket->IsConnected()){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("type"),TEXT("voice.stop"));O->SetStringField(TEXT("reason"),Reason);Socket->Send(Json(O));}Status=StopMessage.IsEmpty()?TEXT("Завершение разговора…"):StopMessage;}else bStarting=false;if(Clear)ClearHighlight();}
void AVoiceConsole::ClearHighlight() {
    if(!Socket||!Socket->IsConnected())return;
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("type"),TEXT("lab.command"));O->SetStringField(TEXT("name"),TEXT("lab_clear_highlight"));O->SetObjectField(TEXT("args"),MakeShared<FJsonObject>());Socket->Send(Json(O));
}

void AVoiceConsole::Tick(float D)
{
    Super::Tick(D);
    if(!bPanelInputReady && Panel){
        if(auto* PC=UGameplayStatics::GetPlayerController(this,0)){
            FInputModeUIOnly Mode; Mode.SetWidgetToFocus(Panel->TakeWidget());
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->bShowMouseCursor=true; PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
            PC->SetInputMode(Mode);
            if(auto* Viewport=GetWorld()->GetGameViewport()){
                Viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
                Viewport->SetHideCursorDuringCapture(false);
            }
            bPanelInputReady=true;
            UE_LOG(LogTemp,Display,TEXT("Lab UI: panel focused, Slate UI input for buttons and 3D controls"));
        }
    }
    if(!bConnected&&FPlatformTime::Seconds()>NextConnect)Connect();
    std::vector<int16_t> PCM;bool Failed=false;
    {FScopeLock Lock(&AudioLock);Level=Resampler.Level;Failed=Resampler.Overflow||(bReady&&FPlatformTime::Seconds()-LastCapture>3);
      if(bReady)PCM=Resampler.Drain();else Resampler.Reset();}
    if(Failed){bool Overflow;{FScopeLock Lock(&AudioLock);Overflow=Resampler.Overflow;}StopMessage=Overflow?TEXT("Переполнен буфер микрофона. Начните разговор снова."):TEXT("Микрофон не передавал звук 3 секунды. Начните разговор снова.");StopVoice(true,Overflow?TEXT("capture_overflow"):TEXT("capture_timeout"));return;}
    if(bReady&&Socket){
        const double Now=FPlatformTime::Seconds(),Output=Wave?Wave->LastNonSilent.load():0;
        for(size_t I=0;I+480<=PCM.size();I+=480){Timing.Input(PCM.data()+I,480,Now-double(PCM.size()-I-480)/24000.,Output);Socket->Send(PCM.data()+I,960,true);}
        double Elapsed=Timing.Poll(Now,Output);if(Elapsed)Metric(TEXT("interruption_renderer"),Elapsed,Elapsed>0);
    }
}
void AVoiceConsole::EndPlay(const EEndPlayReason::Type Reason)
{
    bEnding=true;StopVoice(true,TEXT("app_exit"));if(Server.IsValid())Send(TEXT("desktop.shutdown"));if(Panel)Panel->RemoveFromParent();
    if(Socket){Socket->OnClosed().Clear();Socket->OnConnectionError().Clear();Socket->OnMessage().Clear();Socket->OnBinaryMessage().Clear();Socket->Close();}
    // Child watches parent lifetime; graceful shutdown is requested through its owner channel.
    if(Server.IsValid())FPlatformProcess::CloseProc(Server);
    Super::EndPlay(Reason);
}

void AVoiceConsole::Metric(const FString& Kind,double Milliseconds,bool Success){
    if(!Socket||!Socket->IsConnected())return;
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("type"),TEXT("voice.metric"));O->SetStringField(TEXT("kind"),Kind);O->SetNumberField(TEXT("milliseconds"),Milliseconds);O->SetBoolField(TEXT("success"),Success);Socket->Send(Json(O));
}
