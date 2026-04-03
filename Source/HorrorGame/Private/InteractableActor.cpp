// Source/HorrorGame/Private/InteractableActor.cpp
#include "InteractableActor.h"
#include "InteractableManager.h"
#include "Components/WidgetComponent.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

AInteractableActor::AInteractableActor()
{
    PrimaryActorTick.bCanEverTick = false; // default: no tick. Derived classes can enable if needed.

    // Create default components (derived classes may replace or reconfigure)
    ArrowWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("ArrowWidget"));
    ArrowWidget->SetupAttachment(RootComponent);
    ArrowWidget->SetVisibility(false);

    FullInteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("FullInteractionWidget"));
    FullInteractionWidget->SetupAttachment(RootComponent);
    FullInteractionWidget->SetVisibility(false);

    InteractionCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InteractionCamera"));
    InteractionCamera->SetupAttachment(RootComponent);
    InteractionCamera->SetActive(false);

    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    InteractionBox->SetupAttachment(RootComponent);
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AInteractableActor::BeginPlay()
{
    Super::BeginPlay();
    RegisterSelf();
}

void AInteractableActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnregisterSelf();
    Super::EndPlay(EndPlayReason);
}

void AInteractableActor::RegisterSelf()
{
    if (UInteractableManager* Mgr = UInteractableManager::Get(GetGameInstance()))
    {
        Mgr->RegisterInteractable(this);
    }
}

void AInteractableActor::UnregisterSelf()
{
    if (UInteractableManager* Mgr = UInteractableManager::Get(GetGameInstance()))
    {
        Mgr->UnregisterInteractable(this);
    }
}

bool AInteractableActor::CanShowInteraction(APawn* /*Player*/) const
{
    // default: show arrow if actor is valid; derived classes should override
    return true;
}

bool AInteractableActor::CanShowFullInteraction(APawn* /*Player*/) const
{
    // default: don't show full widget; derived classes override
    return false;
}

void AInteractableActor::SetFullWidgetVisible(bool bVisible, APawn* /*Player*/)
{
    if (FullInteractionWidget)
    {
        FullInteractionWidget->SetVisibility(bVisible);
    }

    // If showing full widget hide arrow
    if (ArrowWidget)
    {
        ArrowWidget->SetVisibility(!bVisible && ArrowWidget->IsVisible());
    }
}

UBoxComponent* AInteractableActor::GetActiveInteractionBox(APawn* /*Player*/) const
{
    return InteractionBox;
}

FVector AInteractableActor::GetInteractionLocation() const
{
    if (InteractionBox)
    {
        return InteractionBox->GetComponentLocation();
    }
    return GetActorLocation();
}

void AInteractableActor::UpdateArrowVisibility(APawn* Player)
{
    // Default behavior: arrow visible when CanShowInteraction() and not CanShowFullInteraction()
    const bool bArrow = CanShowInteraction(Player) && !CanShowFullInteraction(Player);
    if (ArrowWidget)
    {
        ArrowWidget->SetVisibility(bArrow);
    }
}

UCameraComponent* AInteractableActor::GetInteractionCamera(const APawn* Player) const
{
    return InteractionCamera;
}

bool AInteractableActor::IsInteractionVisibleToPlayer(APawn* Player) const
{
    if (!Player || !InteractionBox) return false;

    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC || !PC->PlayerCameraManager) return false;

    FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
    FVector CameraForward = PC->PlayerCameraManager->GetCameraRotation().Vector();
    FVector ToBox = (InteractionBox->GetComponentLocation() - CameraLocation).GetSafeNormal();

    // Facing check: is the player roughly looking toward the interaction box?
    // 0.3 ≈ ~72° half-angle — generous enough to not feel restrictive
    if (FVector::DotProduct(CameraForward, ToBox) < 0.3f)
        return false;

    // Line trace: is there a wall between camera and interaction box?
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Player);
    Params.AddIgnoredActor(this);

    if (GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, InteractionBox->GetComponentLocation(), ECC_Visibility, Params))
        return false;

    return true;
}