#include "KeyActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DoorActor.h"
#include "HorrorGameCharacter.h"

AKeyActor::AKeyActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    KeyMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KeyMesh"));
    KeyMeshComp->SetupAttachment(Root);
    KeyMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Phase = EKeyAnimationPhase::None;
    Duration = 1.0f;
    Elapsed = 0.f;

    CallbackDoor = nullptr;
    CallbackCharacter = nullptr;
}

void AKeyActor::BeginPlay()
{
    Super::BeginPlay();
}

void AKeyActor::SetKeyMesh(UStaticMesh* Mesh)
{
    if (KeyMeshComp && Mesh)
    {
        KeyMeshComp->SetStaticMesh(Mesh);
    }
}

void AKeyActor::StartInsertAnimation(
    const FTransform& StartTransform,
    const FTransform& TargetTransform,
    float DurationSeconds,
    ADoorActor* TargetDoor,
    AHorrorGameCharacter* OwningCharacter)
{
    FromTransform = StartTransform;
    ToTransform = TargetTransform;

    Duration = FMath::Max(0.01f, DurationSeconds);
    Elapsed = 0.f;

    Phase = EKeyAnimationPhase::Translating;

    CallbackDoor = TargetDoor;
    CallbackCharacter = OwningCharacter;

    SetActorTransform(FromTransform);
}

void AKeyActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (Phase == EKeyAnimationPhase::None)
        return;

    Elapsed += DeltaSeconds;

    // =========================
    // PHASE 1: TRANSLATION
    // =========================
    if (Phase == EKeyAnimationPhase::Translating)
    {
        float Alpha = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
        float SmoothAlpha = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

        FVector NewLoc = FMath::Lerp(
            FromTransform.GetLocation(),
            ToTransform.GetLocation(),
            SmoothAlpha
        );

        SetActorLocation(NewLoc);

        if (Alpha >= 1.f)
        {
            // prepare rotation phase
            Phase = EKeyAnimationPhase::Rotating;
            Elapsed = 0.f;

            RotationStart = GetActorQuat();

            const FVector RotationAxis =
                ToTransform.GetRotation().GetAxisY(); // WORLD-SPACE AXIS

            const FQuat TurnQuat(
                RotationAxis,
                FMath::DegreesToRadians(RotationDegrees)
            );

            RotationEnd = TurnQuat * RotationStart;
        }
        return;
    }

    // =========================
    // PHASE 2: ROTATION
    // =========================
    if (Phase == EKeyAnimationPhase::Rotating)
    {
        float Alpha = FMath::Clamp(Elapsed / RotationDuration, 0.f, 1.f);
        float SmoothAlpha = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

        FQuat NewRot = FQuat::Slerp(
            RotationStart,
            RotationEnd,
            SmoothAlpha
        );

        SetActorRotation(NewRot);

        if (Alpha >= 1.f)
        {
            FinishAnimation();
        }
    }
}

void AKeyActor::FinishAnimation()
{
    Phase = EKeyAnimationPhase::None;

    if (CallbackDoor)
    {
        CallbackDoor->UnlockDoor();
        CallbackDoor->ToggleDoor();
    }

    if (CallbackCharacter)
    {
        CallbackCharacter->EndDoorUnlockSequence(true);
    }

    Destroy();
}