#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "TimerManager.h"
#include "ROS2NodeComponent.h"
#include "ROS2Publisher.h"
#include "ROS2Subscriber.h"
#include "Cesium3DTileset.h"
#include "CesiumGlobeAnchorComponent.h"
#include "CesiumGeoreference.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "HAL/CriticalSection.h"

#include "MyGolfCartPawn.generated.h"

class UInputAction;
class UPoseableMeshComponent;
class USkeletalMeshComponent;
struct FCesiumSampleHeightResult;

/**
 * Pawn do carrinho de golfe.
 *  – Publica pose via ROS 2  
 *  – Exibe HUD com LLA  
 *  – Câmera top-down fixa em C++
 */
UCLASS()
class CAMPUS_ITABIRA_API AMyGolfCartPawn : public APawn
{
    GENERATED_BODY()

public:
    AMyGolfCartPawn();

    /** Texto formatado exibido no HUD principal. */
    FString DisplayText;

    /** Ultima posicao GPS recebida do pipeline Flutter -> ROS 2. */
    double LastLat = 0.0;
    double LastLon = 0.0;
    bool bHasFix = false;

    /** Destino para interpolação suave de posição (evita saltos a 10 Hz). */
    double TargetLat = 0.0;
    double TargetLon = 0.0;
    double LastHeadingDeg = 0.0;
    double TargetHeadingDeg = 0.0;
    bool bHasTarget = false;

    /**
     * Posição inicial do carrinho no mapa.
     * Quando bUseCustomSpawnLocation = true, usa estas coordenadas em vez da
     * origem do CesiumGeoreference, permitindo nascer sobre uma rua real.
     * Configure no Blueprint/Details do ator na cena.
     *
     * Padrão: Entrada da UNIFEI Itabira (lat=-19.673652, lon=-43.213106).
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Spawn|GPS",
              meta=(ClampMin="-90.0", ClampMax="90.0"))
    double InitialSpawnLat = -19.673026;   // RobSIC — calculado de UE(2234,-2641,710)

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Spawn|GPS",
              meta=(ClampMin="-180.0", ClampMax="180.0"))
    double InitialSpawnLon = -43.214130;   // RobSIC

    /** Altitude inicial em metros (WGS-84). 0 = usa a origem do Georef ou fallback. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Spawn|GPS",
              meta=(ClampMin="0.0"))
    double InitialSpawnHeight = 851.0;     // RobSIC: h_origin(843.93) + Z(7.10) ≈ 851 m

    /** Se false, volta ao comportamento original (origem do CesiumGeoreference). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Spawn|GPS")
    bool bUseCustomSpawnLocation = true;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    /* ---------- ROS2 ---------- */
    UPROPERTY() UROS2NodeComponent* NodeComponent = nullptr;
    UPROPERTY() UROS2Publisher*     PosePublisher = nullptr;
    UPROPERTY() UROS2Publisher*     StatePublisher = nullptr;
    UPROPERTY() UROS2Subscriber*    Subscriber    = nullptr;
    FTimerHandle DeferredRosInitTimerHandle;
    bool bRosInterfacesInitialized = false;

    /* ---------- Cesium ---------- */
    UPROPERTY(VisibleAnywhere, meta=(AllowPrivateAccess="true"))
    UCesiumGlobeAnchorComponent* GlobeAnchor = nullptr;
    ACesiumGeoreference* Georef = nullptr;
    UPROPERTY(Transient)
    ACesium3DTileset* GroundTileset = nullptr;

    /* ---------- Câmera top-down ---------- */
    UPROPERTY() USceneComponent*   CamRoot     = nullptr;   // pivot
    UPROPERTY() UCameraComponent*  TopDownCam  = nullptr;   // câmera real

    /* ---------- Movimento ---------- */
    UPROPERTY(VisibleAnywhere, meta=(AllowPrivateAccess="true"))
    UFloatingPawnMovement* MoveComp = nullptr;

    /* ---------- Visual do carrinho ---------- */
    UPROPERTY(Transient)
    UPoseableMeshComponent* VisualCartMesh = nullptr;

    UPROPERTY(Transient)
    USkeletalMeshComponent* SourceCartMesh = nullptr;

    FVector LastActorLocation = FVector::ZeroVector;
    float WheelSpinDegrees = 0.f;
    TMap<FName, FTransform> BaseWheelBoneTransforms;
    double LastAppliedHeight = 850.0;
    double LastSampleRequestLon = 0.0;
    double LastSampleRequestLat = 0.0;
    double BaseActorYawDeg = 0.0;
    float LastGroundSampleRequestTime = -1000.f;
    float LastStatePublishTimeSeconds = -1000.f;
    bool bHeightSampleInFlight = false;
    FTimerHandle DeferredInitialHeightSampleTimerHandle;
    FCriticalSection PendingRosDataMutex;
    FString PendingDisplayText;
    double PendingTargetLat = 0.0;
    double PendingTargetLon = 0.0;
    double PendingTargetHeadingDeg = 0.0;
    bool bHasPendingRosPose = false;

    /* ---------- Enhanced Input ---------- */
    UPROPERTY(EditDefaultsOnly, Category="Input") UInputAction* IA_MoveForward = nullptr;
    UPROPERTY(EditDefaultsOnly, Category="Input") UInputAction* IA_MoveRight   = nullptr;

    /* ---------- callbacks ---------- */
    void OnMoveForward(const FInputActionValue& Value);
    void OnMoveRight  (const FInputActionValue& Value);

    void InitializeRosInterfaces();
    void InitSpawnStateOnly();
    void InitializeVisualCartMesh();
    void UpdateVisualWheelSpin(float DeltaTime);
    void SpawnAtGeoreferenceOrigin();
    ACesium3DTileset* ResolveGroundTileset();
    void RequestInitialGroundSample();
    void MaybeRequestGroundHeightSample(double Longitude, double Latitude, bool bForce = false);
    void RequestGroundHeightSample(double Longitude, double Latitude);
    void MoveCartToLongitudeLatitudeHeight(double Longitude, double Latitude, double Height);

    /* ---------- ROS2 callback ---------- */
    UFUNCTION()
    void OnMessageReceived(const UROS2GenericMsg* InMsg);
};
