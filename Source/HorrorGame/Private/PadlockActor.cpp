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

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    StaticLockFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticLockFrame"));
    StaticLockFrame->SetupAttachment(Root);
    StaticLockFrame->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RotatableLockFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RotatableLockFrame"));
    RotatableLockFrame->SetupAttachment(Root);
    RotatableLockFrame->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    StaticLockDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticLockDoor"));
    StaticLockDoor->SetupAttachment(Root);
    StaticLockDoor->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PadlockU = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockU"));
    PadlockU->SetupAttachment(Root);
    PadlockU->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    SwingPivot = CreateDefaultSubobject<USceneComponent>(TEXT("SwingPivot"));
    SwingPivot->SetupAttachment(Root);

    PadlockLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockLeft"));
    PadlockLeft->SetupAttachment(SwingPivot); PadlockLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PadlockRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockRight"));
    PadlockRight->SetupAttachment(SwingPivot); PadlockRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PadlockRotatable1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockRotatable1"));
    PadlockRotatable1->SetupAttachment(SwingPivot); PadlockRotatable1->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PadlockRotatable2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockRotatable2"));
    PadlockRotatable2->SetupAttachment(SwingPivot); PadlockRotatable2->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PadlockRotatable3 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockRotatable3"));
    PadlockRotatable3->SetupAttachment(SwingPivot); PadlockRotatable3->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PadlockRotatable4 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockRotatable4"));
    PadlockRotatable4->SetupAttachment(SwingPivot); PadlockRotatable4->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PadlockSeparation = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockSeparation"));
    PadlockSeparation->SetupAttachment(SwingPivot); PadlockSeparation->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PadlockBeams = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadlockBeams"));
    PadlockBeams->SetupAttachment(SwingPivot); PadlockBeams->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (InteractionBox) { InteractionBox->SetupAttachment(Root); InteractionBox->SetBoxExtent(FVector(30.f,30.f,30.f)); InteractionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision); InteractionBox->SetGenerateOverlapEvents(false); }
    if (ArrowWidget) { ArrowWidget->SetupAttachment(InteractionBox); ArrowWidget->SetWidgetSpace(EWidgetSpace::World); ArrowWidget->SetDrawAtDesiredSize(true); ArrowWidget->SetVisibility(false); ArrowWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision); ArrowWidget->SetBlendMode(EWidgetBlendMode::Transparent); ArrowWidget->SetRenderCustomDepth(true); }
    if (FullInteractionWidget) { FullInteractionWidget->SetupAttachment(InteractionBox); FullInteractionWidget->SetWidgetSpace(EWidgetSpace::World); FullInteractionWidget->SetDrawAtDesiredSize(true); FullInteractionWidget->SetVisibility(false); FullInteractionWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision); FullInteractionWidget->SetBlendMode(EWidgetBlendMode::Transparent); FullInteractionWidget->SetRenderCustomDepth(true); }
    if (InteractionCamera) { InteractionCamera->SetupAttachment(Root); InteractionCamera->bAutoActivate = false; }

    CorrectCombination = {1, 9, 4, 7};
    for (int32 i = 0; i < 4; ++i) DialValues[i] = 0;

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
    if (LinkedDoor)
    {
        LinkedDoor->bBlockedByExternalLock = true;
        if (StaticLockDoor && LinkedDoor->HingeAxis)
        {
            StaticLockDoor->SetMobility(EComponentMobility::Movable);
            StaticLockDoor->AttachToComponent(LinkedDoor->HingeAxis, FAttachmentTransformRules::KeepWorldTransform);
        }
    }
    if (SwingPivot && PadlockLeft)
    {
        TArray<USceneComponent*> SwingChildren;
        SwingPivot->GetChildrenComponents(false, SwingChildren);
        TArray<FTransform> ChildWorldTransforms;
        for (USceneComponent* Child : SwingChildren) ChildWorldTransforms.Add(Child->GetComponentTransform());
        SwingPivot->SetWorldLocation(PadlockLeft->GetComponentLocation());
        for (int32 i = 0; i < SwingChildren.Num(); ++i) SwingChildren[i]->SetWorldTransform(ChildWorldTransforms[i]);
    }
    for (int32 i = 0; i < 4; ++i) { DialValues[i] = 0; ApplyDialRotation(i); }
    CreateDialDynamicMaterials();
}

void APadlockActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bAnimating && AnimPhase != EPadlockAnimPhase::None)
    {
        AnimElapsed += DeltaTime;
        switch (AnimPhase)
        {
        case EPadlockAnimPhase::ShackleRelease:
        {
            const float Alpha = FMath::Clamp(AnimElapsed / ShackleSlideDuration, 0.f, 1.f);
            const float Smooth = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);
            if (PadlockRight) PadlockRight->SetRelativeLocation(FMath::Lerp(ShackleStartLoc, ShackleTargetLoc, Smooth));
            if (Alpha >= 1.f) { AnimPhase = EPadlockAnimPhase::BodySwing; AnimElapsed = 0.f; SwingStartRot = SwingPivot->GetRelativeRotation(); SwingTargetRot = SwingStartRot; SwingTargetRot.Roll += BodySwingDegrees; }
            break;
        }
        case EPadlockAnimPhase::BodySwing:
        {
            const float Alpha = FMath::Clamp(AnimElapsed / BodySwingDuration, 0.f, 1.f);
            const float Smooth = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);
            if (SwingPivot) SwingPivot->SetRelativeRotation(FMath::Lerp(SwingStartRot, SwingTargetRot, Smooth));
            if (Alpha >= 1.f) { if (PadlockU) PadlockU->SetVisibility(false); if (SwingPivot) SwingPivot->SetVisibility(false, true); AnimPhase = EPadlockAnimPhase::HideAndRotate; AnimElapsed = 0.f; FrameStartRot = RotatableLockFrame->GetRelativeRotation(); FrameTargetRot = FrameStartRot; FrameTargetRot.Yaw += FrameRotateDegrees; }
            break;
        }
        case EPadlockAnimPhase::HideAndRotate:
        {
            const float Alpha = FMath::Clamp(AnimElapsed / FrameRotateDuration, 0.f, 1.f);
            const float Smooth = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);
            if (RotatableLockFrame) RotatableLockFrame->SetRelativeRotation(FMath::Lerp(FrameStartRot, FrameTargetRot, Smooth));
            if (Alpha >= 1.f) { AnimPhase = EPadlockAnimPhase::Complete; FinishUnlockAnimation(); }
            break;
        }
        default: break;
        }
        return;
    }

    if (bSolved) { if (ArrowWidget) ArrowWidget->SetVisibility(false); if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false); return; }
    if (bInInteractionMode) { if (ArrowWidget) ArrowWidget->SetVisibility(false); if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false); return; }

    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player) { if (ArrowWidget) ArrowWidget->SetVisibility(false); if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false); return; }

    const bool bCanInteract = CanShowInteraction(Player);
    const bool bCanFull = CanShowFullInteraction(Player);
    if (ArrowWidget) ArrowWidget->SetVisibility(bCanInteract && !bCanFull);
    if (FullInteractionWidget) FullInteractionWidget->SetVisibility(bCanFull);

    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC || !PC->PlayerCameraManager) return;
    FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
    if (ArrowWidget && ArrowWidget->IsVisible()) FInteractableUtils::FaceWidgetTowardsCamera(ArrowWidget, CameraLocation);
    if (FullInteractionWidget && FullInteractionWidget->IsVisible()) FInteractableUtils::FaceWidgetTowardsCamera(FullInteractionWidget, CameraLocation);
}

bool APadlockActor::CanShowInteraction(APawn* Player) const
{
    if (!Player || !InteractionBox) return false;
    if (bSolved || bAnimating) return false;
    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    if (Dist > InteractionMaxDistance) return false;
    return IsInteractionVisibleToPlayer(Player);
}

bool APadlockActor::CanShowFullInteraction(APawn* Player) const
{
    if (!Player || !InteractionBox) return false;
    if (bSolved || bAnimating) return false;
    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    if (Dist > InteractionUseDistance) return false;
    return IsInteractionVisibleToPlayer(Player);
}

void APadlockActor::SetFullWidgetVisible(bool bVisible, APawn*) { if (FullInteractionWidget) FullInteractionWidget->SetVisibility(bVisible); }
FVector APadlockActor::GetInteractionLocation() const { return InteractionBox ? InteractionBox->GetComponentLocation() : GetActorLocation(); }
void APadlockActor::DeactivateInteractionCamera() { if (InteractionCamera) InteractionCamera->Deactivate(); }

void APadlockActor::SelectNextDial()
{
    if (bAnimating) return;
    SelectedDialIndex = (SelectedDialIndex + 1) % 4;
    if (bInInteractionMode) UpdateDialHighlight(SelectedDialIndex);
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, FString::Printf(TEXT("Dial %d selected"), SelectedDialIndex + 1));
}

void APadlockActor::SelectPreviousDial()
{
    if (bAnimating) return;
    SelectedDialIndex = (SelectedDialIndex - 1 + 4) % 4;
    if (bInInteractionMode) UpdateDialHighlight(SelectedDialIndex);
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, FString::Printf(TEXT("Dial %d selected"), SelectedDialIndex + 1));
}

void APadlockActor::RotateSelectedDial(int32 Direction)
{
    if (bAnimating || bSolved) return;
    int32& Val = DialValues[SelectedDialIndex];
    Val = (Val + Direction + 10) % 10;
    ApplyDialRotation(SelectedDialIndex);
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan, FString::Printf(TEXT("Dial %d -> %d  [%d %d %d %d]"), SelectedDialIndex+1, Val, DialValues[0], DialValues[1], DialValues[2], DialValues[3]));
    if (CheckCombination() && CallbackCharacter) StartUnlockAnimation(CallbackCharacter);
}

bool APadlockActor::CheckCombination() const
{
    if (CorrectCombination.Num() < 4) return false;
    for (int32 i = 0; i < 4; ++i) { if (DialValues[i] != CorrectCombination[i]) return false; }
    return true;
}

UStaticMeshComponent* APadlockActor::GetDialMesh(int32 Index) const
{
    switch (Index) { case 0: return PadlockRotatable1; case 1: return PadlockRotatable2; case 2: return PadlockRotatable3; case 3: return PadlockRotatable4; default: return nullptr; }
}

void APadlockActor::ApplyDialRotation(int32 Index)
{
    UStaticMeshComponent* Dial = GetDialMesh(Index); if (!Dial) return;
    FRotator Rot = Dial->GetRelativeRotation();
    Rot.Roll = BaseDialOffset + (DialValues[Index] * DegreesPerStep);
    Dial->SetRelativeRotation(Rot);
}

void APadlockActor::CreateDialDynamicMaterials()
{
    DialDynMaterials.SetNum(4);
    for (int32 i = 0; i < 4; ++i) { UStaticMeshComponent* Dial = GetDialMesh(i); if (!Dial) continue; UMaterialInstanceDynamic* DynMat = Dial->CreateDynamicMaterialInstance(0); if (DynMat) DynMat->SetScalarParameterValue(EmissiveParamName, 0.f); DialDynMaterials[i] = DynMat; }
}

void APadlockActor::UpdateDialHighlight(int32 ActiveIndex)
{
    for (int32 i = 0; i < 4; ++i) { if (!DialDynMaterials.IsValidIndex(i) || !DialDynMaterials[i]) continue; DialDynMaterials[i]->SetScalarParameterValue(EmissiveParamName, (i == ActiveIndex) ? HighlightEmissiveStrength : 0.f); }
}

void APadlockActor::ClearAllDialHighlights()
{
    for (int32 i = 0; i < 4; ++i) { if (!DialDynMaterials.IsValidIndex(i) || !DialDynMaterials[i]) continue; DialDynMaterials[i]->SetScalarParameterValue(EmissiveParamName, 0.f); }
}

void APadlockActor::StartUnlockAnimation(AHorrorGameCharacter* OwningCharacter)
{
    if (bAnimating || bSolved) return;
    bAnimating = true; bSolved = true; CallbackCharacter = OwningCharacter;
    AnimPhase = EPadlockAnimPhase::ShackleRelease; AnimElapsed = 0.f;
    if (PadlockRight) { ShackleStartLoc = PadlockRight->GetRelativeLocation(); ShackleTargetLoc = ShackleStartLoc; ShackleTargetLoc.Y += ShackleSlideDistance; }
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("Padlock unlocking..."));
}

void APadlockActor::FinishUnlockAnimation()
{
    bAnimating = false; AnimPhase = EPadlockAnimPhase::None;
    if (LinkedDoor) LinkedDoor->bBlockedByExternalLock = false;
    if (ArrowWidget) ArrowWidget->SetVisibility(false);
    if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false);
    bInInteractionMode = false; ClearAllDialHighlights();
    if (CallbackCharacter) CallbackCharacter->EndPadlockInteraction();
    CallbackCharacter = nullptr;
}