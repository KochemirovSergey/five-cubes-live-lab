#include "LabPanel.h"
#include "VoiceConsole.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
TSharedRef<SWidget> ULabPanel::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
    auto* Border=WidgetTree->ConstructWidget<UBorder>(); Border->SetBrushColor(FLinearColor(0.015,0.025,0.05,0.94)); Border->SetPadding(FMargin(18));
    auto* Slot=Root->AddChildToCanvas(Border); Slot->SetAnchors(FAnchors(0,0)); Slot->SetPosition(FVector2D(20,20)); Slot->SetSize(FVector2D(620,280));
    auto* Column=WidgetTree->ConstructWidget<UVerticalBox>(); Border->SetContent(Column);
    auto AddText=[&](const TCHAR* Value) { auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Value)); T->SetAutoWrapText(true); auto F=T->GetFont(); F.Size=16; T->SetFont(F); Column->AddChildToVerticalBox(T); return T; };
    AddText(TEXT("ЛАБОРАТОРИЯ • ПЯТЬ КУБИКОВ")); StateText=AddText(TEXT("Запуск…"));
    auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>(); Column->AddChildToVerticalBox(Row);
    auto Button=[&](const TCHAR* Title) { auto* B=WidgetTree->ConstructWidget<UButton>(); auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Title)); auto F=T->GetFont(); F.Size=14; T->SetFont(F); T->SetColorAndOpacity(FSlateColor(FLinearColor::Black)); B->SetContent(T); Row->AddChildToHorizontalBox(B); return B; };
    StartButton=Button(TEXT("Начать разговор")); StartButton->OnClicked.AddDynamic(this,&ULabPanel::Start);
    Button(TEXT("Завершить"))->OnClicked.AddDynamic(this,&ULabPanel::Stop);
    Button(TEXT("Снять подсветку"))->OnClicked.AddDynamic(this,&ULabPanel::Clear);
    Row=WidgetTree->ConstructWidget<UHorizontalBox>(); Column->AddChildToVerticalBox(Row);
    Button(TEXT("  1  "))->OnClicked.AddDynamic(this,&ULabPanel::One); Button(TEXT("  2  "))->OnClicked.AddDynamic(this,&ULabPanel::Two);
    Button(TEXT("  3  "))->OnClicked.AddDynamic(this,&ULabPanel::Three); Button(TEXT("  4  "))->OnClicked.AddDynamic(this,&ULabPanel::Four); Button(TEXT("  5  "))->OnClicked.AddDynamic(this,&ULabPanel::Five);
    ResultText=AddText(TEXT("Для свободного разговора используйте наушники.")); Dialogue=AddText(TEXT("Нажмите «Начать разговор» и попросите подсветить кубик."));
    return Super::RebuildWidget();
}
void ULabPanel::NativeTick(const FGeometry& G,float D)
{
    Super::NativeTick(G,D); if (!Console.IsValid() || !StateText || !StartButton) return;
    StateText->SetText(FText::FromString(Console->Status+FString::Printf(TEXT("  •  Микрофон: %.0f%%"),Console->Level*100)));
    if (!Console->Transcript.IsEmpty()) Dialogue->SetText(FText::FromString(Console->Transcript.Right(340)));
    if (!Console->ToolStatus.IsEmpty()) ResultText->SetText(FText::FromString(Console->ToolStatus));
    StartButton->SetIsEnabled(Console->bConnected && !Console->bReady && !Console->bStarting);
}
void ULabPanel::Start(){if(Console.IsValid())Console->StartVoice();}
void ULabPanel::Stop(){if(Console.IsValid())Console->StopVoice();}
void ULabPanel::Clear(){if(Console.IsValid())Console->Command(0);}
void ULabPanel::One(){if(Console.IsValid())Console->Command(1);}
void ULabPanel::Two(){if(Console.IsValid())Console->Command(2);}
void ULabPanel::Three(){if(Console.IsValid())Console->Command(3);}
void ULabPanel::Four(){if(Console.IsValid())Console->Command(4);}
void ULabPanel::Five(){if(Console.IsValid())Console->Command(5);}
