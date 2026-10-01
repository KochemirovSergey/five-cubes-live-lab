#include "LabScene.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"

// Opt-in packaged integration checks. Storage must be explicitly isolated.
bool ALabScene::RunFullSelfTest(){
    FString Directory;if(!FParse::Value(FCommandLine::Get(),TEXT("ObTestDir="),Directory)||FPaths::IsRelative(Directory))return false;
    auto Require=[](bool Ok,const TCHAR* What){if(!Ok)UE_LOG(LogTemp,Error,TEXT("OB_FULL_CHECK: %s"),What);return Ok;};
    FullMode=true;ActiveLab=TEXT("oberbeck");LoadCatalog(TEXT("oberbeck-full-"));Notebook=FOberbeckLab();Experiment.Reset();Experiment.Individual=true;bSaveBlocked=false;
    if(!Require(bOberbeckValid&&StageIds.Num()==5&&CatalogIds.Contains(TEXT("oberbeck.caliper")),TEXT("resources")))return false;
    Notebook.Measure("D0","30","mm",.03,true);Notebook.Measure("D1","60","mm",.06,true);Notebook.Measure("h","600","mm",.6,true);Notebook.Measure("R","225","mm",.225,true);
    for(int K=0;K<4;K++){
        Experiment.NewTrial();Experiment.H=.6;Experiment.Pulley=K%2;Experiment.Installed.fill(K>=2);Experiment.MassR.fill(.225);
        if(!Require(Notebook.Begin(Experiment,3),TEXT("begin series")))return false;
        for(int I=0;I<3;I++){
            if(!Require(Notebook.CanRelease(Experiment)&&Experiment.Release()&&Experiment.TimerToggle(),TEXT("release/start")))return false;
            Experiment.Advance(Experiment.HitTime()+.04+I*.01);if(!Require(Experiment.TimerToggle(),TEXT("land/stop")))return false;
            Notebook.Capture(Experiment,"pending");auto A=Notebook.Current()->Attempts.back();if(!Require(Notebook.Enter(A.Id,std::to_string(A.Displayed),"s"),TEXT("enter time")))return false;
            Experiment.NewTrial();Experiment.Wind(100);
        }
    }
    for(auto& R:Notebook.References())if(!Require(Notebook.Check(R.first,TCHAR_TO_UTF8(*FString::Printf(TEXT("%.16g"),R.second.Value)),R.second.Unit),TEXT("reference answer")))return false;
    Notebook.Conclude(0,Notebook.Agree(0));Notebook.Conclude(1,Notebook.Agree(1));Notebook.Conclude(2,Notebook.Agree(2));Notebook.Coefficients=true;
    if(!Require(Notebook.Task()==5&&SaveLab(),TEXT("complete and save")))return false;
    int ExpectedAttempt=Notebook.NextAttempt;Notebook=FOberbeckLab();
    if(!Require(LoadLab()&&Notebook.Task()==5&&Notebook.NextAttempt==ExpectedAttempt&&Notebook.Series.size()==4,TEXT("round trip")))return false;
    auto Public=FullPublicState();if(!Require(!Public->HasField(TEXT("height"))&&!Public->HasField(TEXT("radius"))&&!Public->HasField(TEXT("answers")),TEXT("public privacy")))return false;
    FString PublicText;FJsonSerializer::Serialize(Public,TJsonWriterFactory<>::Create(&PublicText));FFileHelper::SaveStringToFile(PublicText,*(SaveDirectory()/TEXT("public-state.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if(!Require(Public->GetObjectField(TEXT("next_action"))->GetStringField(TEXT("id"))==TEXT("complete"),TEXT("guidance complete")))return false;
    ExportLab();if(!Require(Notice.StartsWith(TEXT("Экспорт сохранён:")),TEXT("export")))return false;
    Experiment.NewTrial();Experiment.H=.6;Experiment.Installed.fill(false);Experiment.Pulley=0;Notebook.Begin(Experiment,3);Experiment.Release();Experiment.TimerToggle();Experiment.Advance(.2);SaveLab();Notebook=FOberbeckLab();
    if(!Require(LoadLab()&&!Experiment.Falling&&!Experiment.TimerRunning&&Notebook.Current()->Attempts.back().Status=="interrupted",TEXT("interrupted recovery")))return false;
    const FString File=SaveDirectory()/TEXT("progress.json");FString Good;if(!FFileHelper::LoadFileToString(Good,*File))return false;
    FFileHelper::SaveStringToFile(TEXT("{bad"),*File);if(!Require(!LoadLab(),TEXT("corrupt refused")))return false;
    FString Preserved;FFileHelper::LoadFileToString(Preserved,*File);if(!Require(Preserved==TEXT("{bad"),TEXT("corrupt preserved")))return false;
    FFileHelper::SaveStringToFile(Good,*File);TSharedPtr<FJsonObject> Object;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Good),Object);Object->SetNumberField(TEXT("format_version"),999);FString Incompatible;FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Incompatible));FFileHelper::SaveStringToFile(Incompatible,*File);if(!Require(!LoadLab(),TEXT("incompatible refused")))return false;
    FFileHelper::SaveStringToFile(Good,*File);if(!LoadLab())return false;SaveLab();
    return Require(IFileManager::Get().FileExists(*(SaveDirectory()/TEXT("progress.backup.json"))),TEXT("backup"));
}
