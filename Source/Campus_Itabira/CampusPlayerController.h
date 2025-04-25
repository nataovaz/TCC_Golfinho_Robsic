#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CampusPlayerController.generated.h"

class UMyUserWidget;
class UInputMappingContext;
class UInputAction;

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
    virtual void SetupInputComponent() override;   // <── declare

    /** Classe do widget (definida via código ou no editor) */
    UPROPERTY(EditAnywhere, Category="HUD")
    TSubclassOf<UMyUserWidget> HUDWidgetClass;

    /** Classe de Input Mapping Context (definida via código ou no editor) */
    UPROPERTY(EditDefaultsOnly, Category="Input")
    UInputMappingContext* IMC_GolfCart;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    UInputAction* IA_MoveForward;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    UInputAction* IA_MoveRight;

    UFUNCTION() void OnIgnoreEscape();


private:
    UPROPERTY() 
    UMyUserWidget* HUDWidget = nullptr;
};
