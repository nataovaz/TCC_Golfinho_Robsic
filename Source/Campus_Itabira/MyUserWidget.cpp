#include "MyUserWidget.h"

void UMyUserWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetLatLon(0.0, 0.0);          // valor inicial
}

void UMyUserWidget::SetLatLon(double Lat, double Lon)
{
    if (TxtLatitude)
        TxtLatitude ->SetText(FText::FromString(
            FString::Printf(TEXT("Lat: %.6f"), Lat)));

    if (TxtLongitude)
        TxtLongitude->SetText(FText::FromString(
            FString::Printf(TEXT("Lon: %.6f"), Lon)));
}
