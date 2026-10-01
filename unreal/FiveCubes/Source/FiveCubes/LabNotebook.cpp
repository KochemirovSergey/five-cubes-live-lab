#include "LabNotebook.h"
#include "LabScene.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSlider.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace {
FText Txt(const FString& S){return FText::FromString(S);}
FString Fs(const std::string& S){return UTF8_TO_TCHAR(S.c_str());}
class SObDrawing : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(SObDrawing){} SLATE_ARGUMENT(TWeakObjectPtr<ALabScene>,Scene) SLATE_ARGUMENT(int,Kind) SLATE_END_ARGS()
    void Construct(const FArguments& A){Scene=A._Scene;Kind=A._Kind;}
    virtual void Tick(const FGeometry&,double,float)override{Invalidate(EInvalidateWidgetReason::Paint);}
    virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(360,Kind==3?240:90);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool)const override{
        if(!Scene.IsValid())return Layer;auto S=Scene.Get();const FVector2D Size=G.GetLocalSize();const float W=Size.X-24;const auto Font=FCoreStyle::GetDefaultFontStyle("Regular",10);
        auto Line=[&](FVector2D A,FVector2D B,FLinearColor C,float Width=1){TArray<FVector2D> P{A,B};FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,Width);};
        auto Text=[&](FString T,float X,float Y,FLinearColor C=FLinearColor::White){FSlateDrawElement::MakeText(Out,Layer+1,G.ToPaintGeometry(FVector2D(W,16),FSlateLayoutTransform(FVector2D(X,Y))),T,Font,ESlateDrawEffect::None,C);};
        FLinearColor Cyan(.2,.85,.95),Gold(1,.7,.15);
        if(Kind<3){
            double Max=Kind==0?.08:Kind==1?.25:.8;
            double Cursor=Kind==0?S->Caliper:Kind==1?S->RCursor:S->HCursor;
            double Target=Kind==0?2*S->Physics().Radius():Kind==1?S->Physics().MassR[0]:S->Physics().H;
            Text(Kind==0?TEXT("Губки: слева ноль, справа подвижная"):Kind==1?TEXT("Ноль — ось; цель — центр груза №1"):TEXT("Ноль — основание; цель — нижняя грань"),8,0);
            Line({12,58},{W+12,58},Cyan);for(int I=0;I<=10;I++){float X=12+W*I/10;Line({X,54},{X,62},Cyan);}
            float X=12+W*Target/Max,C=12+W*Cursor/Max;
            if(Kind==0){Line({12,23},{12,70},Gold,3);Line({12,35},{X,35},FLinearColor::Gray,16);Line({X,24},{X,50},FLinearColor::White);}
            else {Line({X,30},{X,52},FLinearColor::White,3);Text(Kind==1?TEXT("центр"):TEXT("подвес"),X-22,17);}
            Line({C,22},{C,72},Gold,3);Text(FString::Printf(TEXT("%.1f мм"),Cursor*1000),8,73,Gold);
            // Magnified inset: ±5 mm around the cursor, target alignment at centre.
            float Offset=FMath::Clamp((Target-Cursor)/.005,-1.,1.);Line({W-60,71},{W-60,86},Gold,2);Line({W-60+Offset*35,74},{W-60+Offset*35,86},FLinearColor::White,2);
        }else {
            int I=S->ReplaySeries;if(I<0||I>=(int)S->Notebook.Series.size())return Layer;
            auto& A=S->Notebook.Series[I];const FObSeries* B=S->CompareSeries>=0&&S->CompareSeries<(int)S->Notebook.Series.size()?&S->Notebook.Series[S->CompareSeries]:nullptr;
            double Duration=std::sqrt(2*A.H/A.Acceleration);if(B)Duration=std::max(Duration,std::sqrt(2*B->H/B->Acceleration));
            for(int Plot=0;Plot<3;Plot++){
                float Y=Plot*78+18;double Scale=Plot==0?std::max(A.H,B?B->H:0.):Plot==1?std::max(std::sqrt(2*A.H*A.Acceleration),B?std::sqrt(2*B->H*B->Acceleration):0.):std::max(A.H/A.Radius,B?B->H/B->Radius:0.);
                Text(Plot==0?TEXT("h(t), м"):Plot==1?TEXT("v(t), м/с"):TEXT("|θ(t)|, рад"),12,Y-17);Text(FString::Printf(TEXT("max %.3g"),Scale),W-100,Y-17);
                Line({12,Y+48},{W+12,Y+48},FLinearColor::Gray);Line({12,Y},{12,Y+48},FLinearColor::Gray);
                auto Curve=[&](const FObSeries& R,FLinearColor Color){FVector2D Prev;for(int J=0;J<=100;J++){double T=Duration*J/100,Hit=std::sqrt(2*R.H/R.Acceleration),E=std::min(T,Hit),Value=Plot==0?R.H-R.Acceleration*E*E/2:Plot==1?(T>Hit?0:R.Acceleration*E):R.Acceleration*E*E/(2*R.Radius);FVector2D P(12+W*J/100,Y+48-48*Value/Scale);if(J)Line(Prev,P,Color,2);Prev=P;}};
                Curve(A,Cyan);if(B)Curve(*B,Gold);float X=12+W*FMath::Clamp(S->ReplayTime/Duration,0.,1.);Line({X,Y},{X,Y+48},FLinearColor::White);
            }Text(FString::Printf(TEXT("t: 0 … %.2f с"),Duration),W-95,225);
        }return Layer+1;
    }
private:TWeakObjectPtr<ALabScene> Scene;int Kind=0;
};
class SObNotebook : public SCompoundWidget {
public:
    SLATE_BEGIN_ARGS(SObNotebook){} SLATE_ARGUMENT(TWeakObjectPtr<ALabScene>,Scene) SLATE_ARGUMENT(TWeakObjectPtr<UObject>,Context) SLATE_END_ARGS()
    void Construct(const FArguments& A){Scene=A._Scene;Context=A._Context;ChildSlot[SAssignNew(Body,SVerticalBox)];Build();}
    virtual void Tick(const FGeometry& G,double T,float D)override{SCompoundWidget::Tick(G,T,D);if(!Scene.IsValid()&&Context.IsValid())Scene=Cast<ALabScene>(UGameplayStatics::GetActorOfClass(Context.Get(),ALabScene::StaticClass()));if(!Scene.IsValid())return;int V=Scene->NotebookView,R=Scene->Notebook.Revision;if(V!=View||R!=Revision||WasFull!=Scene->FullMode||WorkVersion!=Scene->SceneIdentity())Build();
        for(auto& Entry:FieldWidgets)if(auto W=Entry.Value.Pin();W&&W->HasKeyboardFocus())Scene->SelectedField=Entry.Key;
        Scene->VisibleUiTargets.Empty();
        if(Scroll){auto Clip=Scroll->GetCachedGeometry().GetLayoutBoundingRect();for(auto& Entry:TargetWidgets){auto W=Entry.Widget.Pin();if(!W||!Selected(Entry.Field))continue;auto Rect=W->GetCachedGeometry().GetLayoutBoundingRect();if(Rect.Right>Clip.Left&&Rect.Left<Clip.Right&&Rect.Top>=Clip.Top&&Rect.Bottom<=Clip.Bottom&&Rect.Bottom>Rect.Top)Scene->VisibleUiTargets.Add(Entry.Id);}}
    }
private:
    TWeakObjectPtr<ALabScene> Scene;TWeakObjectPtr<UObject> Context;TSharedPtr<SVerticalBox> Body,Content;FString WorkVersion;int View=-1,Revision=-1;bool WasFull=false,Hint=false;
    TMap<FString,FString> Input,Units;TArray<TSharedPtr<TArray<TSharedPtr<FString>>>> Options;
    struct FTargetWidget {FString Id,Field;TWeakPtr<SWidget> Widget;};
    TMap<FString,TWeakPtr<SEditableTextBox>> FieldWidgets;
    TArray<FTargetWidget> TargetWidgets;TSharedPtr<SScrollBox> Scroll;
    bool Selected(const FString& Field)const {
        if(Field.IsEmpty())return true;
        if(Field.StartsWith(TEXT("answer:")))return Field==(Scene->SelectedField.StartsWith(TEXT("answer:"))?Scene->SelectedField:Fs(Scene->NextHint().Field));
        if(Field.StartsWith(TEXT("time:")))return Field==Fs(Scene->NextHint().Field);
        return true;
    }
    TSharedRef<SWidget> Mark(FString Id,TSharedRef<SWidget> Child,FString Field=TEXT("")){
        auto Border=SNew(SBorder).Padding(2).BorderBackgroundColor_Lambda([this,Id,Field](){return Selected(Field)&&Scene->FullHighlight()==Id?FLinearColor(1,.7,0):FLinearColor::Transparent;})[Child];
        TargetWidgets.Add({Id,Field,Border});return Border;
    }
    void Text(const FString& Value,int Size=12){Content->AddSlot().AutoHeight().Padding(0,3)[SNew(STextBlock).Text(Txt(Value)).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size))];}
    TSharedRef<SButton> Button(FString Label,TFunction<void()> Action){return SNew(SButton).ContentPadding(FMargin(7,5)).OnClicked_Lambda([Action](){Action();return FReply::Handled();})[SNew(STextBlock).Text(Txt(Label)).Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).AutoWrapText(true)];}
    void Act(FString Id,FString T=TEXT(""),FString U=TEXT("")){if(Scene.IsValid())Scene->FullAction(Id,T,U);}
    void Field(FString Id,FString Label,FString DefaultUnit,TArray<FString> Choices,FString Value=TEXT("")){
        if(!Input.Contains(Id))Input.Add(Id,Value);if(!Units.Contains(Id))Units.Add(Id,DefaultUnit);
        auto Opt=MakeShared<TArray<TSharedPtr<FString>>>();for(auto& U:Choices)Opt->Add(MakeShared<FString>(U));Options.Add(Opt);
        Text(Label);auto Row=SNew(SHorizontalBox);
        TSharedPtr<SEditableTextBox> Edit;
        Row->AddSlot().FillWidth(1)[SAssignNew(Edit,SEditableTextBox).Text(Txt(Input[Id])).HintText(Txt(TEXT("Введите число"))).OnTextChanged_Lambda([this,Id](const FText& T){Input[Id]=T.ToString();Scene->SelectedField=Id;})];
        FieldWidgets.Add(Id,Edit);
        Row->AddSlot().AutoWidth().Padding(4,0)[SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&Opt.Get()).OnGenerateWidget_Lambda([](TSharedPtr<FString> S){return SNew(STextBlock).Text(Txt(*S));}).OnSelectionChanged_Lambda([this,Id](TSharedPtr<FString> U,ESelectInfo::Type){if(U)Units[Id]=*U;})[SNew(STextBlock).Text_Lambda([this,Id](){return Txt(Units[Id]);})]];
        Row->AddSlot().AutoWidth()[Button(TEXT("Проверить"),[this,Id](){Act(Id,Input[Id],Units[Id]);})];FString Target=Id.StartsWith(TEXT("measure:"))?TEXT("oberbeck.input.")+Id.Mid(8):Id.StartsWith(TEXT("time:"))?TEXT("oberbeck.attempt.input"):Id==TEXT("coefficients")?TEXT("oberbeck.coefficients"):TEXT("oberbeck.answer.input");
        Content->AddSlot().AutoHeight().Padding(0,2)[Mark(Target,Row,Id)];
    }
    void Instrument(int Kind,FString Id,FString Title){
        auto S=Scene.Get();Text(Title,14);Content->AddSlot().AutoHeight()[SNew(SObDrawing).Scene(Scene).Kind(Kind)];
        double Max=Kind==0?.08:Kind==1?.25:.8,Step=Kind==0?.0001:.001;
        auto Get=[this,Kind](){auto S=Scene.Get();return Kind==0?S->Caliper:Kind==1?S->RCursor:S->HCursor;};
        auto Row=SNew(SHorizontalBox);
        Row->AddSlot().AutoWidth()[Button(TEXT("−"),[this,Id,Get,Step](){Scene->SetInstrument(Id,Get()-Step);})];
        Row->AddSlot().FillWidth(1).Padding(8,0)[SNew(SSlider).Value_Lambda([Get,Max](){return float(Get()/Max);}).OnValueChanged_Lambda([this,Id,Max,Step](float V){Scene->SetInstrument(Id,FOberbeckLab::Rounded(V*Max,Step));})];
        Row->AddSlot().AutoWidth()[Button(TEXT("+"),[this,Id,Get,Step](){Scene->SetInstrument(Id,Get()+Step);})];Content->AddSlot().AutoHeight()[Row];
    }
    void Build(){
        if(!Body||!Scene.IsValid())return;auto S=Scene.Get();if(!S->FullMode||WorkVersion!=S->SceneIdentity()){Input.Empty();Units.Empty();}WorkVersion=S->SceneIdentity();
        View=S->NotebookView;Revision=S->Notebook.Revision;WasFull=S->FullMode;Body->ClearChildren();Options.Empty();TargetWidgets.Empty();FieldWidgets.Empty();if(!WasFull)return;
        auto Nav=SNew(SHorizontalBox);const TCHAR* Names[]={TEXT("Прибор"),TEXT("Журнал"),TEXT("Расчёты"),TEXT("Разбор")};for(int I=0;I<4;I++)Nav->AddSlot().FillWidth(1).Padding(2)[SNew(SBorder).Padding(2).BorderBackgroundColor_Lambda([this,I](){FString Id=I==1?TEXT("oberbeck.journal"):I==2?TEXT("oberbeck.calculations"):I==0?TEXT("oberbeck.apparatus"):TEXT("oberbeck.review");return Scene->FullHighlight()==Id?FLinearColor(1,.7,0):FLinearColor::Transparent;})[Button(Names[I],[this,I](){Scene->NotebookView=I;})]];Body->AddSlot().AutoHeight()[Nav];
        Body->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this](){return Txt(Scene->ActiveHighlight().IsEmpty()?FString():TEXT("Подсветка: ")+Scene->ActiveHighlightCaption());}).AutoWrapText(true)];
        Body->AddSlot().AutoHeight()[Mark(TEXT("oberbeck.resume"),Button(TEXT("Продолжить после паузы"),[this](){Scene->ExperimentAction(TEXT("resume"));}))];
        Body->AddSlot().AutoHeight()[Button(TEXT("Прервать попытку"),[this](){Scene->ExperimentAction(TEXT("new"));})];
        Body->AddSlot().AutoHeight().Padding(0,6)[SNew(STextBlock).Text_Lambda([this](){return Txt(Scene->FullStatus());}).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular",12))];
        Body->AddSlot().FillHeight(1)[SAssignNew(Scroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(Content,SVerticalBox)]];
        Content->SetEnabled(TAttribute<bool>::CreateLambda([this](){return !Scene->Physics().Falling&&!Scene->Physics().TimerRunning&&!Scene->Replay;}));
        if(View==0){
            Text(TEXT("Отдельные грузы: перетащите каждый на подписанное место слева или обратно вдоль своей спицы. При сближении R до 3 мм радиусы защёлкиваются. Секундомер — пробел."));
            auto Actions=SNew(SHorizontalBox);Actions->AddSlot().FillWidth(1)[Mark(TEXT("oberbeck.new_trial"),Button(TEXT("Новый опыт"),[this](){Scene->ExperimentAction(TEXT("new"));}))];Actions->AddSlot().FillWidth(1)[Button(TEXT("Продолжить"),[this](){Scene->ExperimentAction(TEXT("resume"));})];Content->AddSlot().AutoHeight()[Actions];
            Instrument(0,TEXT("oberbeck.caliper"),TEXT("Штангенциркуль • диаметр выбранного шкива"));
            Field(TEXT("measure:D0"),TEXT("Диаметр малого шкива (нажмите малый шкив)"),TEXT("mm"),{TEXT("mm"),TEXT("cm"),TEXT("m")});Field(TEXT("measure:D1"),TEXT("Диаметр большого шкива (нажмите большой шкив)"),TEXT("mm"),{TEXT("mm"),TEXT("cm"),TEXT("m")});
            Instrument(1,TEXT("oberbeck.ruler"),TEXT("Линейка R • центр груза №1, все R равны"));Field(TEXT("measure:R"),TEXT("R нагруженного маховика (22–23 см)"),TEXT("cm"),{TEXT("cm"),TEXT("mm"),TEXT("m")});
            Instrument(2,TEXT("oberbeck.height_cursor"),TEXT("Высота • совместите с нижней гранью подвеса"));Field(TEXT("measure:h"),TEXT("Начальная высота h"),TEXT("cm"),{TEXT("cm"),TEXT("mm"),TEXT("m")});
            Text(TEXT("Серия фиксирует шкив, грузы и высоту. Первое открытие устанавливает ограничитель намотки; последующие опыты поднимайте до упора. Новая серия заменяет расчётную серию этой конфигурации, сохраняя историю."));
            auto Ns=SNew(SHorizontalBox);for(int N=3;N<=5;N++)Ns->AddSlot().FillWidth(1)[Button(FString::Printf(TEXT("%d повтора"),N),[this,N](){Scene->Repeats=N;})];Content->AddSlot().AutoHeight()[Ns];Content->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this](){return Txt(FString::Printf(TEXT("Выбрано повторов: %d"),Scene->Repeats));})];Content->AddSlot().AutoHeight()[Mark(TEXT("oberbeck.series.open"),Button(TEXT("Открыть новую серию"),[this](){Act(TEXT("series"));}))];
        }else if(View==1){
            Text(TEXT("Показание секундомера переносится вручную. Неуспешные и исключённые попытки остаются в истории; среднее считается только по принятым."));
            for(auto& R:S->Notebook.Readings)Text(FString::Printf(TEXT("%s: %.6g м; Δ = %.6g м"),*Fs(R.first),R.second.Value,R.second.Error));
            for(auto& Series:S->Notebook.Series){Text(FString::Printf(TEXT("Серия №%d • %s • шкив %d • %d/%d"),Series.Id,Series.Slot<2?TEXT("без грузов"):TEXT("с грузами"),Series.Slot%2+1,Series.Count(),Series.N),15);
                for(auto& A:Series.Attempts){Text(FString::Printf(TEXT("Попытка №%d: %.2f с • %s"),A.Id,A.Displayed,*Fs(A.Status)));if(A.Status=="pending"||A.Status=="accepted")Field(FString::Printf(TEXT("time:%d"),A.Id),TEXT("Отсчёт, с"),TEXT("s"),{TEXT("s")},A.Status=="accepted"?FString::Printf(TEXT("%.2f"),A.Entered):TEXT(""));if(A.Status!="excluded")Content->AddSlot().AutoHeight()[Button(TEXT("Исключить попытку (сохранить историю)"),[this,Id=A.Id](){Act(FString::Printf(TEXT("exclude:%d"),Id));})];}
            }
            Content->AddSlot().AutoHeight()[Mark(TEXT("oberbeck.export"),Button(TEXT("Экспорт CSV + JSON"),[this](){Act(TEXT("export"));}))];Text(TEXT("Работа сохраняется автоматически в Application Support/FiveCubes/Labs/oberbeck. В меню доступно продолжение."));
        }else if(View==2){
            auto Tasks=SNew(SHorizontalBox);for(int I=0;I<5;I++)Tasks->AddSlot().FillWidth(1)[Button(FString::FromInt(I+1),[this,I](){Scene->CalculationTask=I;Scene->SelectedField.Empty();Build();})];Content->AddSlot().AutoHeight()[Tasks];Text(FString::Printf(TEXT("Расчёты задания %d"),Scene->CalculationTask+1),16);
            Content->AddSlot().AutoHeight()[Button(Hint?TEXT("Скрыть формулы"):TEXT("Показать формулы и методику"),[this](){Hint=!Hint;Build();})];
            if(Hint)Text(TEXT("m₀ = 0,100 кг, m = 0,050 кг; g = 9,81 м/с². Погрешности масс ±0,5 г, D ±0,1 мм, r ±0,05 мм, R и h ±1 мм. Δt = max|tᵢ−t̄| + 0,20 + 0,005 с. 0,20 с — учебное допущение реакции, не калибровка и не доверительный интервал. Проверка арифметики: 0,5%; физическая погрешность не расширяет этот допуск."));
            auto Refs=S->Notebook.References();int Count=0;
            for(auto& Pair:Refs)if(Pair.second.Task==Scene->CalculationTask){Count++;auto& R=Pair.second;FString Key=Fs(Pair.first),U=Fs(R.Unit);auto It=S->Notebook.Answers.find(Pair.first);bool Correct=It!=S->Notebook.Answers.end()&&It->second.Correct;
                if(Hint)Text(Fs(R.Formula));TArray<FString> Choices{U};if(U==TEXT("m")){Choices.Add(TEXT("cm"));Choices.Add(TEXT("mm"));}if(U==TEXT("1"))Choices.Add(TEXT("%"));if(U==TEXT("kg*m2"))Choices.Add(TEXT("g*cm2"));
                FString Saved;FString SavedUnit=U;if(It!=S->Notebook.Answers.end()){SavedUnit=Fs(It->second.Unit);double Factor=1;FOberbeckLab::Parse("1",It->second.Unit,Factor);Saved=FString::Printf(TEXT("%.9g"),It->second.Value/Factor);}
                Field(TEXT("answer:")+Key,Fs(R.Label)+(Correct?TEXT(" ✓"):TEXT("")),SavedUnit,Choices,Saved);
            }
            if(!Count)Text(TEXT("Поля появятся после необходимых измерений и завершения серий. Если расчёт J даёт ноль или отрицательное значение, проверьте высоту и ручное время и повторите измерение."));
            if(Scene->CalculationTask==0||Scene->CalculationTask==1||Scene->CalculationTask==3){int Pair=Scene->CalculationTask==0?0:Scene->CalculationTask==1?1:2;Text(Scene->CalculationTask==3?TEXT("Совпадают ли M/α двух шкивов в пределах суммы ΔJ?"):TEXT("Согласуются ли две оценки J в пределах суммы ΔJ?"));Content->AddSlot().AutoHeight()[Mark(TEXT("oberbeck.conclusion"),Button(TEXT("Да, в пределах погрешности"),[this,Pair](){Act(FString::Printf(TEXT("conclusion:%d"),Pair),TEXT("yes"));}))];Content->AddSlot().AutoHeight()[Mark(TEXT("oberbeck.conclusion"),Button(TEXT("Нет, требуется повтор измерений"),[this,Pair](){Act(FString::Printf(TEXT("conclusion:%d"),Pair),TEXT("no"));}))];}
            if(Scene->CalculationTask==2&&Hint)Text(TEXT("Разность включает собственную инерцию протяжённого груза. mR² — приближение точечной массы; измеренный результат может отличаться также из-за ручного отсчёта времени."));
            if(Scene->CalculationTask==4){Field(TEXT("coefficients"),TEXT("Коэффициенты при Δm₀/m₀, Δr/r, Δt/t, Δh/h (через запятую)"),TEXT("1"),{TEXT("1")});Text(TEXT("Приближённые поля доступны только для малых шкивов при a/g ≤ 0,05. Если условие не выполнено, используйте точное распространение погрешностей. Погрешность среднего и погрешность приближённой отдельной серии — разные результаты."));if(Hint)Text(TEXT("ΔJ = r²|B|Δm₀ + 2m₀r|B|Δr + m₀r²gt/h Δt + m₀r²gt²/(2h²)Δh; B = gt²/(2h) − 1."));}
        }else {
            Content->SetEnabled(true);Text(TEXT("Разбор записи модели. Голубая кривая — выбранная серия, жёлтая — сравнение. Это расчётная траектория, а не ручной отсчёт секундомера."));
            for(int I=0;I<(int)S->Notebook.Series.size();I++)if(S->Notebook.Series[I].Complete()){auto Row=SNew(SHorizontalBox);Row->AddSlot().FillWidth(1)[Button(FString::Printf(TEXT("Серия №%d"),S->Notebook.Series[I].Id),[this,I](){Scene->ReplaySeries=I;Scene->ReplayTime=0;Build();})];Row->AddSlot().FillWidth(1)[Button(TEXT("Сравнить с ней"),[this,I](){Scene->CompareSeries=I;Build();})];Content->AddSlot().AutoHeight()[Row];}
            Content->AddSlot().AutoHeight()[SNew(SObDrawing).Scene(Scene).Kind(3)];auto Rates=SNew(SHorizontalBox);for(double Rate:{.25,.5,1.})Rates->AddSlot().FillWidth(1)[Button(FString::Printf(TEXT("×%.2g"),Rate),[this,Rate](){Scene->ReplaySpeed=Rate;})];Content->AddSlot().AutoHeight()[Rates];
            Content->AddSlot().AutoHeight()[Button(TEXT("Воспроизвести модель"),[this](){Act(TEXT("replay"));})];Content->AddSlot().AutoHeight()[Button(TEXT("Вернуться к установке"),[this](){Act(TEXT("replay_stop"));})];Content->AddSlot().AutoHeight()[Button(TEXT("Показать / скрыть силы и R"),[this](){Act(TEXT("forces"));Build();})];
            if(S->ShowForces)Text(TEXT("Стрелки показывают направления, их длина не масштабирует силу. На подвес: m₀g ↓, T ↑. На шкив: момент Tr; α = a/r. R — расстояние от оси до центра подвижного груза. Сила тяжести грузов на симметричном маховике не даёт суммарного момента."));
        }
    }
};
}
TSharedRef<SWidget> ULabNotebook::RebuildWidget(){auto* Scene=Cast<ALabScene>(UGameplayStatics::GetActorOfClass(this,ALabScene::StaticClass()));return SNew(SObNotebook).Scene(Scene).Context(this);}
