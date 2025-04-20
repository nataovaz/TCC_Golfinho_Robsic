#include "MyActor.h"

#include "Msgs/ROS2Str.h"
#include "Msgs/ROS2PoseStamped.h"
#include "Kismet/GameplayStatics.h"
#include "CampusPlayerController.h"
#include "MyUserWidget.h"

AMyActor::AMyActor()
{
    PrimaryActorTick.bCanEverTick = true;

    /* ROS2 node */
    NodeComponent = CreateDefaultSubobject<UROS2NodeComponent>(TEXT("ROS2Node"));

    /* Cesium anchor (ActorComponent, não precisa attachment) */
    GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("GlobeAnchor"));
}

void AMyActor::BeginPlay()
{
    Super::BeginPlay();

    /* ---------- ROS2 ---------- */
    NodeComponent->Init();

    PosePublisher = NodeComponent->CreatePublisher(
        TEXT("/campus_pose"),
        UROS2Publisher::StaticClass(),
        UROS2PoseStampedMsg::StaticClass());

    FSubscriptionCallback Cb;
    Cb.BindDynamic(this, &AMyActor::OnMessageReceived);

    Subscriber = NodeComponent->CreateSubscriber(
        TEXT("/teste_unreal"),
        UROS2StrMsg::StaticClass(),
        Cb);

    /* ---------- Cesium ---------- */
    Georef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
    if (!Georef)
    {
        UE_LOG(LogTemp, Error, TEXT("ACesiumGeoreference não encontrado!"));
        return;
    }

    GlobeAnchor->SetGeoreference(Georef);   // associa explicitamente
}

void AMyActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!PosePublisher) return;

    /* -------- Publicar pose -------- */
    FROSPoseStamped Msg;
    const float T = UGameplayStatics::GetTimeSeconds(GetWorld());

    Msg.Header.FrameId      = TEXT("map");
    Msg.Header.Stamp.Sec    = static_cast<int32>(T);
    Msg.Header.Stamp.Nanosec= uint32((T - Msg.Header.Stamp.Sec) * 1e9f);

    const FVector Loc = GetActorLocation();
    const FQuat   Rot = GetActorQuat();

    Msg.Pose.Position.X = Loc.X / 100.f;
    Msg.Pose.Position.Y = Loc.Y / 100.f;
    Msg.Pose.Position.Z = Loc.Z / 100.f;

    Msg.Pose.Orientation = { Rot.X, Rot.Y, Rot.Z, Rot.W };

    PosePublisher->Publish<UROS2PoseStampedMsg, FROSPoseStamped>(Msg);

    /* -------- Coordenadas LLA -------- */
    if (GlobeAnchor && GlobeAnchor->IsRegistered())
    {
        const FVector Lla = GlobeAnchor->GetLongitudeLatitudeHeight(); // Lon, Lat, Alt

        UE_LOG(LogTemp, Display, TEXT("LLA: %.8f, %.8f, %.2f m"),
               Lla.X, Lla.Y, Lla.Z);

        if (auto* PC = Cast<ACampusPlayerController>(
                UGameplayStatics::GetPlayerController(this, 0)))
            if (auto* HUD = PC->GetHUDWidget())
                HUD->SetLatLon(Lla.Y, Lla.X);   // HUD = (Lat, Lon)
    }
}

void AMyActor::EndPlay(const EEndPlayReason::Type Reason)
{
    Super::EndPlay(Reason);
}

void AMyActor::OnMessageReceived(const UROS2GenericMsg* InMsg)
{
    if (const auto* Msg = Cast<UROS2StrMsg>(InMsg))
    {
        FROSStr Data; Msg->GetMsg(Data);
        UE_LOG(LogTemp, Display, TEXT("Mensagem recebida: %s"), *Data.Data);
    }
}
