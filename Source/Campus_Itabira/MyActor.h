// =============================
// MyActor.h
// =============================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ROS2NodeComponent.h"
#include "ROS2Publisher.h"
#include "ROS2Subscriber.h"
#include "CesiumGeoreference.h"
#include "MyActor.generated.h"

UCLASS()
class CAMPUS_ITABIRA_API AMyActor : public AActor
{
    GENERATED_BODY()

public:
    AMyActor();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY()
    UROS2NodeComponent* NodeComponent;

    UPROPERTY()
    UROS2Publisher* PosePublisher;

    UPROPERTY()
    UROS2Subscriber* Subscriber;

    ACesiumGeoreference* Georef = nullptr;

    UFUNCTION()
    void OnMessageReceived(const UROS2GenericMsg* InMsg);
};
