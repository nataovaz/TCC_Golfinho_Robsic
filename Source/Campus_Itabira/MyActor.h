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

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    /* -------- ROS 2 -------- */
    UPROPERTY() UROS2NodeComponent* NodeComponent = nullptr;
    UPROPERTY() UROS2Publisher*     PosePublisher = nullptr;
    UPROPERTY() UROS2Subscriber*    Subscriber    = nullptr;

    /* -------- Cesium ------- */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = "true"))
    UCesiumGlobeAnchorComponent* GlobeAnchor = nullptr;

    ACesiumGeoreference* Georef = nullptr;

    /* -------- Callback ----- */
    UFUNCTION()
    void OnMessageReceived(const UROS2GenericMsg* InMsg);
};
