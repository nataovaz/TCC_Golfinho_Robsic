// MyGolfCartPawn.cpp ---------------------------------------------------------
#include "MyGolfCartPawn.h"
#include "Msgs/ROS2Str.h"
#include "Msgs/ROS2PoseStamped.h"
#include "CampusPlayerController.h"
#include "MyUserWidget.h"
#include "Ros2RuntimeGuard.h"

#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
const FName WheelFrontLeft(TEXT("Wheel_Front_Left"));
const FName WheelFrontRight(TEXT("Wheel_Front_Right"));
const FName WheelRearLeft(TEXT("Wheel_Rear_Left"));
const FName WheelRearRight(TEXT("Wheel_Rear_Right"));
}


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
    TopDownCam->SetRelativeLocation({-220.f, 170.f, 430.f}); // deslocada para a direita para destacar a roda lateral
    TopDownCam->SetRelativeRotation({-57.f, -32.f, 0.f});    // inclina e vira levemente para enquadrar o carrinho
    TopDownCam->bUsePawnControlRotation = false;

    /* ---------- Movimento ---------- */
    MoveComp = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MoveComp"));
    MoveComp->UpdatedComponent = RootComponent;

    /* ---------- ROS2 & Cesium ---------- */
    NodeComponent = CreateDefaultSubobject<UROS2NodeComponent>(TEXT("ROS2Node"));
    NodeComponent->Name = TEXT("campus_itabira_golfcart");
    NodeComponent->Namespace = TEXT("/campus_itabira");
    GlobeAnchor   = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("GlobeAnchor"));
}


void AMyGolfCartPawn::BeginPlay()
{
    Super::BeginPlay();

    LastActorLocation = GetActorLocation();
    InitializeVisualCartMesh();

    /* ROS2  */
    if (CampusRos2RuntimeGuard::CanInitializeRos2())
    {
        NodeComponent->Init();
        PosePublisher = NodeComponent->CreatePublisher(
            TEXT("/campus_pose"), UROS2Publisher::StaticClass(),
            UROS2PoseStampedMsg::StaticClass());

        FSubscriptionCallback Cb;
        Cb.BindDynamic(this, &AMyGolfCartPawn::OnMessageReceived);
        Subscriber = NodeComponent->CreateSubscriber(
            TEXT("/teste_unreal"), UROS2StrMsg::StaticClass(), Cb);

        UE_LOG(
            LogTemp,
            Log,
            TEXT("Golf cart ROS setup | node_state=%d publisher=%s subscriber=%s"),
            static_cast<int32>(NodeComponent ? NodeComponent->State.GetValue() : UROS2State::Created),
            PosePublisher ? TEXT("ok") : TEXT("null"),
            Subscriber ? TEXT("ok") : TEXT("null")
        );
    }

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

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        float ForwardInput = 0.f;
        float RightInput = 0.f;

        if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))
        {
            ForwardInput += 1.f;
        }
        if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))
        {
            ForwardInput -= 1.f;
        }
        if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right))
        {
            RightInput += 1.f;
        }
        if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))
        {
            RightInput -= 1.f;
        }

        if (!FMath::IsNearlyZero(ForwardInput))
        {
            AddMovementInput(GetActorForwardVector(), ForwardInput);
        }
        if (!FMath::IsNearlyZero(RightInput))
        {
            AddMovementInput(GetActorRightVector(), RightInput);
        }
    }

    UpdateVisualWheelSpin(DeltaTime);
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

void AMyGolfCartPawn::InitializeVisualCartMesh()
{
    if (VisualCartMesh)
    {
        return;
    }

    TArray<USkeletalMeshComponent*> SkeletalMeshes;
    GetComponents(SkeletalMeshes);

    USkeletalMeshComponent* SourceMesh = nullptr;
    for (USkeletalMeshComponent* Candidate : SkeletalMeshes)
    {
        if (!Candidate || !Candidate->GetSkeletalMeshAsset())
        {
            continue;
        }

        SourceMesh = Candidate;
        if (Candidate->GetName().Contains(TEXT("Golfinho")))
        {
            break;
        }
    }

    if (!SourceMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("No skeletal mesh found on %s to drive golf cart wheel visuals."), *GetName());
        return;
    }

    SourceCartMesh = SourceMesh;

    VisualCartMesh = NewObject<UPoseableMeshComponent>(this, TEXT("VisualCartMesh"));
    if (!VisualCartMesh)
    {
        return;
    }

    USceneComponent* AttachParent = SourceMesh->GetAttachParent();
    const FName AttachSocket = SourceMesh->GetAttachSocketName();
    if (!AttachParent)
    {
        AttachParent = RootComponent.Get();
    }
    VisualCartMesh->SetupAttachment(AttachParent, AttachSocket);
    VisualCartMesh->RegisterComponent();
    VisualCartMesh->SetRelativeTransform(SourceMesh->GetRelativeTransform());
    VisualCartMesh->SetSkinnedAssetAndUpdate(SourceMesh->GetSkeletalMeshAsset(), false);
    VisualCartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VisualCartMesh->SetCastShadow(SourceMesh->CastShadow);
    VisualCartMesh->CopyPoseFromSkeletalComponent(SourceMesh);

    BaseWheelBoneTransforms.Add(WheelFrontLeft, VisualCartMesh->GetBoneTransformByName(WheelFrontLeft, EBoneSpaces::ComponentSpace));
    BaseWheelBoneTransforms.Add(WheelFrontRight, VisualCartMesh->GetBoneTransformByName(WheelFrontRight, EBoneSpaces::ComponentSpace));
    BaseWheelBoneTransforms.Add(WheelRearLeft, VisualCartMesh->GetBoneTransformByName(WheelRearLeft, EBoneSpaces::ComponentSpace));
    BaseWheelBoneTransforms.Add(WheelRearRight, VisualCartMesh->GetBoneTransformByName(WheelRearRight, EBoneSpaces::ComponentSpace));

    const int32 NumMaterials = SourceMesh->GetNumMaterials();
    for (int32 Index = 0; Index < NumMaterials; ++Index)
    {
        VisualCartMesh->SetMaterial(Index, SourceMesh->GetMaterial(Index));
    }

    SourceMesh->SetHiddenInGame(true, true);
    SourceMesh->SetVisibility(false, true);
}

void AMyGolfCartPawn::UpdateVisualWheelSpin(float DeltaTime)
{
    if (!VisualCartMesh || DeltaTime <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    if (SourceCartMesh)
    {
        VisualCartMesh->CopyPoseFromSkeletalComponent(SourceCartMesh);
    }

    const FVector CurrentLocation = GetActorLocation();
    const float SpeedCmPerSecond = FVector::Dist(CurrentLocation, LastActorLocation) / DeltaTime;
    LastActorLocation = CurrentLocation;

    // Keeps wheel motion readable even when movement comes from teleports or GPS updates.
    const float ClampedSpeed = FMath::Min(SpeedCmPerSecond, 5000.f);
    WheelSpinDegrees = FMath::Fmod(WheelSpinDegrees + (ClampedSpeed * DeltaTime * 0.4f), 360.f);

    auto ApplyWheelRotation = [this](const FName BoneName)
    {
        const FTransform* BaseTransform = BaseWheelBoneTransforms.Find(BoneName);
        if (!BaseTransform)
        {
            return;
        }

        FTransform WheelTransform = *BaseTransform;
        const FVector WheelAxis = BaseTransform->GetRotation().RotateVector(FVector::RightVector).GetSafeNormal();
        const FQuat SpinQuat(WheelAxis, FMath::DegreesToRadians(WheelSpinDegrees));
        WheelTransform.SetRotation((SpinQuat * BaseTransform->GetRotation()).GetNormalized());
        VisualCartMesh->SetBoneTransformByName(BoneName, WheelTransform, EBoneSpaces::ComponentSpace);
    };

    ApplyWheelRotation(WheelFrontLeft);
    ApplyWheelRotation(WheelFrontRight);
    ApplyWheelRotation(WheelRearLeft);
    ApplyWheelRotation(WheelRearRight);
}
