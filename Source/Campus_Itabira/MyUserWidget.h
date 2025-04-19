#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"          // <‑‑ para UTextBlock
#include "MyUserWidget.generated.h"

/** Widget que exibe Latitude / Longitude */
UCLASS()
class CAMPUS_ITABIRA_API UMyUserWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Atualiza os textos (chamado pelo ator / controlador) */
    UFUNCTION(BlueprintCallable)
    void SetLatLon(double Lat, double Lon);

protected:
    virtual void NativeConstruct() override;

    /** Ligados automaticamente aos TextBlocks do Blueprint */
    UPROPERTY(meta = (BindWidget)) UTextBlock* TxtLatitude;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TxtLongitude;
};
