#include "LabScene.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "LabWorldLabel.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

namespace {
using Obj=TSharedRef<FJsonObject>;
Obj Object(){return MakeShared<FJsonObject>();}
FString Str(const std::string& S){return UTF8_TO_TCHAR(S.c_str());}
std::string Std(const FString& S){return TCHAR_TO_UTF8(*S);}
void Num(Obj O,const TCHAR* K,double V){O->SetNumberField(K,V);}
FString Json(Obj O){FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
bool ReadNumber(const TSharedPtr<FJsonObject>& O,const TCHAR* K,double& V){return O&&O->TryGetNumberField(K,V)&&FMath::IsFinite(V);}
}
FString ALabScene::SaveDirectory()const{FString Override;if(FParse::Param(FCommandLine::Get(),TEXT("ObSelfTest"))&&FParse::Value(FCommandLine::Get(),TEXT("ObTestDir="),Override)&&!FPaths::IsRelative(Override))return Override;return FPaths::Combine(FPlatformProcess::UserHomeDir(),TEXT("Library/Application Support/FiveCubes/Labs/oberbeck"));}
void ALabScene::SelectFull(bool Continue){
    SelectLab(TEXT("oberbeck"));if(ActiveLab!=TEXT("oberbeck"))return;
    SelectedField.Empty();LastAction.Empty();LastInput.Empty();LastUnit.Empty();CalculationTask=0;VisibleUiTargets.Empty();
    FullMode=true;Notebook=FOberbeckLab();Experiment.Individual=true;NotebookView=0;Caliper=RCursor=HCursor=0;bSaveBlocked=false;
    LoadCatalog(TEXT("oberbeck-full-"));SceneVersion=FGuid::NewGuid().ToString();StateSeq=0;
    if(Continue){if(!LoadLab())bSaveBlocked=true;}
    else {
        FString File=SaveDirectory()/TEXT("progress.json");
        if(IFileManager::Get().FileExists(*File)){FString Archive=SaveDirectory()/(TEXT("previous-")+FGuid::NewGuid().ToString()+TEXT(".json"));if(IFileManager::Get().Copy(*Archive,*File)!=COPY_OK){bSaveBlocked=true;Notice=TEXT("Не удалось сохранить предыдущую работу; новая запись заблокирована.");}}
        if(!bSaveBlocked&&IFileManager::Get().FileExists(*File))IFileManager::Get().Delete(*File);
        if(!bSaveBlocked){Notice=TEXT("Снимите 4 груза на их места слева. Измерьте оба диаметра и высоту.");SaveLab();}
    }
    RefreshOberbeck();SendState();
}
void ALabScene::FullChanged(){if(!FullMode)return;SaveLab();RefreshOberbeck();SendState();}
void ALabScene::SetInstrument(const FString& Id,double V){
    if(!FullMode||!Experiment.Editable()||Replay)return;
    if(Id==TEXT("oberbeck.caliper"))Caliper=FMath::Clamp(V,0.,.08);
    if(Id==TEXT("oberbeck.ruler"))RCursor=FMath::Clamp(V,0.,.25);
    if(Id==TEXT("oberbeck.height_cursor"))HCursor=FMath::Clamp(V,0.,.8);
    RefreshFullTools();
}
void ALabScene::FullAction(const FString& Id,const FString& Text,const FString& Unit){
    if(!FullMode)return;
    if(bSaveBlocked){Notice=TEXT("Сохранение не прочитано. Вернитесь в меню и создайте новую работу; старый файл будет сохранён.");return;}
    if(Id==TEXT("export")){ExportLab();return;}
    if(Id==TEXT("forces")){ShowForces=!ShowForces;RefreshFullTools();return;}
    if(Id==TEXT("replay_stop")){Replay=false;RefreshOberbeck();return;}
    if(Experiment.Falling||Experiment.TimerRunning||Replay){Notice=TEXT("Во время опыта и просмотра журнал доступен только для чтения.");return;}
    Notebook.Error.clear();Notebook.ErrorCode.clear();LastAction=Id;LastInput=Text.Left(120);LastUnit=Unit.Left(24);
    if(Id.StartsWith(TEXT("answer:"))||Id.StartsWith(TEXT("measure:"))||Id.StartsWith(TEXT("time:")))SelectedField=Id;
    if(Id==TEXT("series"))Notebook.Begin(Experiment,Repeats);
    else if(Id==TEXT("replay")){
        if(ReplaySeries>=0&&ReplaySeries<(int)Notebook.Series.size()&&Notebook.Series[ReplaySeries].Complete()){Replay=true;ReplayTime=0;LastClock=FPlatformTime::Seconds();}else Notice=TEXT("Для просмотра нужна завершённая серия.");return;
    }
    else if(Id.StartsWith(TEXT("measure:"))){
        std::string K=Std(Id.Mid(8));double D=0;bool Positioned=false;
        if(K=="D0"||K=="D1"){D=FOberbeckLab::Rounded(Caliper,.0001);Positioned=Experiment.Editable()&&Experiment.Pulley==(K=="D1"?1:0)&&FMath::Abs(Caliper-2*Experiment.Radius())<=.00005;}
        else if(K=="R"){D=FOberbeckLab::Rounded(RCursor,.001);Positioned=Experiment.Editable()&&Experiment.MassCount()==4&&Experiment.Balanced()&&FMath::Abs(RCursor-Experiment.MassR[0])<=.0005;}
        else if(K=="h"){D=FOberbeckLab::Rounded(HCursor,.001);Positioned=Experiment.Editable()&&Experiment.H>=Experiment.P.HMin&&FMath::Abs(HCursor-Experiment.H)<=.0005;}
        Notebook.Measure(K,Std(Text),Std(Unit),D,Positioned);
    }
    else if(Id.StartsWith(TEXT("time:")))Notebook.Enter(FCString::Atoi(*Id.Mid(5)),Std(Text),Std(Unit));
    else if(Id.StartsWith(TEXT("exclude:")))Notebook.Exclude(FCString::Atoi(*Id.Mid(8)));
    else if(Id.StartsWith(TEXT("answer:")))Notebook.Check(Std(Id.Mid(7)),Std(Text),Std(Unit));
    else if(Id.StartsWith(TEXT("conclusion:")))Notebook.Conclude(FCString::Atoi(*Id.Mid(11)),Text==TEXT("yes"));
    else if(Id==TEXT("coefficients")){
        FString Clean=Text.Replace(TEXT(" "),TEXT(""));Notebook.Coefficients=Clean==TEXT("1,2,2,1");Notebook.Error=Notebook.Coefficients?"Коэффициенты верны.":"Проверьте степени m₀, r, t и h в приближённой формуле.";Notebook.ErrorCode=Notebook.Coefficients?"":"coefficients";Notebook.Revision++;
    }
    if(!Notebook.Error.empty())Notice=Str(Notebook.Error);
    FullChanged();
}
FString ALabScene::FullStatus()const {
    FString S=FString::Printf(TEXT("Секундомер: %.2f с%s   •   Шкив %d\nГрузов: %d/4%s\n"),Experiment.Stopwatch(),Experiment.TimerRunning?TEXT(" • идёт"):TEXT(""),Experiment.Pulley+1,Experiment.MassCount(),Experiment.Balanced()?TEXT(""):TEXT(" • несимметрично, пуск запрещён"));
    if(Notebook.Active>=0&&Notebook.Active<(int)Notebook.Series.size()){auto& R=Notebook.Series[Notebook.Active];S+=FString::Printf(TEXT("Серия №%d: принято %d/%d\n"),R.Id,R.Count(),R.N);}
    if(Experiment.Paused)S+=TEXT("Пауза — нажмите «Продолжить».\n");
    if(Replay)S+=TEXT("Просмотр модели: отсчёты не записываются.\n");
    const auto Hint=NextHint();return S+Notice+TEXT("\nДалее: ")+Str(Hint.Text);
}
void ALabScene::BuildFullTools(){
    TArray<USceneComponent*> Before;GetComponents<USceneComponent>(Before);
    auto Add=[&](FString Id,FVector P,FVector Size){auto M=Box(Id,P,Size/100,DarkMaterial,Id);OberbeckMeshes.Add(Id,M);OberbeckOriginal.Add(Id,DarkMaterial);};
    Box(TEXT("OBTray"),FVector(17,51,10),FVector(.025,.43,.06),BaseMaterial);
    Label(TEXT("Места для грузов"),FVector(24,51,23),FVector2D(520,75),.075);
    for(int I=0;I<4;I++){
        auto P=StoragePosition(I);
        Add(FString::Printf(TEXT("oberbeck.storage.%d"),I+1),P-FVector(2,0,3),FVector(2,9,3));
        Label(FString::FromInt(I+1),P+FVector(3,0,-7),FVector2D(100,70),.065);
        auto W=Label(FString::FromInt(I+1),FVector::ZeroVector,FVector2D(100,70),.05);
        if(auto L=Cast<ULabWorldLabel>(W->GetUserWidgetObject()))L->Ink(FLinearColor(.02,.04,.05));
        MassLabels.Add(W);
    }
    Add(TEXT("oberbeck.caliper"),FVector(24,0,95),FVector(.6,.6,10));
    Add(TEXT("oberbeck.caliper.fixed"),FVector(24,0,95),FVector(.6,.6,10));
    Add(TEXT("oberbeck.ruler"),FVector(24,0,95),FVector(.6,1,1));
    Add(TEXT("oberbeck.height_cursor"),FVector(24,-10,2),FVector(.6,6,.4));
    // Force shafts and arrow heads, visible only after an explicit learner toggle.
    Add(TEXT("oberbeck.force.gravity"),FVector(24,-3,10),FVector(.6,.6,10));
    Add(TEXT("oberbeck.force.tension"),FVector(24,-3,20),FVector(.6,.6,10));
    for(auto Force:{TEXT("gravity"),TEXT("tension")})for(int Side=0;Side<2;Side++){FString Id=FString(TEXT("oberbeck.arrow."))+Force+FString::FromInt(Side);Add(Id,FVector::ZeroVector,FVector(.6,.5,3));}
    Add(TEXT("oberbeck.radius_line"),FVector::ZeroVector,FVector(.3,1,.3));
    TArray<USceneComponent*> After;GetComponents<USceneComponent>(After);for(auto* C:After)if(!Before.Contains(C))FullParts.Add(C);
}
void ALabScene::RefreshFullTools(){
    for(auto C:FullParts)C->SetVisibility(FullMode&&ActiveLab==TEXT("oberbeck"));
    const TCHAR* Tools[]={TEXT("oberbeck.caliper"),TEXT("oberbeck.caliper.fixed"),TEXT("oberbeck.ruler"),TEXT("oberbeck.height_cursor")};
    for(auto K:Tools)if(auto P=OberbeckMeshes.Find(K)){(*P)->SetVisibility(FullMode&&!Replay);(*P)->SetCollisionEnabled(FullMode&&!Replay?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);}
    for(int I=0;I<4;I++){auto M=OberbeckMeshes[FString::Printf(TEXT("oberbeck.storage.%d"),I+1)];M->SetVisibility(FullMode);}
    if(FullMode){
        OberbeckMeshes[TEXT("oberbeck.caliper.fixed")]->SetRelativeLocation(FVector(24,-Experiment.Radius()*100,95));
        OberbeckMeshes[TEXT("oberbeck.caliper")]->SetRelativeLocation(FVector(24,(-Experiment.Radius()+Caliper)*100,95));
        double A=Experiment.Angle+PI/4;
        OberbeckMeshes[TEXT("oberbeck.ruler")]->SetRelativeLocation(FVector(24,FMath::Cos(A)*RCursor*100,95+FMath::Sin(A)*RCursor*100));
        OberbeckMeshes[TEXT("oberbeck.height_cursor")]->SetRelativeLocation(FVector(24,-10,2+HCursor*100));
    }
    for(auto Force:{TEXT("gravity"),TEXT("tension")})for(int Side=0;Side<2;Side++){
        bool Down=FString(Force)==TEXT("gravity");auto M=OberbeckMeshes[FString(TEXT("oberbeck.arrow."))+Force+FString::FromInt(Side)];M->SetVisibility(FullMode&&ShowForces&&!Replay);M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        M->SetRelativeLocation(FVector(25,-Experiment.Radius()*100+(Down?-2:2)+(Side?1:-1),5+Experiment.CurrentHeight()*100+(Down?-10:10)));M->SetRelativeRotation(FQuat(FVector(1,0,0),(Side?1:-1)*(Down?1:-1)*PI/4));
    }
    auto RadiusLine=OberbeckMeshes[TEXT("oberbeck.radius_line")];RadiusLine->SetVisibility(FullMode&&ShowForces&&!Replay);RadiusLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);double A=Experiment.Angle+PI/4;RadiusLine->SetRelativeLocation(FVector(25,FMath::Cos(A)*Experiment.MassR[0]*50,95+FMath::Sin(A)*Experiment.MassR[0]*50));RadiusLine->SetRelativeRotation(FQuat(FVector(1,0,0),A));RadiusLine->SetRelativeScale3D(FVector(.003,Experiment.MassR[0],.003));
    for(auto K:{TEXT("oberbeck.force.gravity"),TEXT("oberbeck.force.tension")}){auto M=OberbeckMeshes[K];M->SetVisibility(FullMode&&ShowForces&&!Replay);M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetRelativeLocation(FVector(25, -Experiment.Radius()*100+(FString(K).EndsWith(TEXT("gravity"))?-2:2),5+Experiment.CurrentHeight()*100+(FString(K).EndsWith(TEXT("gravity"))?-6:6)));}
}
Obj ALabScene::FullPublicState()const {
    auto O=Object();O->SetStringField(TEXT("phase"),Replay?TEXT("replay"):Experiment.Falling?TEXT("falling"):Experiment.Landed?TEXT("landed"):TEXT("preparing"));
    Num(O,TEXT("pulley"),Experiment.Pulley);Num(O,TEXT("mass_count"),Experiment.MassCount());O->SetBoolField(TEXT("balanced"),Experiment.Balanced());Num(O,TEXT("stopwatch"),FOberbeckLab::Rounded(Experiment.Stopwatch(),.01));O->SetBoolField(TEXT("timer_running"),Experiment.TimerRunning);O->SetBoolField(TEXT("paused"),Experiment.Paused);
    O->SetStringField(TEXT("substep"),Replay?TEXT("review"):Experiment.Falling?TEXT("wait_for_landing"):Experiment.TimerRunning?TEXT("stop_timer_after_landing"):Experiment.Landed?TEXT("copy_time_to_journal"):TEXT("prepare_or_calculate"));
    auto R=Object();for(auto& V:Notebook.Readings){auto M=Object();Num(M,TEXT("value_si"),V.second.Value);Num(M,TEXT("error_si"),V.second.Error);R->SetObjectField(Str(V.first),M);}O->SetObjectField(TEXT("measurements"),R);
    TArray<TSharedPtr<FJsonValue>> Series;
    for(int I=0;I<4;I++)if(auto S=Notebook.Slot(I)){auto V=Object();Num(V,TEXT("id"),S->Id);Num(V,TEXT("configuration"),I);Num(V,TEXT("accepted"),S->Count());Num(V,TEXT("required"),S->N);TArray<TSharedPtr<FJsonValue>> Times;for(auto& A:S->Attempts)if(A.Status=="accepted")Times.Add(MakeShared<FJsonValueNumber>(A.Entered));V->SetArrayField(TEXT("times"),Times);Series.Add(MakeShared<FJsonValueObject>(V));}O->SetArrayField(TEXT("series"),Series);
    TArray<TSharedPtr<FJsonValue>> Answers;for(auto& A:Notebook.Answers)if(A.second.Correct)Answers.Add(MakeShared<FJsonValueString>(Str(A.first)));O->SetArrayField(TEXT("checked_answers"),Answers);
    TArray<TSharedPtr<FJsonValue>> Masses;for(int I=0;I<4;I++){auto M=Object();Num(M,TEXT("id"),I+1);M->SetBoolField(TEXT("installed"),Experiment.Installed[I]);Masses.Add(MakeShared<FJsonValueObject>(M));}O->SetArrayField(TEXT("masses"),Masses);
    Num(O,TEXT("view"),NotebookView);Num(O,TEXT("calculation_task"),CalculationTask+1);O->SetStringField(TEXT("selected_field"),SelectedField);
    const FObSeries* Current=Notebook.Active>=0&&Notebook.Active<(int)Notebook.Series.size()?&Notebook.Series[Notebook.Active]:nullptr;
    Num(O,TEXT("active_series"),Current?Current->Id:0);
    auto Attempt=Object();Num(Attempt,TEXT("id"),Current&&!Current->Attempts.empty()?Current->Attempts.back().Id:0);Attempt->SetStringField(TEXT("status"),Current&&!Current->Attempts.empty()?Str(Current->Attempts.back().Status):TEXT("none"));O->SetObjectField(TEXT("attempt"),Attempt);
    auto Entered=Object();for(const auto& A:Notebook.Answers){auto V=Object();Num(V,TEXT("value_si"),A.second.Value);V->SetStringField(TEXT("unit"),Str(A.second.Unit));V->SetBoolField(TEXT("correct"),A.second.Correct);Entered->SetObjectField(Str(A.first),V);}O->SetObjectField(TEXT("submitted_answers"),Entered);
    auto Feedback=Object();Feedback->SetStringField(TEXT("code"),Str(Notebook.ErrorCode));Feedback->SetStringField(TEXT("message"),Str(Notebook.Error));Feedback->SetStringField(TEXT("action"),LastAction);Feedback->SetStringField(TEXT("input"),LastInput);Feedback->SetStringField(TEXT("unit"),LastUnit);O->SetObjectField(TEXT("feedback"),Feedback);
    auto Hint=NextHint();auto Next=Object();Next->SetStringField(TEXT("id"),Str(Hint.Id));Next->SetStringField(TEXT("text"),Str(Hint.Text));Next->SetStringField(TEXT("target_id"),Str(Hint.Target));Next->SetStringField(TEXT("field"),Str(Hint.Field));O->SetObjectField(TEXT("next_action"),Next);
    TArray<TSharedPtr<FJsonValue>> Visible;for(const auto& Id:CatalogIds)if(TargetVisibilityError(Id).IsEmpty())Visible.Add(MakeShared<FJsonValueString>(Id));O->SetArrayField(TEXT("visible_targets"),Visible);return O;
}
Obj ALabScene::LabDocument()const {
    auto O=Object();Num(O,TEXT("format_version"),FOberbeckLab::Format);Num(O,TEXT("method_version"),FOberbeckLab::Method);Num(O,TEXT("profile_version"),FOberbeckLab::Profile);
    Num(O,TEXT("next_series"),Notebook.NextSeries);Num(O,TEXT("next_attempt"),Notebook.NextAttempt);Num(O,TEXT("active"),Notebook.Active);Num(O,TEXT("fixed_height"),Notebook.FixedHeight);Num(O,TEXT("revision"),Notebook.Revision);
    O->SetBoolField(TEXT("coefficients"),Notebook.Coefficients);O->SetBoolField(TEXT("conclusion0"),Notebook.Conclusion0);O->SetBoolField(TEXT("conclusion1"),Notebook.Conclusion1);O->SetBoolField(TEXT("conclusion_rotation"),Notebook.ConclusionRotation);
    auto P=Object();Num(P,TEXT("h"),Experiment.H);Num(P,TEXT("angle"),Experiment.Angle);Num(P,TEXT("pulley"),Experiment.Pulley);P->SetBoolField(TEXT("in_progress"),Experiment.Falling||Experiment.TimerRunning);TArray<TSharedPtr<FJsonValue>> Masses;for(int I=0;I<4;I++){auto M=Object();M->SetBoolField(TEXT("installed"),Experiment.Installed[I]);Num(M,TEXT("r"),Experiment.MassR[I]);Masses.Add(MakeShared<FJsonValueObject>(M));}P->SetArrayField(TEXT("masses"),Masses);O->SetObjectField(TEXT("preparation"),P);
    auto R=Object();for(auto& V:Notebook.Readings){auto M=Object();Num(M,TEXT("value"),V.second.Value);Num(M,TEXT("error"),V.second.Error);M->SetStringField(TEXT("unit"),Str(V.second.Unit));R->SetObjectField(Str(V.first),M);}O->SetObjectField(TEXT("readings"),R);
    auto Answers=Object();for(auto& V:Notebook.Answers){auto A=Object();Num(A,TEXT("value"),V.second.Value);A->SetStringField(TEXT("unit"),Str(V.second.Unit));A->SetBoolField(TEXT("correct"),V.second.Correct);Answers->SetObjectField(Str(V.first),A);}O->SetObjectField(TEXT("answers"),Answers);
    TArray<TSharedPtr<FJsonValue>> Series;
    for(auto& S:Notebook.Series){auto V=Object();Num(V,TEXT("id"),S.Id);Num(V,TEXT("slot"),S.Slot);Num(V,TEXT("n"),S.N);Num(V,TEXT("h"),S.H);Num(V,TEXT("r"),S.R);Num(V,TEXT("j"),S.J);Num(V,TEXT("pulley_radius"),S.Radius);Num(V,TEXT("acceleration"),S.Acceleration);TArray<TSharedPtr<FJsonValue>> Attempts;for(auto& A:S.Attempts){auto T=Object();Num(T,TEXT("id"),A.Id);Num(T,TEXT("displayed"),A.Displayed);Num(T,TEXT("entered"),A.Entered);Num(T,TEXT("initial_angle"),A.InitialAngle);T->SetStringField(TEXT("status"),Str(A.Status));Attempts.Add(MakeShared<FJsonValueObject>(T));}V->SetArrayField(TEXT("attempts"),Attempts);Series.Add(MakeShared<FJsonValueObject>(V));}O->SetArrayField(TEXT("series"),Series);
    TArray<TSharedPtr<FJsonValue>> History;for(auto& H:Notebook.History)History.Add(MakeShared<FJsonValueString>(Str(H)));O->SetArrayField(TEXT("history"),History);
    auto Policy=Object();Num(Policy,TEXT("mass_error_kg"),.0005);Num(Policy,TEXT("diameter_error_m"),.0001);Num(Policy,TEXT("length_error_m"),.001);Num(Policy,TEXT("reaction_allowance_s"),.20);Num(Policy,TEXT("timer_half_resolution_s"),.005);Num(Policy,TEXT("answer_relative_tolerance"),.005);Policy->SetStringField(TEXT("interpretation"),TEXT("Учебная сумма предельных вкладов; не доверительный интервал"));O->SetObjectField(TEXT("uncertainty_policy"),Policy);return O;
}
bool ALabScene::SaveLab(){
    if(!FullMode||bSaveBlocked)return false;
    FString Dir=SaveDirectory();IFileManager::Get().MakeDirectory(*Dir,true);FString File=Dir/TEXT("progress.json"),Temp=Dir/TEXT("progress.writing.json"),Backup=Dir/TEXT("progress.backup.json");
    if(!FFileHelper::SaveStringToFile(Json(LabDocument()),*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)){Notice=TEXT("Ошибка автосохранения: не удалось записать файл.");return false;}
    if(IFileManager::Get().FileExists(*File)&&IFileManager::Get().Copy(*Backup,*File)!=COPY_OK){Notice=TEXT("Ошибка резервной копии. Предыдущий файл сохранён.");return false;}
    // POSIX rename is atomic on the same filesystem; do not delete the destination first.
    if(rename(TCHAR_TO_UTF8(*Temp),TCHAR_TO_UTF8(*File))!=0){Notice=TEXT("Ошибка атомарной замены сохранения.");return false;}return true;
}
bool ALabScene::LoadLab(){
    FString Raw;TSharedPtr<FJsonObject> O;
    const FString File=SaveDirectory()/TEXT("progress.json");
    if(!FFileHelper::LoadFileToString(Raw,*File)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),O)||!O){Notice=TEXT("Сохранение отсутствует или повреждено. Оно не перезаписано; создайте новую работу из меню.");return false;}
    auto Fail=[&](int Line){UE_LOG(LogTemp,Warning,TEXT("Oberbeck save validation rejected at %d"),Line);Notice=TEXT("Сохранение повреждено или несовместимо. Оно оставлено на диске; автозапись заблокирована.");return false;};
    double Format=0,Method=0,Profile=0;if(!ReadNumber(O,TEXT("format_version"),Format)||Format!=FOberbeckLab::Format||!ReadNumber(O,TEXT("method_version"),Method)||Method!=FOberbeckLab::Method||!ReadNumber(O,TEXT("profile_version"),Profile)||Profile!=FOberbeckLab::Profile)return Fail(__LINE__);
    FOberbeckLab N;auto Number=[&](const TCHAR* K,int& V){double D;if(!ReadNumber(O,K,D)||D!=FMath::FloorToDouble(D)||D< -1||D>1000000)return false;V=(int)D;return true;};
    if(!Number(TEXT("next_series"),N.NextSeries)||!Number(TEXT("next_attempt"),N.NextAttempt)||!Number(TEXT("active"),N.Active)||!Number(TEXT("revision"),N.Revision)||!ReadNumber(O,TEXT("fixed_height"),N.FixedHeight)||N.FixedHeight<0||N.FixedHeight>.8)return Fail(__LINE__);
    if(!O->TryGetBoolField(TEXT("coefficients"),N.Coefficients)||!O->TryGetBoolField(TEXT("conclusion0"),N.Conclusion0)||!O->TryGetBoolField(TEXT("conclusion1"),N.Conclusion1)||!O->TryGetBoolField(TEXT("conclusion_rotation"),N.ConclusionRotation))return Fail(__LINE__);
    const TSharedPtr<FJsonObject>* R;const TSharedPtr<FJsonObject>* A;const TSharedPtr<FJsonObject>* Prep;const TArray<TSharedPtr<FJsonValue>>* Series;const TArray<TSharedPtr<FJsonValue>>* History;
    if(!O->TryGetObjectField(TEXT("readings"),R)||!O->TryGetObjectField(TEXT("answers"),A)||!O->TryGetObjectField(TEXT("preparation"),Prep)||!O->TryGetArrayField(TEXT("series"),Series)||!O->TryGetArrayField(TEXT("history"),History)||Series->Num()>10000)return Fail(__LINE__);
    for(auto& V:(*R)->Values){auto M=V.Value->AsObject();FObReading Reading;FString Unit;if(!ReadNumber(M,TEXT("value"),Reading.Value)||Reading.Value<=0||!ReadNumber(M,TEXT("error"),Reading.Error)||Reading.Error<=0||!M->TryGetStringField(TEXT("unit"),Unit))return Fail(__LINE__);Reading.Unit=Std(Unit);N.Readings[Std(FString(*V.Key))]=Reading;}
    for(auto& V:(*A)->Values){auto M=V.Value->AsObject();FObAnswer Answer;FString Unit;if(!ReadNumber(M,TEXT("value"),Answer.Value)||!M->TryGetStringField(TEXT("unit"),Unit)||!M->TryGetBoolField(TEXT("correct"),Answer.Correct))return Fail(__LINE__);Answer.Unit=Std(Unit);N.Answers[Std(FString(*V.Key))]=Answer;}
    for(auto& Value:*Series){auto V=Value->AsObject();if(!V)return Fail(__LINE__);FObSeries S;double Id,Slot,Ns;const TArray<TSharedPtr<FJsonValue>>* Attempts;
        if(!ReadNumber(V,TEXT("id"),Id)||Id<1||Id>=N.NextSeries||!ReadNumber(V,TEXT("slot"),Slot)||Slot<0||Slot>3||!ReadNumber(V,TEXT("n"),Ns)||Ns<3||Ns>5||!ReadNumber(V,TEXT("h"),S.H)||S.H<.2||S.H>.8||!ReadNumber(V,TEXT("r"),S.R)||S.R<0||S.R>.23||!ReadNumber(V,TEXT("j"),S.J)||S.J<=0||!ReadNumber(V,TEXT("pulley_radius"),S.Radius)||S.Radius<=0||!ReadNumber(V,TEXT("acceleration"),S.Acceleration)||S.Acceleration<=0||!V->TryGetArrayField(TEXT("attempts"),Attempts))return Fail(__LINE__);if(Id!=FMath::FloorToDouble(Id)||Slot!=FMath::FloorToDouble(Slot)||Ns!=FMath::FloorToDouble(Ns))return Fail(__LINE__);for(auto& Old:N.Series)if(Old.Id==Id)return Fail(__LINE__);S.Id=Id;S.Slot=Slot;S.N=Ns;
        for(auto& AV:*Attempts){auto M=AV->AsObject();FObAttempt Attempt;double Aid;FString Status;if(!ReadNumber(M,TEXT("id"),Aid)||Aid<1||Aid>=N.NextAttempt||!ReadNumber(M,TEXT("displayed"),Attempt.Displayed)||Attempt.Displayed<0||!ReadNumber(M,TEXT("entered"),Attempt.Entered)||Attempt.Entered<0||!ReadNumber(M,TEXT("initial_angle"),Attempt.InitialAngle)||!M->TryGetStringField(TEXT("status"),Status)||!TArray<FString>{TEXT("accepted"),TEXT("pending"),TEXT("excluded"),TEXT("early"),TEXT("interrupted")}.Contains(Status))return Fail(__LINE__);if(Aid!=FMath::FloorToDouble(Aid))return Fail(__LINE__);Attempt.Id=Aid;Attempt.Status=Std(Status);if(Attempt.Status=="accepted"&&(Attempt.Entered<=0||FMath::Abs(Attempt.Entered-Attempt.Displayed)>.00501))return Fail(__LINE__);S.Attempts.push_back(Attempt);}if(S.Count()>S.N)return Fail(__LINE__);N.Series.push_back(S);
    }
    if(N.Active>=int(N.Series.size())||N.Active< -1)return Fail(__LINE__);for(auto& H:*History){FString S;if(!H->TryGetString(S))return Fail(__LINE__);N.History.push_back(Std(S));}
    FOberbeckModel P;P.P=Experiment.P;P.Individual=true;P.HeightStop=N.FixedHeight;double Pulley;bool Interrupted;const TArray<TSharedPtr<FJsonValue>>* Masses;
    if(!ReadNumber(*Prep,TEXT("h"),P.H)||P.H<0||P.H>.8||!ReadNumber(*Prep,TEXT("angle"),P.Angle)||!ReadNumber(*Prep,TEXT("pulley"),Pulley)||(Pulley!=0&&Pulley!=1)||!(*Prep)->TryGetBoolField(TEXT("in_progress"),Interrupted)||!(*Prep)->TryGetArrayField(TEXT("masses"),Masses)||Masses->Num()!=4)return Fail(__LINE__);P.Pulley=Pulley;
    for(int I=0;I<4;I++){auto M=(*Masses)[I]->AsObject();if(!M||!M->TryGetBoolField(TEXT("installed"),P.Installed[I])||!ReadNumber(M,TEXT("r"),P.MassR[I])||P.MassR[I]<.08||P.MassR[I]>.23)return Fail(__LINE__);}
    if(Interrupted){N.Capture(P,"interrupted");P.NewTrial();}
    // Recheck accepted answers against current measurements, not a saved trust flag.
    auto Refs=N.References();for(auto& Pair:N.Answers){auto Ref=Refs.find(Pair.first);if(Ref==Refs.end()||std::abs(Pair.second.Value-Ref->second.Value)>std::max(.005*std::abs(Ref->second.Value),1e-12))Pair.second.Correct=false;}
    Notebook=N;Experiment=P;Notice=Interrupted?TEXT("Работа восстановлена. Прерванная попытка исключена; подготовьте повтор."):TEXT("Работа восстановлена. Ранее принятые измерения в журнале.");return true;
}
void ALabScene::ExportLab(){
    FString Dir=SaveDirectory()/TEXT("exports")/(FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))+TEXT("-")+FGuid::NewGuid().ToString());IFileManager::Get().MakeDirectory(*Dir,true);
    FString M=TEXT("series_id,configuration,required,attempt_id,status,displayed_s,entered_s,true_h_m,true_R_m,true_pulley_m,true_J_kg_m2\n");
    for(auto& S:Notebook.Series)for(auto& A:S.Attempts)M+=FString::Printf(TEXT("%d,%d,%d,%d,%s,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n"),S.Id,S.Slot,S.N,A.Id,*Str(A.Status),A.Displayed,A.Entered,S.H,S.R,S.Radius,S.J);
    FString C=TEXT("kind,key,value_si,error_si,entered_unit,correct\n");for(auto& R:Notebook.Readings)C+=FString::Printf(TEXT("measurement,%s,%.12g,%.12g,%s,\n"),*Str(R.first),R.second.Value,R.second.Error,*Str(R.second.Unit));for(auto& A:Notebook.Answers)C+=FString::Printf(TEXT("answer,%s,%.12g,,%s,%s\n"),*Str(A.first),A.second.Value,*Str(A.second.Unit),A.second.Correct?TEXT("true"):TEXT("false"));
    bool Ok=FFileHelper::SaveStringToFile(M,*(Dir/TEXT("measurements.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)&&FFileHelper::SaveStringToFile(C,*(Dir/TEXT("calculations.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)&&FFileHelper::SaveStringToFile(Json(LabDocument()),*(Dir/TEXT("protocol.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    Notice=Ok?TEXT("Экспорт сохранён: ")+Dir:TEXT("Не удалось записать полный экспорт: ")+Dir;
}

FObHint ALabScene::NextHint()const {
    if(bSaveBlocked)return {"save_blocked","Сохранение не прочитано. Создайте новую работу из меню; исходный файл будет сохранён.","",""};
    return OberbeckNext(Notebook,Experiment,Replay);
}
FString ALabScene::TargetVisibilityError(const FString& Id)const {
    if(!FullMode)return FString();
    if(Id==TEXT("oberbeck.journal")||Id==TEXT("oberbeck.calculations")||Id==TEXT("oberbeck.apparatus")||Id==TEXT("oberbeck.review")||Id==TEXT("oberbeck.resume"))return FString();
    if(Id.StartsWith(TEXT("oberbeck.input."))||Id==TEXT("oberbeck.new_trial")||Id==TEXT("oberbeck.series.open")){
        if(NotebookView!=0)return TEXT("TARGET_HIDDEN: откройте вкладку Прибор (1)");
    }else if(Id==TEXT("oberbeck.attempt.input")||Id==TEXT("oberbeck.export")){
        if(NotebookView!=1)return TEXT("TARGET_HIDDEN: откройте вкладку Журнал (2)");
    }else if(Id==TEXT("oberbeck.answer.input")||Id==TEXT("oberbeck.conclusion")||Id==TEXT("oberbeck.coefficients")){
        if(NotebookView!=2)return TEXT("TARGET_HIDDEN: откройте вкладку Расчёты (3)");
    }else {auto M=OberbeckMeshes.Find(Id);return M&&(*M)->IsVisible()?FString():TEXT("TARGET_HIDDEN");}
    return VisibleUiTargets.Contains(Id)?FString():TEXT("TARGET_HIDDEN: прокрутите вкладку до элемента или выберите нужное задание");
}
