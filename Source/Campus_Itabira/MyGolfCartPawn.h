#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"                  // ← trocado
#include "ROS2NodeComponent.h"
#include "ROS2Publisher.h"
#include "ROS2Subscriber.h"
#include "CesiumGlobeAnchorComponent.h"
#include "CesiumGeoreference.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

#include "MyGolfCartPawn.generated.h"

UCLASS()
class CAMPUS_ITABIRA_API AMyGolfCartPawn : public APawn
{
    GENERATED_BODY()

public:
    AMyGolfCartPawn();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    /* ----- ROS2 ----- */
    UPROPERTY() UROS2NodeComponent* NodeComponent = nullptr;
    UPROPERTY() UROS2Publisher*     PosePublisher = nullptr;
    UPROPERTY() UROS2Subscriber*    Subscriber    = nullptr;

    /* ----- Cesium ---- */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess="true"))
    UCesiumGlobeAnchorComponent* GlobeAnchor = nullptr;
    ACesiumGeoreference* Georef = nullptr;

    /* ----- Câmera ----- */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess="true"))
    USpringArmComponent* SpringArm = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess="true"))
    UCameraComponent* Cam = nullptr;

    /* ----- Callback ROS2 ---- */
    UFUNCTION()
    void OnMessageReceived(const UROS2GenericMsg* InMsg);
};
