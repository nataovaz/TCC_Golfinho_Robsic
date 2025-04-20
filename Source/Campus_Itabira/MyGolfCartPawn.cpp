#include "MyGolfCartPawn.h"

#include "Msgs/ROS2Str.h"
#include "Msgs/ROS2PoseStamped.h"
#include "Kismet/GameplayStatics.h"
#include "CampusPlayerController.h"
#include "MyUserWidget.h"

AMyGolfCartPawn::AMyGolfCartPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Player0;          // controla já no Play

    /* ROOT (invisível) */
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    /* Câmera em terceira pessoa tipo GPS */
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 350.f;       // 3,5 m atrás
    SpringArm->SocketOffset    = FVector(0.f, 0.f, 100.f);   // 1 m acima
    SpringArm->bInheritPitch   = false;
    SpringArm->bInheritYaw     = false;
    SpringArm->bInheritRoll    = false;
    SpringArm->SetRelativeRotation(FRotator(-20.f, 0.f, 0.f));

    Cam = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Cam->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

    /* ROS2 */
    NodeComponent = CreateDefaultSubobject<UROS2NodeComponent>(TEXT("ROS2Node"));

    /* Cesium */
    GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("GlobeAnchor"));
}

void AMyGolfCartPawn::BeginPlay()
{
    Super::BeginPlay();

    /* ROS2 init */
    NodeComponent->Init();
    PosePublisher = NodeComponent->CreatePublisher(
        TEXT("/campus_pose"), UROS2Publisher::StaticClass(),
        UROS2PoseStampedMsg::StaticClass());

    FSubscriptionCallback Cb; Cb.BindDynamic(this, &AMyGolfCartPawn::OnMessageReceived);
    Subscriber = NodeComponent->CreateSubscriber(
        TEXT("/teste_unreal"), UROS2StrMsg::StaticClass(), Cb);

    /* Cesium ref */
    Georef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
    if (Georef) GlobeAnchor->SetGeoreference(Georef);
}

void AMyGolfCartPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!PosePublisher) return;

    /* publica pose no ROS2 (igual ao código anterior) */
    FROSPoseStamped Msg;
    const float T = UGameplayStatics::GetTimeSeconds(GetWorld());
    Msg.Header.FrameId = TEXT("map");
    Msg.Header.Stamp.Sec = int32(T);
    Msg.Header.Stamp.Nanosec = uint32((T - Msg.Header.Stamp.Sec) * 1e9f);

    const FVector Loc = GetActorLocation();
    const FQuat   Rot = GetActorQuat();

    Msg.Pose.Position = { Loc.X/100.f, Loc.Y/100.f, Loc.Z/100.f };
    Msg.Pose.Orientation = { Rot.X, Rot.Y, Rot.Z, Rot.W };
    PosePublisher->Publish<UROS2PoseStampedMsg, FROSPoseStamped>(Msg);

    /* LLA precisa */
    if (GlobeAnchor && GlobeAnchor->IsRegistered())
    {
        const FVector Lla = GlobeAnchor->GetLongitudeLatitudeHeight();
        if (auto* PC = Cast<ACampusPlayerController>(
                UGameplayStatics::GetPlayerController(this, 0)))
            if (auto* HUD = PC->GetHUDWidget())
                HUD->SetLatLon(Lla.Y, Lla.X);
    }
}

void AMyGolfCartPawn::EndPlay(const EEndPlayReason::Type Reason)
{
    Super::EndPlay(Reason);
}

void AMyGolfCartPawn::OnMessageReceived(const UROS2GenericMsg* InMsg)
{
    if (const auto* Msg = Cast<UROS2StrMsg>(InMsg))
    {
        FROSStr Data; Msg->GetMsg(Data);
        UE_LOG(LogTemp, Display, TEXT("Mensagem recebida: %s"), *Data.Data);
    }
}
