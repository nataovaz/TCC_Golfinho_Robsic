#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CampusGameMode.generated.h"

UCLASS()
class CAMPUS_ITABIRA_API ACampusGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACampusGameMode();

protected:
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    virtual void StartPlay() override;
};
