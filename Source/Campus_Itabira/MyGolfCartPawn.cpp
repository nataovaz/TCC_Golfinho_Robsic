// MyGolfCartPawn.cpp ---------------------------------------------------------
#include "MyGolfCartPawn.h"
#include "CesiumSampleHeightResult.h"
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
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeLock.h"
#include "TimerManager.h"

namespace
{
const FName WheelFrontLeft(TEXT("Wheel_Front_Left"));
const FName WheelFrontRight(TEXT("Wheel_Front_Right"));
const FName WheelRearLeft(TEXT("Wheel_Rear_Left"));
const FName WheelRearRight(TEXT("Wheel_Rear_Right"));
const FName GoogleTilesetTag(TEXT("CampusGooglePhotorealisticTiles"));
constexpr double kFallbackHeightMeters = 850.0;   // altitude WGS-84 segura acima do terreno de Itabira-MG
constexpr double kCartHeightOffsetMeters = 1.25;
constexpr double kGroundSampleMinMoveDegrees = 0.00002;
constexpr double kSampleComparisonEpsilonDegrees = 1e-6;
constexpr float kGroundSampleIntervalSeconds = 1.0f;
// Delay maior: permite ao motor estabilizar antes de inicializar ROS e Cesium,
// reduzindo o pico de CPU/RAM que trava o desktop no startup.
constexpr float kDeferredRosInitDelaySeconds = 2.0f;
constexpr float kInitialGroundSampleDelaySeconds = 4.0f;
constexpr float kStatePublishIntervalSeconds = 0.1f;
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
    // A rotacao inicial do ator no editor representa o alinhamento do modelo
    // do campus no mundo Unreal. O heading vindo do pipeline ROS deve girar
    // o carrinho em torno dessa referencia, nao substitui-la.
    BaseActorYawDeg = GetActorRotation().Yaw;
    DisplayText = TEXT("Aguardando GPS fix...");
    InitializeVisualCartMesh();

    /* ROS2  */
    if (CampusRos2RuntimeGuard::CanInitializeRos2())
    {
        GetWorldTimerManager().SetTimer(
            DeferredRosInitTimerHandle,
            this,
            &AMyGolfCartPawn::InitializeRosInterfaces,
            kDeferredRosInitDelaySeconds,
            false);
    }

    /* Cesium  */
    if (ACesiumGeoreference* G = ACesiumGeoreference::GetDefaultGeoreference(GetWorld()))
    {
        Georef = G;
        GlobeAnchor->SetGeoreference(G);

        const FVector OriginLLH = G->GetOriginLongitudeLatitudeHeight();
        UE_LOG(LogTemp, Display,
            TEXT("[DIAG] CesiumGeoreference origin: Lon=%.8f Lat=%.8f Height=%.2f"),
            OriginLLH.X, OriginLLH.Y, OriginLLH.Z);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[DIAG] CesiumGeoreference NAO encontrado!"));
    }

    // Posição do ator no mundo Unreal antes de qualquer teleporte
    const FVector WorldPos = GetActorLocation();
    UE_LOG(LogTemp, Display,
        TEXT("[DIAG] Cart world pos inicial (cm): X=%.1f Y=%.1f Z=%.1f"),
        WorldPos.X, WorldPos.Y, WorldPos.Z);

    GroundTileset = ResolveGroundTileset();

    // Inicializa as variáveis de estado GPS sem mover o ator.
    // O carrinho nasce onde foi posicionado no editor (no chão do modelo 3D).
    // Altura fixa = kFallbackHeightMeters (850 m WGS-84 sobre Itabira).
    // SampleHeightMostDetailed desativado — forçava carregamento de tiles em
    // resolução máxima, causando pico de CPU/GPU e travamento do PC.
    InitSpawnStateOnly();

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

void AMyGolfCartPawn::InitializeRosInterfaces()
{
    if (bRosInterfacesInitialized || !NodeComponent)
    {
        return;
    }

    NodeComponent->Init();
    PosePublisher = NodeComponent->CreatePublisher(
        TEXT("/campus_pose"),
        UROS2Publisher::StaticClass(),
        UROS2PoseStampedMsg::StaticClass(),
        UROS2QoS::SensorData);

    StatePublisher = NodeComponent->CreatePublisher(
        TEXT("/estado_simulacao"),
        UROS2Publisher::StaticClass(),
        UROS2StrMsg::StaticClass(),
        UROS2QoS::SensorData);

    FSubscriptionCallback Callback;
    Callback.BindDynamic(this, &AMyGolfCartPawn::OnMessageReceived);
    Subscriber = NodeComponent->CreateSubscriber(
        TEXT("/localizacao_display"),
        UROS2StrMsg::StaticClass(),
        Callback,
        UROS2QoS::SensorData);

    bRosInterfacesInitialized =
        PosePublisher != nullptr || StatePublisher != nullptr || Subscriber != nullptr;

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Golf cart ROS setup | node_state=%d pose_pub=%s state_pub=%s subscriber=%s"),
        static_cast<int32>(NodeComponent->State.GetValue()),
        PosePublisher ? TEXT("ok") : TEXT("null"),
        StatePublisher ? TEXT("ok") : TEXT("null"),
        Subscriber ? TEXT("ok") : TEXT("null"));
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

    {
        FScopeLock Lock(&PendingRosDataMutex);
        if (bHasPendingRosPose)
        {
            DisplayText = PendingDisplayText;
            TargetLat = PendingTargetLat;
            TargetLon = PendingTargetLon;
            TargetHeadingDeg = PendingTargetHeadingDeg;
            bHasTarget = true;
            bHasFix = true;
            bHasPendingRosPose = false;
        }
    }

    // ── Interpolação suave de posição GPS (10 Hz → 60 Hz) ──────────────────
    // Lerp da posição atual até o destino a cada frame para eliminar saltos.
    if (bHasTarget && GlobeAnchor)
    {
        constexpr double kLerpSpeed = 8.0;   // converge em ~3 frames a 60 fps
        const double Alpha = FMath::Min(1.0, kLerpSpeed * static_cast<double>(DeltaTime));

        const double NewLat = LastLat + Alpha * (TargetLat - LastLat);
        const double NewLon = LastLon + Alpha * (TargetLon - LastLon);
        const double HeadingDeltaDeg = FMath::FindDeltaAngleDegrees(
            static_cast<float>(LastHeadingDeg),
            static_cast<float>(TargetHeadingDeg));
        const double NewHeadingDeg =
            FMath::UnwindDegrees(LastHeadingDeg + Alpha * HeadingDeltaDeg);

        const bool bLatChanged = !FMath::IsNearlyEqual(NewLat, LastLat, 1e-9);
        const bool bLonChanged = !FMath::IsNearlyEqual(NewLon, LastLon, 1e-9);
        const bool bHeadingChanged =
            !FMath::IsNearlyEqual(NewHeadingDeg, LastHeadingDeg, 1e-4);

        LastHeadingDeg = NewHeadingDeg;

        if (bLatChanged || bLonChanged)
        {
            LastLat = NewLat;
            LastLon = NewLon;
            MoveCartToLongitudeLatitudeHeight(LastLon, LastLat, LastAppliedHeight + kCartHeightOffsetMeters);
        }

        if (bHeadingChanged)
        {
            FRotator NewRotation = GetActorRotation();
            // O heading do EKF cresce no sentido matematico (Leste -> Norte),
            // mas o mapa local do Unreal/Cesium usa +Y apontando para Sul.
            // Para a frente visual do carrinho coincidir com o deslocamento
            // georreferenciado, o yaw aplicado no ator precisa inverter o sinal.
            NewRotation.Yaw = static_cast<float>(BaseActorYawDeg - LastHeadingDeg);
            SetActorRotation(NewRotation);
        }
    }

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

    if (StatePublisher && (T - LastStatePublishTimeSeconds) >= kStatePublishIntervalSeconds)
    {
        FROSStr SimState;
        SimState.Data = FString::Printf(
            TEXT("{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f,\"heading\":%.3f}"),
            Loc.X / 100.f,
            Loc.Y / 100.f,
            Loc.Z / 100.f,
            LastHeadingDeg);
        StatePublisher->Publish<UROS2StrMsg, FROSStr>(SimState);
        LastStatePublishTimeSeconds = T;
    }

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
    const auto* Msg = Cast<UROS2StrMsg>(In);
    if (!Msg)
    {
        return;
    }

    FROSStr Data;
    Msg->GetMsg(Data);

    TArray<FString> Lines;
    Data.Data.ParseIntoArray(Lines, TEXT("\n"), true);

    double NewLat = LastLat;
    double NewLon = LastLon;
    double NewHeadingDeg = TargetHeadingDeg;

    for (const FString& Line : Lines)
    {
        FString Key;
        FString Val;
        if (!Line.Split(TEXT(": "), &Key, &Val))
        {
            continue;
        }

        Key.TrimStartAndEndInline();
        Val.TrimStartAndEndInline();

        if (Key.Equals(TEXT("Lat")))
        {
            NewLat = FCString::Atod(*Val);
        }
        else if (Key.Equals(TEXT("Lon")))
        {
            NewLon = FCString::Atod(*Val);
        }
        else if (Key.Equals(TEXT("Heading")))
        {
            NewHeadingDeg = FCString::Atod(*Val);
        }
    }

    if (NewLat == 0.0 || NewLon == 0.0)
    {
        return;
    }

    {
        FScopeLock Lock(&PendingRosDataMutex);
        PendingDisplayText = Data.Data;
        PendingTargetLat = NewLat;
        PendingTargetLon = NewLon;
        PendingTargetHeadingDeg = NewHeadingDeg;
        bHasPendingRosPose = true;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("[MyGolfCartPawn] Posicao: Lat=%.6f Lon=%.6f Heading=%.1f"),
        NewLat,
        NewLon,
        NewHeadingDeg);
}

// ─────────── ENDPLAY --------------------------------------------------------
void AMyGolfCartPawn::EndPlay(const EEndPlayReason::Type Reason)
{
    // Limpa timers antes de qualquer outra coisa.
    GetWorldTimerManager().ClearAllTimersForObject(this);

    // Destrói subscriber e publisher explicitamente para liberar
    // recursos rcl/DDS antes que o NodeComponent seja destruído.
    // Sem isso, o executor do rclUE pode ficar em loop infinito e
    // travar o editor/PC ao fechar o projeto.
    if (IsValid(Subscriber))
    {
        Subscriber->Destroy();
        Subscriber = nullptr;
    }
    if (IsValid(PosePublisher))
    {
        PosePublisher->Destroy();
        PosePublisher = nullptr;
    }
    if (IsValid(StatePublisher))
    {
        StatePublisher->Destroy();
        StatePublisher = nullptr;
    }

    // Marca como não inicializado para evitar re-entradas.
    bRosInterfacesInitialized = false;

    Super::EndPlay(Reason);
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

void AMyGolfCartPawn::InitSpawnStateOnly()
{
    // Prioridade: deriva GPS diretamente do GlobeAnchor (posição real do ator
    // no editor + CesiumGeoreference). Evita usar UPROPERTYs que o Blueprint
    // pode ter em cache com valores antigos, causando salto visual no início.
    if (GlobeAnchor && Georef)
    {
        const FVector LLH = GlobeAnchor->GetLongitudeLatitudeHeight();
        if (!FMath::IsNearlyZero(LLH.Y) && !FMath::IsNearlyZero(LLH.X))
        {
            LastLon           = LLH.X;
            LastLat           = LLH.Y;
            LastAppliedHeight = (LLH.Z > 100.0) ? LLH.Z : kFallbackHeightMeters;
            UE_LOG(LogTemp, Display,
                TEXT("[MyGolfCartPawn] Estado GPS derivado do GlobeAnchor: Lat=%.6f Lon=%.6f Height=%.1f"),
                LastLat, LastLon, LastAppliedHeight);
            return;
        }
    }

    // Fallback: usa UPROPERTYs configuráveis
    if (bUseCustomSpawnLocation)
    {
        LastLat = InitialSpawnLat;
        LastLon = InitialSpawnLon;
        LastAppliedHeight = (InitialSpawnHeight > 0.0)
            ? InitialSpawnHeight
            : kFallbackHeightMeters;
    }
    else if (Georef)
    {
        const FVector O = Georef->GetOriginLongitudeLatitudeHeight();
        LastLon           = O.X;
        LastLat           = O.Y;
        LastAppliedHeight = O.Z > 0.0 ? O.Z : kFallbackHeightMeters;
    }
    else
    {
        LastAppliedHeight = kFallbackHeightMeters;
    }

    UE_LOG(LogTemp, Display,
        TEXT("[MyGolfCartPawn] Estado GPS inicializado (fallback UPROPERTY): Lat=%.6f Lon=%.6f Height=%.1f"),
        LastLat, LastLon, LastAppliedHeight);
}

void AMyGolfCartPawn::SpawnAtGeoreferenceOrigin()
{
    if (!GlobeAnchor || !Georef)
    {
        return;
    }

    if (bUseCustomSpawnLocation)
    {
        // Usa coordenadas configuráveis (padrão: Entrada da UNIFEI Itabira).
        LastLat = InitialSpawnLat;
        LastLon = InitialSpawnLon;

        // Resolve altura: prioridade → UPROPERTY → origem do Georef → fallback.
        if (InitialSpawnHeight > 0.0)
        {
            LastAppliedHeight = InitialSpawnHeight;
        }
        else
        {
            const FVector OriginLlh = Georef->GetOriginLongitudeLatitudeHeight();
            LastAppliedHeight = OriginLlh.Z > 0.0 ? OriginLlh.Z : kFallbackHeightMeters;
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("[MyGolfCartPawn] Spawn em posicao customizada (rua): Lat=%.6f Lon=%.6f Height=%.1f"),
            LastLat,
            LastLon,
            LastAppliedHeight);
    }
    else
    {
        // Comportamento original: nasce na origem do CesiumGeoreference.
        const FVector OriginLlh = Georef->GetOriginLongitudeLatitudeHeight();
        LastLon = OriginLlh.X;
        LastLat = OriginLlh.Y;
        LastAppliedHeight = OriginLlh.Z > 0.0 ? OriginLlh.Z : kFallbackHeightMeters;

        UE_LOG(
            LogTemp,
            Display,
            TEXT("[MyGolfCartPawn] Spawn na origem do CesiumGeoreference: Lon=%.6f Lat=%.6f Height=%.1f"),
            LastLon,
            LastLat,
            LastAppliedHeight);
    }

    MoveCartToLongitudeLatitudeHeight(
        LastLon,
        LastLat,
        LastAppliedHeight + kCartHeightOffsetMeters);
}

void AMyGolfCartPawn::RequestInitialGroundSample()
{
    MaybeRequestGroundHeightSample(LastLon, LastLat, true);
}

ACesium3DTileset* AMyGolfCartPawn::ResolveGroundTileset()
{
    if (IsValid(GroundTileset))
    {
        return GroundTileset;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    ACesium3DTileset* FallbackTileset = nullptr;

    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        ACesium3DTileset* Tileset = *It;
        if (!IsValid(Tileset))
        {
            continue;
        }

        if (Tileset->ActorHasTag(GoogleTilesetTag) ||
            Tileset->GetUrl().Contains(TEXT("tile.googleapis.com")))
        {
            GroundTileset = Tileset;
            break;
        }

        if (!FallbackTileset)
        {
            FallbackTileset = Tileset;
        }
    }

    if (!GroundTileset)
    {
        GroundTileset = FallbackTileset;
    }

    if (GroundTileset)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("[MyGolfCartPawn] Tileset de solo resolvido: %s"),
            *GroundTileset->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[MyGolfCartPawn] Nenhum Cesium3DTileset encontrado para amostrar altura."));
    }

    return GroundTileset;
}

void AMyGolfCartPawn::RequestGroundHeightSample(double Longitude, double Latitude)
{
    if (bHeightSampleInFlight)
    {
        return;
    }

    ACesium3DTileset* Tileset = ResolveGroundTileset();
    if (!Tileset)
    {
        LastAppliedHeight = kFallbackHeightMeters;
        return;
    }

    bHeightSampleInFlight = true;
    LastSampleRequestLon = Longitude;
    LastSampleRequestLat = Latitude;
    LastGroundSampleRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    TArray<FVector> Positions;
    Positions.Add(FVector(Longitude, Latitude, LastAppliedHeight));

    TWeakObjectPtr<AMyGolfCartPawn> WeakThis(this);
    FCesiumSampleHeightMostDetailedCallback Callback;
    Callback.BindLambda(
        [WeakThis, Longitude, Latitude](
            ACesium3DTileset* /*TilesetActor*/,
            const TArray<FCesiumSampleHeightResult>& Results,
            const TArray<FString>& Warnings)
        {
            AMyGolfCartPawn* Self = WeakThis.Get();
            if (!Self)
            {
                return;
            }

            Self->bHeightSampleInFlight = false;

            double SampledHeight = Self->LastAppliedHeight;
            if (Results.Num() > 0 && Results[0].SampleSuccess)
            {
                SampledHeight = Results[0].LongitudeLatitudeHeight.Z;
                Self->LastAppliedHeight = SampledHeight;
            }
            else if (Self->LastAppliedHeight <= 0.0)
            {
                Self->LastAppliedHeight = kFallbackHeightMeters;
                SampledHeight = Self->LastAppliedHeight;
            }

            if (Warnings.Num() > 0)
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("[MyGolfCartPawn] Avisos ao amostrar altura do tileset: %s"),
                    *FString::Join(Warnings, TEXT(" | ")));
            }

            const bool bSampleStillCurrent =
                FMath::IsNearlyEqual(Longitude, Self->LastLon, kSampleComparisonEpsilonDegrees) &&
                FMath::IsNearlyEqual(Latitude, Self->LastLat, kSampleComparisonEpsilonDegrees);

            if (bSampleStillCurrent)
            {
                Self->MoveCartToLongitudeLatitudeHeight(
                    Self->LastLon,
                    Self->LastLat,
                    Self->LastAppliedHeight + kCartHeightOffsetMeters);
            }
        });

    Tileset->SampleHeightMostDetailed(Positions, Callback);
}

void AMyGolfCartPawn::MaybeRequestGroundHeightSample(
    double Longitude,
    double Latitude,
    bool bForce)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (World->GetTimeSeconds() < kInitialGroundSampleDelaySeconds && !bForce)
    {
        return;
    }

    if (bHeightSampleInFlight)
    {
        return;
    }

    const bool bMovedEnough =
        FMath::Abs(Longitude - LastSampleRequestLon) + FMath::Abs(Latitude - LastSampleRequestLat) >
        kGroundSampleMinMoveDegrees;
    const bool bIntervalElapsed =
        (World->GetTimeSeconds() - LastGroundSampleRequestTime) >= kGroundSampleIntervalSeconds;

    if (bForce || (bMovedEnough && bIntervalElapsed))
    {
        RequestGroundHeightSample(Longitude, Latitude);
    }
}

void AMyGolfCartPawn::MoveCartToLongitudeLatitudeHeight(
    double Longitude,
    double Latitude,
    double Height)
{
    if (!GlobeAnchor)
    {
        return;
    }

    GlobeAnchor->MoveToLongitudeLatitudeHeight(FVector(Longitude, Latitude, Height));

    static bool bFirstTeleportLog = true;
    if (bFirstTeleportLog)
    {
        bFirstTeleportLog = false;
        const FVector After = GetActorLocation();
        UE_LOG(LogTemp, Display,
            TEXT("[DIAG] Pos apos GlobeAnchor teleporte (cm): X=%.1f Y=%.1f Z=%.1f"),
            After.X, After.Y, After.Z);
    }
}
