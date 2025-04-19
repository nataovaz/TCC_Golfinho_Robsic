#include "CampusPlayerController.h"
#include "MyUserWidget.h"
#include "UObject/ConstructorHelpers.h"

ACampusPlayerController::ACampusPlayerController()
{
    // Atribui automaticamente o Blueprint WB_GolfinhoHUD
    static ConstructorHelpers::FClassFinder<UMyUserWidget> HudBP(
        TEXT("/Game/WB_GolfinhoHUD"));            // ajuste o path se diferente
    if (HudBP.Succeeded())
    {
        HUDWidgetClass = HudBP.Class;
    }
}

void ACampusPlayerController::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Warning, TEXT("CampusPlayerController::BeginPlay disparou"));

    if (!HUDWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("HUDWidgetClass está NULO."));
        return;
    }

    HUDWidget = CreateWidget<UMyUserWidget>(this, HUDWidgetClass);
    if (HUDWidget)
    {
        HUDWidget->AddToViewport();
        HUDWidget->SetAlignmentInViewport({0.5f, 0.f});         // centro‑x, topo‑y
        HUDWidget->SetPositionInViewport({0.f, 20.f}, false);   // 20 px abaixo do topo
        UE_LOG(LogTemp, Warning, TEXT("Widget criado e adicionado ao viewport."));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CreateWidget falhou."));
    }
}
