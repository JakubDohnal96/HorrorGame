// Source/HorrorGame/Private/InteractableActor.cpp
#include "InteractableActor.h"
#include "InteractableManager.h"
#include "Components/WidgetComponent.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"

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