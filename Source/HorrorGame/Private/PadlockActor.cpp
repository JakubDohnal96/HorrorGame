// PadlockActor.cpp
#include "PadlockActor.h"
#include "DoorActor.h"
#include "HorrorGameCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "InteractableUtils.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"

APadlockActor::APadlockActor()
{
    PrimaryActorTick.bCanEverTick = true;

    /* --- Root --- */
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    /* --- Frame parts --- */
    StaticLockFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticLockFrame"));
    StaticLockFrame->SetupAttachment(Root);
    StaticLockFrame->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RotatableLockFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RotatableLockFrame"));
    RotatableLockFrame->SetupAttachment(Root);
    RotatableLockFrame->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    StaticLockDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticLockDoor"));
    StaticLockDoor->SetupAttachment(Root);
    StaticLockDoor->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    /* --- PadlockU (does not swing, just hides) --- */
    PadlockU = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockU"));
    PadlockU->SetupAttachment(Root);
    PadlockU->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    /* --- SwingPivot: position this at PadlockLeft's hinge point in the BP --- */
    SwingPivot = CreateDefaultSubobject<USceneComponent>(TEXT("SwingPivot"));
    SwingPivot->SetupAttachment(Root);

    PadlockLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockLeft"));
    PadlockLeft->SetupAttachment(SwingPivot);
    PadlockLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PadlockRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockRight"));
    PadlockRight->SetupAttachment(SwingPivot);
    PadlockRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PadlockRotatable1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockRotatable1"));
    PadlockRotatable1->SetupAttachment(SwingPivot);
    PadlockRotatable1->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PadlockRotatable2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockRotatable2"));
    PadlockRotatable2->SetupAttachment(SwingPivot);
    PadlockRotatable2->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PadlockRotatable3 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockRotatable3"));
    PadlockRotatable3->SetupAttachment(SwingPivot);
    PadlockRotatable3->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PadlockRotatable4 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockRotatable4"));
    PadlockRotatable4->SetupAttachment(SwingPivot);
    PadlockRotatable4->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PadlockSeparation = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockSeparation"));
    PadlockSeparation->SetupAttachment(SwingPivot);
    PadlockSeparation->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PadlockBeams = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockBeams"));
    PadlockBeams->SetupAttachment(SwingPivot);
    PadlockBeams->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    /* --- Inherited components from AInteractableActor --- */
    if (InteractionBox)
    {
        InteractionBox->SetupAttachment(Root);
        InteractionBox->SetBoxExtent(FVector(30.f, 30.f, 30.f));
        InteractionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        InteractionBox->SetGenerateOverlapEvents(false);
    }

    if (ArrowWidget)
    {
        ArrowWidget->SetupAttachment(InteractionBox);
        ArrowWidget->SetWidgetSpace(EWidgetSpace::World);
        ArrowWidget->SetDrawAtDesiredSize(true);
        ArrowWidget->SetVisibility(false);
        ArrowWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ArrowWidget->SetBlendMode(EWidgetBlendMode::Transparent);
        ArrowWidget->SetRenderCustomDepth(true);
    }

    if (FullInteractionWidget)
    {
        FullInteractionWidget->SetupAttachment(InteractionBox);
        FullInteractionWidget->SetWidgetSpace(EWidgetSpace::World);
        FullInteractionWidget->SetDrawAtDesiredSize(true);
        FullInteractionWidget->SetVisibility(false);
        FullInteractionWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        FullInteractionWidget->SetBlendMode(EWidgetBlendMode::Transparent);
        FullInteractionWidget->SetRenderCustomDepth(true);
    }

    if (InteractionCamera)
    {
        InteractionCamera->SetupAttachment(Root);
        InteractionCamera->bAutoActivate = false;
    }

    /* --- Defaults --- */
    CorrectCombination = {1, 9, 4, 7};

    for (int32 i = 0; i < 4; ++i)
    {
        DialValues[i] = 0;
    }

    /* --- Mobility: all components that move/rotate/hide must be Movable --- */
    RotatableLockFrame->SetMobility(EComponentMobility::Movable);
    StaticLockDoor->SetMobility(EComponentMobility::Movable);
    PadlockU->SetMobility(EComponentMobility::Movable);
    SwingPivot->SetMobility(EComponentMobility::Movable);
    PadlockLeft->SetMobility(EComponentMobility::Movable);
    PadlockRight->SetMobility(EComponentMobility::Movable);
    PadlockRotatable1->SetMobility(EComponentMobility::Movable);
    PadlockRotatable2->SetMobility(EComponentMobility::Movable);
    PadlockRotatable3->SetMobility(EComponentMobility::Movable);
    PadlockRotatable4->SetMobility(EComponentMobility::Movable);
    PadlockSeparation->SetMobility(EComponentMobility::Movable);
    PadlockBeams->SetMobility(EComponentMobility::Movable);
}

void APadlockActor::BeginPlay()
{
    Super::BeginPlay();

    // Block the linked door
    if (LinkedDoor)
    {
        LinkedDoor->bBlockedByExternalLock = true;

        // Re-parent StaticLockDoor to the door's HingeAxis so it rotates with the door.
        // Must set Movable first — a Static component cannot attach to a Movable parent.
        if (StaticLockDoor && LinkedDoor->HingeAxis)
        {
            StaticLockDoor->SetMobility(EComponentMobility::Movable);
            StaticLockDoor->AttachToComponent(
                LinkedDoor->HingeAxis,
                FAttachmentTransformRules::KeepWorldTransform
            );
        }
    }

    // Snap SwingPivot to PadlockLeft's world position so that the Phase 2
    // body-swing animation rotates around the correct hinge point.
    // We save all children's world transforms, move the pivot, then restore them.
    if (SwingPivot && PadlockLeft)
    {
        // 1. Record every child's current world transform
        TArray<USceneComponent*> SwingChildren;
        SwingPivot->GetChildrenComponents(false, SwingChildren);

        TArray<FTransform> ChildWorldTransforms;
        for (USceneComponent* Child : SwingChildren)
        {
            ChildWorldTransforms.Add(Child->GetComponentTransform());
        }

        // 2. Move SwingPivot to PadlockLeft's world location
        SwingPivot->SetWorldLocation(PadlockLeft->GetComponentLocation());

        // 3. Restore children so they stay in their original world positions
        for (int32 i = 0; i < SwingChildren.Num(); ++i)
        {
            SwingChildren[i]->SetWorldTransform(ChildWorldTransforms[i]);
        }
    }

    // Apply initial dial rotation so the "0" on the mesh faces the correct direction.
    for (int32 i = 0; i < 4; ++i)
    {
        DialValues[i] = 0;
        ApplyDialRotation(i);
    }

    // Create dynamic material instances for highlight support
    CreateDialDynamicMaterials();
}

void APadlockActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    /* ===== Unlock animation ===== */
    if (bAnimating && AnimPhase != EPadlockAnimPhase::None)
    {
        AnimElapsed += DeltaTime;

        switch (AnimPhase)
        {
        // -------------------------------------------------------
        // PHASE 1: Shackle slides out (PadlockRight -Y)
        // -------------------------------------------------------
        case EPadlockAnimPhase::ShackleRelease:
        {
            const float Alpha = FMath::Clamp(AnimElapsed / ShackleSlideDuration, 0.f, 1.f);
            const float Smooth = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

            if (PadlockRight)
            {
                FVector NewLoc = FMath::Lerp(ShackleStartLoc, ShackleTargetLoc, Smooth);
                PadlockRight->SetRelativeLocation(NewLoc);
            }

            if (Alpha >= 1.f)
            {
                AnimPhase = EPadlockAnimPhase::BodySwing;
                AnimElapsed = 0.f;

                SwingStartRot = SwingPivot->GetRelativeRotation();
                SwingTargetRot = SwingStartRot;
                SwingTargetRot.Roll += BodySwingDegrees; // rotate around local X
            }
            break;
        }

        // -------------------------------------------------------
        // PHASE 2: Body swings open around SwingPivot X axis
        // -------------------------------------------------------
        case EPadlockAnimPhase::BodySwing:
        {
            const float Alpha = FMath::Clamp(AnimElapsed / BodySwingDuration, 0.f, 1.f);
            const float Smooth = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

            if (SwingPivot)
            {
                FRotator NewRot = FMath::Lerp(SwingStartRot, SwingTargetRot, Smooth);
                SwingPivot->SetRelativeRotation(NewRot);
            }

            if (Alpha >= 1.f)
            {
                // Hide all padlock body parts
                if (PadlockU) PadlockU->SetVisibility(false);
                if (SwingPivot) SwingPivot->SetVisibility(false, true); // propagate to children

                // Start rotating RotatableLockFrame
                AnimPhase = EPadlockAnimPhase::HideAndRotate;
                AnimElapsed = 0.f;

                FrameStartRot = RotatableLockFrame->GetRelativeRotation();
                FrameTargetRot = FrameStartRot;
                FrameTargetRot.Yaw += FrameRotateDegrees; // rotate around local Z
            }
            break;
        }

        // -------------------------------------------------------
        // PHASE 3: RotatableLockFrame swings open
        // -------------------------------------------------------
        case EPadlockAnimPhase::HideAndRotate:
        {
            const float Alpha = FMath::Clamp(AnimElapsed / FrameRotateDuration, 0.f, 1.f);
            const float Smooth = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

            if (RotatableLockFrame)
            {
                FRotator NewRot = FMath::Lerp(FrameStartRot, FrameTargetRot, Smooth);
                RotatableLockFrame->SetRelativeRotation(NewRot);
            }

            if (Alpha >= 1.f)
            {
                AnimPhase = EPadlockAnimPhase::Complete;
                FinishUnlockAnimation();
            }
            break;
        }

        default:
            break;
        }

        return; // skip widget logic during animation
    }

    /* ===== Widget visibility / rotation ===== */

    // If solved, ensure everything is off and bail out
    if (bSolved)
    {
        if (ArrowWidget) ArrowWidget->SetVisibility(false);
        if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false);
        return;
    }

    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player)
    {
        if (ArrowWidget) ArrowWidget->SetVisibility(false);
        if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false);
        return;
    }

    // Determine what should be visible right now
    const bool bCanInteract = CanShowInteraction(Player);
    const bool bCanFull     = CanShowFullInteraction(Player);

    // Arrow: visible when in range but NOT close enough for full interaction
    if (ArrowWidget) ArrowWidget->SetVisibility(bCanInteract && !bCanFull);

    // Full widget: show when close enough, hide otherwise.
    // Managed directly here because the InteractionBox has no collision,
    // so PerformInteractionScan's sphere overlap won't detect the padlock.
    if (FullInteractionWidget) FullInteractionWidget->SetVisibility(bCanFull);

    // Rotate visible widgets to face camera
    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC || !PC->PlayerCameraManager) return;

    FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();

    if (ArrowWidget && ArrowWidget->IsVisible())
        FInteractableUtils::FaceWidgetTowardsCamera(ArrowWidget, CameraLocation);
    if (FullInteractionWidget && FullInteractionWidget->IsVisible())
        FInteractableUtils::FaceWidgetTowardsCamera(FullInteractionWidget, CameraLocation);
}

/* ========== Interactable API ========== */

bool APadlockActor::CanShowInteraction(APawn* Player) const
{
    if (!Player || !InteractionBox) return false;
    if (bSolved || bAnimating) return false;

    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    return Dist <= InteractionMaxDistance;
}

bool APadlockActor::CanShowFullInteraction(APawn* Player) const
{
    if (!Player || !InteractionBox) return false;
    if (bSolved || bAnimating) return false;

    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    if (Dist > InteractionUseDistance) return false;

    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC) return true;

    FVector2D ScreenPos;
    if (!PC->ProjectWorldLocationToScreen(InteractionBox->GetComponentLocation(), ScreenPos, true))
        return false;

    int32 SizeX = 0, SizeY = 0;
    PC->GetViewportSize(SizeX, SizeY);
    return ScreenPos.X >= 0 && ScreenPos.X <= SizeX
        && ScreenPos.Y >= 0 && ScreenPos.Y <= SizeY;
}

void APadlockActor::SetFullWidgetVisible(bool bVisible, APawn* /*Player*/)
{
    if (FullInteractionWidget)
        FullInteractionWidget->SetVisibility(bVisible);
}

FVector APadlockActor::GetInteractionLocation() const
{
    return InteractionBox ? InteractionBox->GetComponentLocation() : GetActorLocation();
}

void APadlockActor::DeactivateInteractionCamera()
{
    if (InteractionCamera) InteractionCamera->Deactivate();
}

/* ========== Dial manipulation ========== */

void APadlockActor::SelectNextDial()
{
    if (bAnimating) return;
    SelectedDialIndex = (SelectedDialIndex + 1) % 4;

    if (bInInteractionMode)
        UpdateDialHighlight(SelectedDialIndex);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow,
            FString::Printf(TEXT("Dial %d selected"), SelectedDialIndex + 1));
    }
}

void APadlockActor::SelectPreviousDial()
{
    if (bAnimating) return;
    SelectedDialIndex = (SelectedDialIndex - 1 + 4) % 4;

    if (bInInteractionMode)
        UpdateDialHighlight(SelectedDialIndex);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow,
            FString::Printf(TEXT("Dial %d selected"), SelectedDialIndex + 1));
    }
}

void APadlockActor::RotateSelectedDial(int32 Direction)
{
    if (bAnimating || bSolved) return;

    int32& Val = DialValues[SelectedDialIndex];
    Val = (Val + Direction + 10) % 10; // wrap 0–9

    ApplyDialRotation(SelectedDialIndex);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan,
            FString::Printf(TEXT("Dial %d → %d  [%d %d %d %d]"),
                SelectedDialIndex + 1, Val,
                DialValues[0], DialValues[1], DialValues[2], DialValues[3]));
    }

    // Check combination
    if (CheckCombination())
    {
        UE_LOG(LogTemp, Log, TEXT("PadlockActor: Correct combination entered!"));
        // The character reference isn't available here — the character will call
        // StartUnlockAnimation from its padlock interaction code after this returns true.
        // We use a slight delay approach: set a flag and let Tick handle it.
        // Actually, simpler: just start it here. The character stores itself on the padlock.
        if (CallbackCharacter)
        {
            StartUnlockAnimation(CallbackCharacter);
        }
    }
}

bool APadlockActor::CheckCombination() const
{
    if (CorrectCombination.Num() < 4) return false;

    for (int32 i = 0; i < 4; ++i)
    {
        if (DialValues[i] != CorrectCombination[i])
            return false;
    }
    return true;
}

UStaticMeshComponent* APadlockActor::GetDialMesh(int32 Index) const
{
    switch (Index)
    {
    case 0: return PadlockRotatable1;
    case 1: return PadlockRotatable2;
    case 2: return PadlockRotatable3;
    case 3: return PadlockRotatable4;
    default: return nullptr;
    }
}

void APadlockActor::ApplyDialRotation(int32 Index)
{
    UStaticMeshComponent* Dial = GetDialMesh(Index);
    if (!Dial) return;

    FRotator Rot = Dial->GetRelativeRotation();
    // BaseDialOffset aligns the mesh's "0" mark; DialValues adds the player's input.
    // Change .Roll to .Pitch or .Yaw if your mesh axis differs.
    Rot.Roll = BaseDialOffset + (DialValues[Index] * DegreesPerStep);
    Dial->SetRelativeRotation(Rot);
}

/* ========== Dial highlight ========== */

void APadlockActor::CreateDialDynamicMaterials()
{
    DialDynMaterials.SetNum(4);

    for (int32 i = 0; i < 4; ++i)
    {
        UStaticMeshComponent* Dial = GetDialMesh(i);
        if (!Dial) continue;

        // Create dynamic instance from whatever material is on slot 0
        UMaterialInstanceDynamic* DynMat = Dial->CreateDynamicMaterialInstance(0);
        if (DynMat)
        {
            // Ensure emissive starts at 0
            DynMat->SetScalarParameterValue(EmissiveParamName, 0.f);
        }
        DialDynMaterials[i] = DynMat;
    }
}

void APadlockActor::UpdateDialHighlight(int32 ActiveIndex)
{
    for (int32 i = 0; i < 4; ++i)
    {
        if (!DialDynMaterials.IsValidIndex(i) || !DialDynMaterials[i]) continue;

        const float Value = (i == ActiveIndex) ? HighlightEmissiveStrength : 0.f;
        DialDynMaterials[i]->SetScalarParameterValue(EmissiveParamName, Value);
    }
}

void APadlockActor::ClearAllDialHighlights()
{
    for (int32 i = 0; i < 4; ++i)
    {
        if (!DialDynMaterials.IsValidIndex(i) || !DialDynMaterials[i]) continue;
        DialDynMaterials[i]->SetScalarParameterValue(EmissiveParamName, 0.f);
    }
}

/* ========== Unlock animation ========== */

void APadlockActor::StartUnlockAnimation(AHorrorGameCharacter* OwningCharacter)
{
    if (bAnimating || bSolved) return;

    bAnimating = true;
    bSolved = true;
    CallbackCharacter = OwningCharacter;

    // Phase 1 setup: shackle release
    AnimPhase = EPadlockAnimPhase::ShackleRelease;
    AnimElapsed = 0.f;

    if (PadlockRight)
    {
        ShackleStartLoc = PadlockRight->GetRelativeLocation();
        ShackleTargetLoc = ShackleStartLoc;
        ShackleTargetLoc.Y += ShackleSlideDistance;
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("Padlock unlocking..."));
    }
}

void APadlockActor::FinishUnlockAnimation()
{
    bAnimating = false;
    AnimPhase = EPadlockAnimPhase::None;

    // Unblock the linked door
    if (LinkedDoor)
    {
        LinkedDoor->bBlockedByExternalLock = false;
    }

    // Hide arrow/full widgets permanently
    if (ArrowWidget) ArrowWidget->SetVisibility(false);
    if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false);

    // Clear dial highlights
    bInInteractionMode = false;
    ClearAllDialHighlights();

    // End the interaction on the character side
    if (CallbackCharacter)
    {
        CallbackCharacter->EndPadlockInteraction();
    }

    CallbackCharacter = nullptr;

    UE_LOG(LogTemp, Log, TEXT("PadlockActor: Unlock complete. Door unblocked."));
}