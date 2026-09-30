#include "LabPanel.h"
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
    ResultText=Text(TEXT("Поворачивайте и перемещайте мышью. Для разговора используйте наушники."),12);
    auto* Bottom=WidgetTree->ConstructWidget<UBorder>();Bottom->SetBrushColor(FLinearColor(0.035,0.065,0.085,0.94));Bottom->SetPadding(FMargin(16));Bottom->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* BS=Root->AddChildToCanvas(Bottom);BS->SetAnchors(FAnchors(0,1,1,1));BS->SetOffsets(FMargin(20,-100,20,84));
    Dialogue=WidgetTree->ConstructWidget<UTextBlock>();Dialogue->SetAutoWrapText(true);auto F=Dialogue->GetFont();F.Size=14;Dialogue->SetFont(F);Dialogue->SetText(FText::FromString(TEXT("Нажмите «Начать разговор» — помощник объяснит задание.")));Bottom->SetContent(Dialogue);
    return Super::RebuildWidget();
}
void ULabPanel::NativeTick(const FGeometry& G,float D) {
    Super::NativeTick(G,D);if(!Console.IsValid()||!StateText||!StartButton)return;
    if(auto* Scene=Cast<ALabScene>(UGameplayStatics::GetActorOfClass(this,ALabScene::StaticClass())))GoalText->SetText(FText::FromString(Scene->ProgressText()+TEXT("  •  ")+Scene->Goal()));
    StateText->SetText(FText::FromString(Console->Status+FString::Printf(TEXT("  Микрофон: %.0f%%"),Console->Level*100)));
    if(!Console->Transcript.IsEmpty())Dialogue->SetText(FText::FromString(Console->Transcript.Right(240)));
    if(!Console->ToolStatus.IsEmpty())ResultText->SetText(FText::FromString(Console->ToolStatus));
    StartButton->SetIsEnabled(Console->bConnected&&!Console->bReady&&!Console->bStarting);
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
