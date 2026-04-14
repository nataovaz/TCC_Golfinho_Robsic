#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LocalizacaoHUD.generated.h"

/**
 * HUD puro em C++ — exibe dados do EKF (Lat, Lon, X, Y, Heading, Vel)
 * sem necessidade de Widget Blueprint.
 * Atualizado via AMyActor::DisplayText.
 */
UCLASS()
class CAMPUS_ITABIRA_API ALocalizacaoHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
};
