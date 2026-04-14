// ============================================================
// Source/Campus_Itabira/MyActor.h
// ============================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ROS2NodeComponent.h"
#include "ROS2Publisher.h"
#include "ROS2Subscriber.h"

#include "CesiumGlobeAnchorComponent.h"
#include "CesiumGeoreference.h"

#include "MyActor.generated.h"               // ← SEMPRE por último!

UCLASS()
class CAMPUS_ITABIRA_API AMyActor : public AActor
{
    GENERATED_BODY()

public:
    AMyActor();

    /** Texto formatado com Lat/Lon/X/Y/Heading/Vel — lido pelo ALocalizacaoHUD */
    FString DisplayText;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    /* -------- ROS 2 -------- */
    UPROPERTY() UROS2NodeComponent* NodeComponent      = nullptr;
    UPROPERTY() UROS2Publisher*     PosePublisher      = nullptr;
    UPROPERTY() UROS2Subscriber*    DisplaySubscriber  = nullptr;

    /* -------- Cesium ------- */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = "true"))
    UCesiumGlobeAnchorComponent* GlobeAnchor = nullptr;

    ACesiumGeoreference* Georef = nullptr;

    /* -------- Última posição recebida ----- */
    double LastLat = 0.0;
    double LastLon = 0.0;
    bool   bHasFix = false;

    /* -------- Callbacks ----- */
    UFUNCTION()
    void OnDisplayReceived(const UROS2GenericMsg* InMsg);
};
