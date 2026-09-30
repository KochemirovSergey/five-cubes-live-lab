#include "LabWorldLabel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
TSharedRef<SWidget> ULabWorldLabel::RebuildWidget(){if(Text)return Super::RebuildWidget();Text=WidgetTree->ConstructWidget<UTextBlock>();WidgetTree->RootWidget=Text;auto Font=Text->GetFont();Font.Size=30;Text->SetFont(Font);Text->SetJustification(ETextJustify::Center);Text->SetAutoWrapText(true);return Super::RebuildWidget();}
void ULabWorldLabel::Caption(const FString& V,bool H){if(!Text)TakeWidget();if(Text){Text->SetText(FText::FromString(V));Text->SetColorAndOpacity(FSlateColor(H?FLinearColor::Yellow:FLinearColor::White));}}
