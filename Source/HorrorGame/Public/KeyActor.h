#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeyActor.generated.h"

class UStaticMeshComponent;
class ADoorActor;
class AHorrorGameCharacter;

UENUM()
enum class EKeyAnimationPhase : uint8
{
    None,
    Translating,
    Rotating
};

UCLASS()
class HORRORGAME_API AKeyActor : public AActor
{
    GENERATED_BODY()

public:
    AKeyActor();

    virtual void Tick(float DeltaSeconds) override;

    void StartInsertAnimation(
        const FTransform& StartTransform,
        const FTransform& TargetTransform,
        float DurationSeconds,
        ADoorActor* TargetDoor,
        AHorrorGameCharacter* OwningCharacter
    );

    void SetKeyMesh(UStaticMesh* Mesh);

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere)
    USceneComponent* Root;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* KeyMeshComp;

    // --- Animation ---
    EKeyAnimationPhase Phase;

    float Duration;
    float Elapsed;

    FTransform FromTransform;
    FTransform ToTransform;

    // Rotation phase
    FQuat RotationStart;
    FQuat RotationEnd;

    UPROPERTY(EditAnywhere, Category="Key Animation")
    float RotationDegrees = -90.f;

    UPROPERTY(EditAnywhere, Category="Key Animation")
    float RotationDuration = 0.4f;

    // callbacks
    UPROPERTY()
    ADoorActor* CallbackDoor;

    UPROPERTY()
    AHorrorGameCharacter* CallbackCharacter;

    void FinishAnimation();
};