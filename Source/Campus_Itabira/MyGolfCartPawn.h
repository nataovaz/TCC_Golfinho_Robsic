#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "ROS2NodeComponent.h"
#include "ROS2Publisher.h"
#include "ROS2Subscriber.h"
#include "CesiumGlobeAnchorComponent.h"
#include "CesiumGeoreference.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"

#include "MyGolfCartPawn.generated.h"

class UInputAction;

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

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    /* ---------- ROS2 ---------- */
    UPROPERTY() UROS2NodeComponent* NodeComponent = nullptr;
    UPROPERTY() UROS2Publisher*     PosePublisher = nullptr;
    UPROPERTY() UROS2Subscriber*    Subscriber    = nullptr;

    /* ---------- Cesium ---------- */
    UPROPERTY(VisibleAnywhere, meta=(AllowPrivateAccess="true"))
    UCesiumGlobeAnchorComponent* GlobeAnchor = nullptr;
    ACesiumGeoreference* Georef = nullptr;

    /* ---------- Câmera top-down ---------- */
    UPROPERTY() USceneComponent*   CamRoot     = nullptr;   // pivot
    UPROPERTY() UCameraComponent*  TopDownCam  = nullptr;   // câmera real

    /* ---------- Movimento ---------- */
    UPROPERTY(VisibleAnywhere, meta=(AllowPrivateAccess="true"))
    UFloatingPawnMovement* MoveComp = nullptr;

    /* ---------- Enhanced Input ---------- */
    UPROPERTY(EditDefaultsOnly, Category="Input") UInputAction* IA_MoveForward = nullptr;
    UPROPERTY(EditDefaultsOnly, Category="Input") UInputAction* IA_MoveRight   = nullptr;

    /* ---------- callbacks ---------- */
    void OnMoveForward(const FInputActionValue& Value);
    void OnMoveRight  (const FInputActionValue& Value);

    /* ---------- ROS2 callback ---------- */
    UFUNCTION()
    void OnMessageReceived(const UROS2GenericMsg* InMsg);
};
