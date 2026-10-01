#include "PanelController.h"
#include "LabScene.h"
#include "Components/InputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "Framework/Application/SlateApplication.h"
void APanelController::SetupInputComponent() {
    Super::SetupInputComponent();
}
bool APanelController::PanelPoint(FVector2D Position,FVector& Point) const {
    FVector Origin,Direction;
    if(!DeprojectScreenPositionToWorld(Position.X,Position.Y,Origin,Direction) || FMath::Abs(Direction.X)<0.001) return false;
    auto* Scene=Cast<ALabScene>(UGameplayStatics::GetActorOfClass(this,ALabScene::StaticClass()));
    const double T=((Scene?Scene->InteractionX():45)-Origin.X)/Direction.X;
    if(T<0)return false; Point=Origin+T*Direction; return true;
}
void APanelController::Press(FVector2D Position) {
    FHitResult Hit; FVector Point;
    UE_LOG(LogTemp,Display,TEXT("Panel mouse press: %.1f %.1f"),Position.X,Position.Y);
    if(GetHitResultAtScreenPosition(Position,ECC_Visibility,false,Hit) && PanelPoint(Position,Point)) {
        auto* Scene=Cast<ALabScene>(Hit.GetActor());
        if(Scene && Hit.GetComponent() && Hit.GetComponent()->ComponentTags.Num()) {
            DragScene=Scene; Scene->PressControl(Hit.GetComponent()->ComponentTags[0].ToString(),Point);
        }
    }
}
void APanelController::Release() { if(DragScene.IsValid())DragScene->ReleaseControl(); DragScene.Reset(); }
void APanelController::PlayerTick(float DeltaSeconds) {
    Super::PlayerTick(DeltaSeconds);
}
void APanelController::Drag(FVector2D Position) {
    if(!DragScene.IsValid())return;
    FVector Point;if(PanelPoint(Position,Point))DragScene->DragControl(Point);
}

#include "Framework/Application/IInputProcessor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SWindow.h"
class FPanelInputProbe : public IInputProcessor {
public:
    virtual void Tick(const float,FSlateApplication&,TSharedRef<ICursor>) override {}
    virtual bool HandleMouseButtonDownEvent(FSlateApplication& App,const FPointerEvent& E) override {
        const auto P=E.GetScreenSpacePosition();
        const auto Path=App.LocateWindowUnderMouse(P,App.GetInteractiveTopLevelWindows());
        FString Names;for(int I=0;I<Path.Widgets.Num();I++)Names+=Path.Widgets[I].Widget->GetTypeAsString()+TEXT(" > ");
        UE_LOG(LogTemp,Warning,TEXT("INPUT PROBE: %.1f %.1f active=%d path=%s"),P.X,P.Y,App.IsActive(),*Names);
        return false;
    }
};
void APanelController::BeginPlay(){Super::BeginPlay();if(!FParse::Param(FCommandLine::Get(),TEXT("LabInputDebug")))return;InputProbe=MakeShared<FPanelInputProbe>();FSlateApplication::Get().RegisterInputPreProcessor(InputProbe,0);}
void APanelController::EndPlay(const EEndPlayReason::Type Reason){FSlateApplication::Get().UnregisterInputPreProcessor(InputProbe);InputProbe.Reset();Super::EndPlay(Reason);}
