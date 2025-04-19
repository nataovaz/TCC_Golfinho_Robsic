// =============================
// MyActor.cpp
// =============================
#include "MyActor.h"
#include "Msgs/ROS2Str.h"
#include "Msgs/ROS2PoseStamped.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "CampusPlayerController.h"
#include "MyUserWidget.h"

AMyActor::AMyActor()
{
    PrimaryActorTick.bCanEverTick = true;
    NodeComponent = CreateDefaultSubobject<UROS2NodeComponent>(TEXT("ROS2Node"));
}

void AMyActor::BeginPlay()
{
    Super::BeginPlay();

    NodeComponent->Init();
    PosePublisher = NodeComponent->CreatePublisher(
        TEXT("/campus_pose"),
        UROS2Publisher::StaticClass(),
        UROS2PoseStampedMsg::StaticClass()
    );

    FSubscriptionCallback Cb;
    Cb.BindDynamic(this, &AMyActor::OnMessageReceived);
    Subscriber = NodeComponent->CreateSubscriber(
        TEXT("/teste_unreal"),
        UROS2StrMsg::StaticClass(),
        Cb
    );

    Georef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
    if (!Georef)
    {
        UE_LOG(LogTemp, Error, TEXT("ACesiumGeoreference não encontrado!"));
    }
}

void AMyActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Aqui você fecha ROS2, timers, ou o que mais precisar
    Super::EndPlay(EndPlayReason);
}

void AMyActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!PosePublisher || !Georef) return;

    // Publica pose no ROS
    FROSPoseStamped Pose;
    Pose.Header.FrameId = TEXT("map");
    float T = UGameplayStatics::GetTimeSeconds(GetWorld());
    Pose.Header.Stamp.Sec     = static_cast<int32>(T);
    Pose.Header.Stamp.Nanosec = uint32((T - Pose.Header.Stamp.Sec) * 1e9f);

    FVector Loc = GetActorLocation();
    FQuat   Q   = GetActorQuat();
    Pose.Pose.Position.X = Loc.X / 100.f;
    Pose.Pose.Position.Y = Loc.Y / 100.f;
    Pose.Pose.Position.Z = Loc.Z / 100.f;
    Pose.Pose.Orientation.X = Q.X;
    Pose.Pose.Orientation.Y = Q.Y;
    Pose.Pose.Orientation.Z = Q.Z;
    Pose.Pose.Orientation.W = Q.W;
    PosePublisher->Publish<UROS2PoseStampedMsg, FROSPoseStamped>(Pose);

    // Converte para LLA
    FVector LonLatAlt = Georef->TransformUnrealPositionToLongitudeLatitudeHeight(Loc);
    double Lon = LonLatAlt.X;
    double Lat = LonLatAlt.Y;
    double Alt = LonLatAlt.Z;
    UE_LOG(LogTemp, Display, TEXT("LLA: %.6f, %.6f, %.2f m"), Lon, Lat, Alt);

    // Atualiza o HUD
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (ACampusPlayerController* CPC = Cast<ACampusPlayerController>(PC))
        {
            if (UMyUserWidget* Hud = CPC->GetHUDWidget())
            {
                Hud->SetLatLon(Lat, Lon);
            }
        }
    }
}

void AMyActor::OnMessageReceived(const UROS2GenericMsg* InMsg)
{
    if (const UROS2StrMsg* Msg = Cast<UROS2StrMsg>(InMsg))
    {
        FROSStr Data;
        Msg->GetMsg(Data);
        UE_LOG(LogTemp, Display, TEXT("Mensagem recebida: %s"), *Data.Data);
    }
}
