// MyGolfCartPawn.cpp ---------------------------------------------------------
#include "MyGolfCartPawn.h"
#include "Msgs/ROS2Str.h"
#include "Msgs/ROS2PoseStamped.h"
#include "CampusPlayerController.h"
#include "MyUserWidget.h"

#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"


AMyGolfCartPawn::AMyGolfCartPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Player0;

    auto* Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
    Capsule->InitCapsuleSize(42.f, 96.f);
    RootComponent = Capsule;

    /* ---------- Câmera ---------- */
    CamRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CamRoot"));
    CamRoot->SetupAttachment(RootComponent);
    CamRoot->SetRelativeLocation({0.f, 0.f, 50.f});          // ponto 30 cm acima

    TopDownCam = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCam"));
    TopDownCam->SetupAttachment(CamRoot);
    TopDownCam->SetRelativeLocation({0.f, 0.f, 500.f});      // 5 m acima
    TopDownCam->SetRelativeRotation({-90.f, 0.f, 0.f});      // olhando p/ baixo
    TopDownCam->bUsePawnControlRotation = false;

    /* ---------- Movimento ---------- */
    MoveComp = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MoveComp"));
    MoveComp->UpdatedComponent = RootComponent;

    /* ---------- ROS2 & Cesium ---------- */
    NodeComponent = CreateDefaultSubobject<UROS2NodeComponent>(TEXT("ROS2Node"));
    GlobeAnchor   = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("GlobeAnchor"));
}


void AMyGolfCartPawn::BeginPlay()
{
    Super::BeginPlay();

    /* ROS2  */
    NodeComponent->Init();
    PosePublisher = NodeComponent->CreatePublisher(
        TEXT("/campus_pose"), UROS2Publisher::StaticClass(),
        UROS2PoseStampedMsg::StaticClass());

    FSubscriptionCallback Cb; 
    Cb.BindDynamic(this, &AMyGolfCartPawn::OnMessageReceived);
    Subscriber = NodeComponent->CreateSubscriber(
        TEXT("/teste_unreal"), UROS2StrMsg::StaticClass(), Cb);

    /* Cesium  */
    if (ACesiumGeoreference* G = ACesiumGeoreference::GetDefaultGeoreference(GetWorld()))
        GlobeAnchor->SetGeoreference(G);

    /* Garante que esta câmera seja a ativa  */
    TopDownCam->Activate();
    FTimerHandle Tmp;
    GetWorldTimerManager().SetTimer(
        Tmp,
        [this]()
        {
            if (auto* PC = Cast<APlayerController>(GetController()))
                PC->SetViewTarget(this, FViewTargetTransitionParams());
        },
        0.01f, false);
}

// TICK: publica pose ROS2 + atualiza HUD 
void AMyGolfCartPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!PosePublisher) return;

    FROSPoseStamped Msg;
    const float T = UGameplayStatics::GetTimeSeconds(GetWorld());
    Msg.Header.FrameId   = TEXT("map");
    Msg.Header.Stamp.Sec = int32(T);
    Msg.Header.Stamp.Nanosec = uint32((T - Msg.Header.Stamp.Sec) * 1e9f);

    const FVector Loc = GetActorLocation();
    const FQuat   Rot = GetActorQuat();
    Msg.Pose.Position    = {Loc.X/100.f, Loc.Y/100.f, Loc.Z/100.f}; // cm → m
    Msg.Pose.Orientation = {Rot.X, Rot.Y, Rot.Z, Rot.W};
    PosePublisher->Publish<UROS2PoseStampedMsg, FROSPoseStamped>(Msg);

    if (GlobeAnchor && GlobeAnchor->IsRegistered())
        if (auto* PC  = Cast<ACampusPlayerController>(UGameplayStatics::GetPlayerController(this,0)))
            if (auto* HUD = PC->GetHUDWidget())
            {
                const FVector Lla = GlobeAnchor->GetLongitudeLatitudeHeight();
                HUD->SetLatLon(Lla.Y, Lla.X);
            }
}

// ─────────── INPUT ---------------------------------------------------------
void AMyGolfCartPawn::SetupPlayerInputComponent(UInputComponent* IC)
{
    if (auto* EIC = Cast<UEnhancedInputComponent>(IC))
    {
        EIC->BindAction(IA_MoveForward, ETriggerEvent::Triggered,
                        this, &AMyGolfCartPawn::OnMoveForward);
        EIC->BindAction(IA_MoveRight,   ETriggerEvent::Triggered,
                        this, &AMyGolfCartPawn::OnMoveRight);
    }
}

void AMyGolfCartPawn::OnMoveForward(const FInputActionValue& V)
{
    if (const float Val = V.Get<float>(); Val != 0.f)
        AddMovementInput(GetActorForwardVector(), Val);
}

void AMyGolfCartPawn::OnMoveRight(const FInputActionValue& V)
{
    if (const float Val = V.Get<float>(); Val != 0.f)
        AddMovementInput(GetActorRightVector(), Val);
}

// ─────────── ROS2 callback --------------------------------------------------
void AMyGolfCartPawn::OnMessageReceived(const UROS2GenericMsg* In)
{
    if (auto* M = Cast<UROS2StrMsg>(In))
    {
        FROSStr D; M->GetMsg(D);
        UE_LOG(LogTemp, Display, TEXT("ROS2 msg: %s"), *D.Data);
    }
}

// ─────────── ENDPLAY --------------------------------------------------------
void AMyGolfCartPawn::EndPlay(const EEndPlayReason::Type Reason)
{
    Super::EndPlay(Reason);
    /* Se necessário, finalize ROS 2, timers etc. */
}
