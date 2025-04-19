#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CampusPlayerController.generated.h"

class UMyUserWidget;

/** PlayerController que instancia e mantém o HUD de LLA */
UCLASS()
class CAMPUS_ITABIRA_API ACampusPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ACampusPlayerController();

    /** Getter para que outros atores possam atualizar o HUD */
    UMyUserWidget* GetHUDWidget() const { return HUDWidget; }

protected:
    virtual void BeginPlay() override;

    /** Classe do widget (definida via código ou no editor) */
    UPROPERTY(EditAnywhere, Category="HUD")
    TSubclassOf<UMyUserWidget> HUDWidgetClass;

private:
    UPROPERTY() UMyUserWidget* HUDWidget = nullptr;
};
