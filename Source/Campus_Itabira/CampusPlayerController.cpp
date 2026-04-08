#include "CampusPlayerController.h"
#include "MyUserWidget.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/SoftObjectPath.h"

ACampusPlayerController::ACampusPlayerController()
{
    const FSoftClassPath HudWidgetPath(TEXT("/Game/WB_GolfinhoHUD.WB_GolfinhoHUD_C"));
    if (UClass* LoadedHudWidgetClass = HudWidgetPath.TryLoadClass<UMyUserWidget>())
    {
        HUDWidgetClass = LoadedHudWidgetClass;
    }

    const FSoftObjectPath InputMappingContextPath(TEXT("/Game/IMC_Golfinho.IMC_Golfinho"));
    if (UObject* LoadedInputMappingContext = InputMappingContextPath.TryLoad())
    {
        IMC_GolfCart = Cast<UInputMappingContext>(LoadedInputMappingContext);
    }
}

void ACampusPlayerController::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Warning, TEXT("CampusPlayerController::BeginPlay disparou"));

    // Adiciona o mapping context ao subsistema do Enhanced Input
    if (UEnhancedInputLocalPlayerSubsystem* Sub =
         ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer())) {
        if (IMC_GolfCart)
        {
            Sub->AddMappingContext(IMC_GolfCart, 0);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("IMC_GolfCart nao encontrado; o carrinho iniciara sem input mapeado."));
        }
    }

    if (!HUDWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("HUDWidgetClass nao encontrado; o HUD nao sera criado."));
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

