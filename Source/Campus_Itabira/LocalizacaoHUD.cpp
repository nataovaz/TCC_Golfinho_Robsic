#include "LocalizacaoHUD.h"
#include "MyActor.h"
#include "Engine/Canvas.h"
#include "Kismet/GameplayStatics.h"

void ALocalizacaoHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!Canvas) return;

    // Busca o MyActor na cena para ler o DisplayText
    AMyActor* Actor = Cast<AMyActor>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AMyActor::StaticClass()));

    const FString Text = Actor ? Actor->DisplayText
                                : TEXT("Aguardando GPS fix...");

    // Divide o texto em linhas
    TArray<FString> Lines;
    Text.ParseIntoArray(Lines, TEXT("\n"), true);

    // Dimensões do painel
    const float PadX     = 16.f;
    const float PadY     = 12.f;
    const float LineH    = 22.f;
    const float BoxW     = 290.f;
    const float BoxH     = PadY * 2.f + Lines.Num() * LineH;
    const float BoxX     = 20.f;
    const float BoxY     = 20.f;

    // Fundo semi-transparente
    DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), BoxX, BoxY, BoxW, BoxH);

    // Borda sutil
    DrawRect(FLinearColor(0.2f, 0.6f, 1.f, 0.8f), BoxX,          BoxY,          BoxW, 2.f);
    DrawRect(FLinearColor(0.2f, 0.6f, 1.f, 0.8f), BoxX,          BoxY + BoxH,   BoxW, 2.f);
    DrawRect(FLinearColor(0.2f, 0.6f, 1.f, 0.8f), BoxX,          BoxY,          2.f,  BoxH);
    DrawRect(FLinearColor(0.2f, 0.6f, 1.f, 0.8f), BoxX + BoxW,   BoxY,          2.f,  BoxH + 2.f);

    // Título
    DrawText(TEXT("[ Gêmeo Digital — EKF ]"),
             FLinearColor(0.3f, 0.8f, 1.f, 1.f),
             BoxX + PadX, BoxY + PadY,
             nullptr, 1.0f);

    // Linhas de dados
    for (int32 i = 0; i < Lines.Num(); ++i)
    {
        DrawText(Lines[i],
                 FLinearColor::White,
                 BoxX + PadX,
                 BoxY + PadY + LineH * (i + 1),
                 nullptr, 1.0f);
    }
}
