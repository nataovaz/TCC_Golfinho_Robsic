#include "CampusPlayerController.h"
#include "MyUserWidget.h"
#include "UObject/ConstructorHelpers.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"

ACampusPlayerController::ACampusPlayerController()
{
    // Atribui automaticamente o Blueprint WB_GolfinhoHUD
    static ConstructorHelpers::FClassFinder<UMyUserWidget> HudBP(
        TEXT("/Game/WB_GolfinhoHUD"));            // ajuste o path se diferente
    if (HudBP.Succeeded())
    {
        HUDWidgetClass = HudBP.Class;
    }

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> 
        IMCObj(TEXT("/Game/IMC_Golfinho.IMC_Golfinho"));
    if (IMCObj.Succeeded()) {
        IMC_GolfCart = IMCObj.Object;
    }
}

void ACampusPlayerController::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Warning, TEXT("CampusPlayerController::BeginPlay disparou"));

    // Adiciona o mapping context ao subsistema do Enhanced Input
    if (UEnhancedInputLocalPlayerSubsystem* Sub = 
         ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer())) {
       Sub->AddMappingContext(IMC_GolfCart, 0);
    }

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

void ACampusPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Remove o binding padrão de Esc (Stop Playing in PIE)
#if WITH_EDITOR
    InputComponent->BindKey(EKeys::Escape, IE_Pressed,
        this, &ACampusPlayerController::OnIgnoreEscape);
#endif
}

void ACampusPlayerController::OnIgnoreEscape()  { /* faz nada */ }

