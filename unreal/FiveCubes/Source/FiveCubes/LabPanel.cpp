#include "LabPanel.h"
#include "LabNotebook.h"
#include "Framework/Application/SlateApplication.h"
#include "VoiceConsole.h"
#include "LabScene.h"
#include "PanelController.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
TSharedRef<SWidget> ULabPanel::RebuildWidget() {
    if(WidgetTree->RootWidget)return Super::RebuildWidget();
    SetVisibility(ESlateVisibility::Visible);
    auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);WidgetTree->RootWidget=Root;
    auto* Border=WidgetTree->ConstructWidget<UBorder>();Border->SetBrushColor(FLinearColor(0.035,0.065,0.085,0.96));Border->SetPadding(FMargin(16,10));
    auto* Slot=Root->AddChildToCanvas(Border);Slot->SetAnchors(FAnchors(0,0,1,0));Slot->SetOffsets(FMargin(20,16,20,142));
    auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();Border->SetContent(Column);
    auto Text=[&](const TCHAR* Value,int Size){auto* T=WidgetTree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(Value));auto F=T->GetFont();F.Size=Size;T->SetFont(F);Column->AddChildToVerticalBox(T);return T;};
    GoalText=Text(TEXT("Учебный пульт"),20);StateText=Text(TEXT("Запуск…"),12);
    auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Column->AddChildToVerticalBox(Row);
    auto Button=[&](const TCHAR* Caption){auto* B=WidgetTree->ConstructWidget<UButton>();auto* T=WidgetTree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(Caption));auto F=T->GetFont();F.Size=13;T->SetFont(F);T->SetColorAndOpacity(FSlateColor(FLinearColor(0.02,0.04,0.05)));B->SetContent(T);auto* BS=Row->AddChildToHorizontalBox(B);BS->SetPadding(FMargin(0,7,12,4));return B;};
    StartButton=Button(TEXT("Начать разговор"));StartButton->OnClicked.AddDynamic(this,&ULabPanel::Start);
    Button(TEXT("Завершить разговор"))->OnClicked.AddDynamic(this,&ULabPanel::Stop);
    Button(TEXT("Снять подсветку"))->OnClicked.AddDynamic(this,&ULabPanel::Clear);
    Button(TEXT("Начать заново"))->OnClicked.AddDynamic(this,&ULabPanel::Reset);
    MenuButton=Button(TEXT("Выбор лабораторной"));MenuButton->OnClicked.AddDynamic(this,&ULabPanel::Menu);
    ResultText=Text(TEXT("Поворачивайте и перемещайте мышью. Для разговора используйте наушники."),12);
    auto* Bottom=WidgetTree->ConstructWidget<UBorder>();Bottom->SetBrushColor(FLinearColor(0.035,0.065,0.085,0.94));Bottom->SetPadding(FMargin(16));Bottom->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* BS=Root->AddChildToCanvas(Bottom);BS->SetAnchors(FAnchors(0,1,1,1));BS->SetOffsets(FMargin(20,-100,20,84));
    Dialogue=WidgetTree->ConstructWidget<UTextBlock>();Dialogue->SetAutoWrapText(true);auto F=Dialogue->GetFont();F.Size=14;Dialogue->SetFont(F);Dialogue->SetText(FText::FromString(TEXT("Нажмите «Начать разговор» — помощник объяснит задание.")));Bottom->SetContent(Dialogue);
    auto Floating=[&](FVector2D Position,FVector2D Size){auto* B=WidgetTree->ConstructWidget<UBorder>();B->SetPadding(FMargin(22));B->SetBrushColor(FLinearColor(.055,.095,.12,.97));auto* S=Root->AddChildToCanvas(B);S->SetAnchors(FAnchors(.5,.5));S->SetPosition(Position);S->SetSize(Size);return B;};
    auto AddText=[&](UVerticalBox* C,const TCHAR* Value,int Size){auto* T=WidgetTree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(Value));T->SetAutoWrapText(true);auto F=T->GetFont();F.Size=Size;T->SetFont(F);C->AddChildToVerticalBox(T);return T;};
    auto AddButton=[&](UVerticalBox* C,const TCHAR* Value){auto* B=WidgetTree->ConstructWidget<UButton>();auto* T=WidgetTree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(Value));auto F=T->GetFont();F.Size=16;T->SetFont(F);T->SetAutoWrapText(true);T->SetColorAndOpacity(FSlateColor(FLinearColor(.02,.04,.05)));B->SetContent(T);C->AddChildToVerticalBox(B);return B;};
    MenuBox=Floating(FVector2D(-280,-145),FVector2D(560,360));auto* MC=WidgetTree->ConstructWidget<UVerticalBox>();MenuBox->SetContent(MC);
    AddText(MC,TEXT("Виртуальная лаборатория"),26);
    AddText(MC,TEXT("Выберите установку. Управление мышью, голосовая помощь по запросу.\n"),16);
    AddButton(MC,TEXT("Прибор Обербека — первый опыт"))->OnClicked.AddDynamic(this,&ULabPanel::ChooseOberbeck);
    AddText(MC,TEXT("\n"),8);
    AddButton(MC,TEXT("Обербек — полная лабораторная: новая работа"))->OnClicked.AddDynamic(this,&ULabPanel::ChooseFull);
    AddButton(MC,TEXT("Обербек — продолжить сохранённую работу"))->OnClicked.AddDynamic(this,&ULabPanel::ContinueFull);
    AddButton(MC,TEXT("Учебный пульт — четыре действия"))->OnClicked.AddDynamic(this,&ULabPanel::ChoosePanel);
    ExperimentBox=Floating(FVector2D(280,-170),FVector2D(330,430));auto* EC=WidgetTree->ConstructWidget<UVerticalBox>();ExperimentBox->SetContent(EC);
    AddText(EC,TEXT("Измерение"),22);ExperimentReadout=AddText(EC,TEXT(""),15);
    AddButton(EC,TEXT("Новый опыт"))->OnClicked.AddDynamic(this,&ULabPanel::NewTrial);
    ResumeButton=AddButton(EC,TEXT("Продолжить"));ResumeButton->OnClicked.AddDynamic(this,&ULabPanel::Resume);
    MenuCaption=AddText(EC,TEXT("Вернуться к выбору? Текущий прогресс будет сброшен. Нажмите «Выбор лабораторной» ещё раз."),13);
    CancelMenuButton=AddButton(EC,TEXT("Остаться в опыте"));CancelMenuButton->OnClicked.AddDynamic(this,&ULabPanel::CancelMenu);
    NotebookBox=WidgetTree->ConstructWidget<UBorder>();NotebookBox->SetPadding(FMargin(12));NotebookBox->SetBrushColor(FLinearColor(.035,.065,.085,.97));
    auto* NS=Root->AddChildToCanvas(NotebookBox);NS->SetAnchors(FAnchors(1,0,1,1));NS->SetOffsets(FMargin(-475,166,455,116));
    NotebookBox->SetContent(WidgetTree->ConstructWidget<ULabNotebook>());
    return Super::RebuildWidget();
}
void ULabPanel::NativeTick(const FGeometry& G,float D) {
    Super::NativeTick(G,D);if(!Console.IsValid()||!StateText||!StartButton)return;
    auto* Scene=Cast<ALabScene>(UGameplayStatics::GetActorOfClass(this,ALabScene::StaticClass()));
    if(Scene){
        GoalText->SetText(FText::FromString(Scene->ProgressText()+TEXT("  •  ")+Scene->Goal()));
        const bool IsMenu=Scene->ActiveLab==TEXT("menu"),IsOberbeck=Scene->ActiveLab==TEXT("oberbeck");
        MenuBox->SetVisibility(IsMenu?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
        NotebookBox->SetVisibility(IsOberbeck&&Scene->FullMode?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
        ExperimentBox->SetVisibility((IsOberbeck&&!Scene->FullMode)||Scene->bConfirmMenu?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
        ExperimentReadout->SetText(FText::FromString(IsOberbeck?Scene->ExperimentText():TEXT("")));
        ResumeButton->SetVisibility(Scene->IsPaused()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
        MenuCaption->SetVisibility(Scene->bConfirmMenu?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
        CancelMenuButton->SetVisibility(Scene->bConfirmMenu?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
        if(IsMenu)ResultText->SetText(FText::FromString(Scene->Notice.IsEmpty()?TEXT("Выберите установку для начала работы."):Scene->Notice));
        if(IsOberbeck)ResultText->SetText(FText::FromString(Scene->FullMode?TEXT("Без трения • 1–4: прибор, журнал, расчёты, разбор • Пробел: секундомер • Перетаскивание мышью"):TEXT("Демонстрационная установка без трения. Грузы — вдоль спиц; маховик — по окружности. Пробел: секундомер.")));
    }
    StateText->SetText(FText::FromString(Console->Status+FString::Printf(TEXT("  Микрофон: %.0f%%"),Console->Level*100)));
    if(!Console->Transcript.IsEmpty())Dialogue->SetText(FText::FromString(Console->Transcript.Right(240)));
    if(Scene&&Scene->ActiveLab==TEXT("training_panel")&&!Console->ToolStatus.IsEmpty())ResultText->SetText(FText::FromString(Console->ToolStatus));
    StartButton->SetIsEnabled(Scene&&Scene->ActiveLab!=TEXT("menu")&&Console->bConnected&&!Console->bReady&&!Console->bStarting);
}
void ULabPanel::Start(){if(Console.IsValid())Console->StartVoice();}
void ULabPanel::Stop(){if(Console.IsValid())Console->StopVoice();}
void ULabPanel::Clear(){if(Console.IsValid())Console->ClearHighlight();}
void ULabPanel::Reset(){if(auto* Scene=Cast<ALabScene>(UGameplayStatics::GetActorOfClass(this,ALabScene::StaticClass())))Scene->ResetExercise();}

FReply ULabPanel::NativeOnPreviewMouseButtonDown(const FGeometry& G,const FPointerEvent& E){
    const auto P=G.AbsoluteToLocal(E.GetScreenSpacePosition());
    UE_LOG(LogTemp,Display,TEXT("Panel HUD press: %.1f %.1f"),P.X,P.Y);
    return Super::NativeOnPreviewMouseButtonDown(G,E);
}
FReply ULabPanel::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& E) {
    if(E.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
    if(auto* PC=Cast<APanelController>(GetOwningPlayer())){FVector2D Pixel,Viewport;USlateBlueprintLibrary::AbsoluteToViewport(this,E.GetScreenSpacePosition(),Pixel,Viewport);PC->Press(Pixel);}
    return FReply::Handled().CaptureMouse(TakeWidget());
}
FReply ULabPanel::NativeOnMouseButtonDoubleClick(const FGeometry& G,const FPointerEvent& E){return NativeOnMouseButtonDown(G,E);}
FReply ULabPanel::NativeOnMouseButtonUp(const FGeometry& G,const FPointerEvent& E){
    if(E.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
    if(auto* PC=Cast<APanelController>(GetOwningPlayer()))PC->Release();
    return FReply::Handled().ReleaseMouseCapture();
}
FReply ULabPanel::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& E){
    if(HasMouseCapture()) {if(auto* PC=Cast<APanelController>(GetOwningPlayer())){FVector2D Pixel,Viewport;USlateBlueprintLibrary::AbsoluteToViewport(this,E.GetScreenSpacePosition(),Pixel,Viewport);PC->Drag(Pixel);}return FReply::Handled();}
    return FReply::Unhandled();
}
void ULabPanel::NativeOnMouseCaptureLost(const FCaptureLostEvent& E){
    if(auto* PC=Cast<APanelController>(GetOwningPlayer()))PC->Release();
    Super::NativeOnMouseCaptureLost(E);
}

static ALabScene* CurrentScene(UUserWidget* W){return Cast<ALabScene>(UGameplayStatics::GetActorOfClass(W,ALabScene::StaticClass()));}
void ULabPanel::ChooseFull(){if(auto* S=CurrentScene(this))S->SelectFull(false);}
void ULabPanel::ContinueFull(){if(auto* S=CurrentScene(this))S->SelectFull(true);}
void ULabPanel::ChoosePanel(){if(auto* S=CurrentScene(this))S->SelectLab(TEXT("training_panel"));}
void ULabPanel::ChooseOberbeck(){if(auto* S=CurrentScene(this))S->SelectLab(TEXT("oberbeck"));}
void ULabPanel::Menu(){if(auto* S=CurrentScene(this))S->RequestMenu();}
void ULabPanel::CancelMenu(){if(auto* S=CurrentScene(this))S->bConfirmMenu=false;}
void ULabPanel::NewTrial(){if(auto* S=CurrentScene(this))S->ExperimentAction(TEXT("new"));}
void ULabPanel::Resume(){if(auto* S=CurrentScene(this))S->ExperimentAction(TEXT("resume"));}
FReply ULabPanel::NativeOnPreviewKeyDown(const FGeometry& G,const FKeyEvent& E){
    auto Focus=FSlateApplication::Get().GetKeyboardFocusedWidget();
    if(Focus.IsValid()&&Focus->GetTypeAsString().Contains(TEXT("EditableText")))return Super::NativeOnPreviewKeyDown(G,E);
    // Plain number shortcuts apply only outside editable fields.
    if(!E.IsControlDown()&&!E.IsAltDown()&&!E.IsShiftDown()&&!E.IsCommandDown()){
        if(auto* S=CurrentScene(this);S&&S->FullMode){
            const FKey Tabs[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four};
            for(int I=0;I<4;I++)if(E.GetKey()==Tabs[I]){S->NotebookView=I;return FReply::Handled();}
        }
    }
    if(E.GetKey()==EKeys::SpaceBar)if(auto* S=CurrentScene(this);S&&S->ActiveLab==TEXT("oberbeck")){if(!E.IsRepeat())S->ExperimentAction(TEXT("oberbeck.timer.toggle"));return FReply::Handled();}
    return Super::NativeOnPreviewKeyDown(G,E);
}
