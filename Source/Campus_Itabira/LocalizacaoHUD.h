#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LocalizacaoHUD.generated.h"

/**
 * HUD em C++ — exibe painel de dados EKF no canto superior esquerdo:
 *   Lat, Lon, X, Y, Heading, Vel
 * (Mini-mapa Google Maps removido — causava lag por HTTP a cada 5s.)
 */
UCLASS()
class CAMPUS_ITABIRA_API ALocalizacaoHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
};
