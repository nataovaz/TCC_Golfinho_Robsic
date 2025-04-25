// NÃO coloque #pragma once aqui!
#include "CampusGameMode.h"
#include "MyGolfCartPawn.h"
#include "CampusPlayerController.h"
// #include "Engine/PlayerStart.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

ACampusGameMode::ACampusGameMode()
{
    DefaultPawnClass      = AMyGolfCartPawn::StaticClass();
    PlayerControllerClass = ACampusPlayerController::StaticClass();
}

AActor* ACampusGameMode::ChoosePlayerStart_Implementation(AController* PC)
{
    if (AActor* PS = Super::ChoosePlayerStart_Implementation(PC))
        return PS;

    const FVector Loc(0.f, 0.f, 50.f);
    const FTransform Tf(FRotator::ZeroRotator, Loc);
    return GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Tf);
}

void ACampusGameMode::StartPlay()
{
    UE_LOG(LogTemp, Warning, TEXT("[CampusGameMode] StartPlay"));
    Super::StartPlay();
}
