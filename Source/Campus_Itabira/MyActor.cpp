#include "MyActor.h"

#include "Msgs/ROS2Str.h"
#include "Msgs/ROS2PoseStamped.h"
#include "Kismet/GameplayStatics.h"
#include "CampusPlayerController.h"
#include "MyUserWidget.h"
#include "Ros2RuntimeGuard.h"

AMyActor::AMyActor()
{
    PrimaryActorTick.bCanEverTick = true;

    /* ROS2 node */
    NodeComponent = CreateDefaultSubobject<UROS2NodeComponent>(TEXT("ROS2Node"));
    NodeComponent->Name      = TEXT("campus_itabira_actor");
    NodeComponent->Namespace = TEXT("/campus_itabira");

    /* Cesium anchor */
    GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("GlobeAnchor"));
}

void AMyActor::BeginPlay()
{
    Super::BeginPlay();

    DisplayText = TEXT("Aguardando GPS fix...");

    /* ---------- ROS2 ---------- */
    if (CampusRos2RuntimeGuard::CanInitializeRos2())
    {
        NodeComponent->Init();

        // Publisher de pose (mantido para log/debug)
        PosePublisher = NodeComponent->CreatePublisher(
            TEXT("/campus_pose"),
            UROS2Publisher::StaticClass(),
            UROS2PoseStampedMsg::StaticClass());

        // Subscriber do display EKF — recebe Lat/Lon/X/Y/Heading/Vel como String
        FSubscriptionCallback Cb;
        Cb.BindDynamic(this, &AMyActor::OnDisplayReceived);

        DisplaySubscriber = NodeComponent->CreateSubscriber(
            TEXT("/localizacao_display"),
            UROS2StrMsg::StaticClass(),
            Cb);

        UE_LOG(LogTemp, Warning, TEXT("[MyActor] Subscriber /localizacao_display criado."));
    }

    /* ---------- Cesium ---------- */
    Georef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
    if (!Georef)
    {
        UE_LOG(LogTemp, Error, TEXT("[MyActor] ACesiumGeoreference não encontrado!"));
        return;
    }
    GlobeAnchor->SetGeoreference(Georef);
}

void AMyActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Publica pose do ator para debug
    if (PosePublisher)
    {
        FROSPoseStamped Msg;
        const float T = UGameplayStatics::GetTimeSeconds(GetWorld());

        Msg.Header.FrameId       = TEXT("map");
        Msg.Header.Stamp.Sec     = static_cast<int32>(T);
        Msg.Header.Stamp.Nanosec = uint32((T - Msg.Header.Stamp.Sec) * 1e9f);

        const FVector Loc = GetActorLocation();
        const FQuat   Rot = GetActorQuat();

        Msg.Pose.Position.X = Loc.X / 100.f;
        Msg.Pose.Position.Y = Loc.Y / 100.f;
        Msg.Pose.Position.Z = Loc.Z / 100.f;
        Msg.Pose.Orientation = { Rot.X, Rot.Y, Rot.Z, Rot.W };

        PosePublisher->Publish<UROS2PoseStampedMsg, FROSPoseStamped>(Msg);
    }

    // Atualiza widget legado (WB_GolfinhoHUD) se existir
    if (bHasFix && GlobeAnchor && GlobeAnchor->IsRegistered())
    {
        const FVector Lla = GlobeAnchor->GetLongitudeLatitudeHeight();
        if (auto* PC = Cast<ACampusPlayerController>(
                UGameplayStatics::GetPlayerController(this, 0)))
            if (auto* HUD = PC->GetHUDWidget())
                HUD->SetLatLon(Lla.Y, Lla.X);
    }
}

void AMyActor::EndPlay(const EEndPlayReason::Type Reason)
{
    Super::EndPlay(Reason);
}

// ── Callback: recebe "/localizacao_display" ────────────────────────────────
void AMyActor::OnDisplayReceived(const UROS2GenericMsg* InMsg)
{
    const auto* Msg = Cast<UROS2StrMsg>(InMsg);
    if (!Msg) return;

    FROSStr Data;
    Msg->GetMsg(Data);

    // Salva texto completo para o HUD
    DisplayText = Data.Data;

    // Extrai Lat e Lon para mover o ator no Cesium
    TArray<FString> Lines;
    Data.Data.ParseIntoArray(Lines, TEXT("\n"), true);

    double NewLat = LastLat;
    double NewLon = LastLon;

    for (const FString& Line : Lines)
    {
        FString Key, Val;
        if (!Line.Split(TEXT(": "), &Key, &Val)) continue;

        Key.TrimStartAndEndInline();
        Val.TrimStartAndEndInline();

        if (Key.Equals(TEXT("Lat")))
            NewLat = FCString::Atod(*Val);
        else if (Key.Equals(TEXT("Lon")))
            NewLon = FCString::Atod(*Val);
    }

    // Só move se tiver coordenadas válidas
    if (NewLat != 0.0 && NewLon != 0.0)
    {
        LastLat = NewLat;
        LastLon = NewLon;
        bHasFix = true;

        // Altitude atual ou padrão (700m = altitude média de Itabira)
        double CurrentAlt = 700.0;
        if (GlobeAnchor && GlobeAnchor->IsRegistered())
        {
            const FVector Lla = GlobeAnchor->GetLongitudeLatitudeHeight();
            if (Lla.Z > 0.0) CurrentAlt = Lla.Z;
        }

        // Move o ator para a posição GPS estimada pelo EKF
        if (GlobeAnchor)
            GlobeAnchor->MoveToLongitudeLatitudeHeight(NewLon, NewLat, CurrentAlt);

        UE_LOG(LogTemp, Display, TEXT("[MyActor] Posição: Lat=%.6f Lon=%.6f"), NewLat, NewLon);
    }
}
