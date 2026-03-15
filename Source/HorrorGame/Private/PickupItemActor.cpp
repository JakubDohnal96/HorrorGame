// PickupItemActor.cpp
#include "PickupItemActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "InteractableUtils.h"

APickupItemActor::APickupItemActor()
{
    PrimaryActorTick.bCanEverTick = true;

    /* --- Root --- */
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    /* --- Visible item mesh --- */
    ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
    ItemMesh->SetupAttachment(Root);
    ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ItemMesh->SetCollisionResponseToAllChannels(ECR_Block);

    /* --- Re-parent inherited components from AInteractableActor --- */
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
}

void APickupItemActor::BeginPlay()
{
    Super::BeginPlay();
}

void APickupItemActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player) return;

    /* --- Arrow visibility --- */
    const bool bArrowShouldShow = CanShowInteraction(Player) && !CanShowFullInteraction(Player);
    if (ArrowWidget) ArrowWidget->SetVisibility(bArrowShouldShow);

    /* --- Rotate widgets toward camera --- */
    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC || !PC->PlayerCameraManager) return;

    FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();

    if (ArrowWidget && ArrowWidget->IsVisible())
        FInteractableUtils::FaceWidgetTowardsCamera(ArrowWidget, CameraLocation);
    if (FullInteractionWidget && FullInteractionWidget->IsVisible())
        FInteractableUtils::FaceWidgetTowardsCamera(FullInteractionWidget, CameraLocation);
}

/* ========== Interactable API ========== */

bool APickupItemActor::CanShowInteraction(APawn* Player) const
{
    if (!Player || !InteractionBox) return false;
    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    return Dist <= InteractionMaxDistance;
}

bool APickupItemActor::CanShowFullInteraction(APawn* Player) const
{
    if (!Player || !InteractionBox) return false;

    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    if (Dist > InteractionUseDistance) return false;

    // On-screen check
    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC) return true;

    FVector2D ScreenPos;
    const bool bProjected = PC->ProjectWorldLocationToScreen(
        InteractionBox->GetComponentLocation(), ScreenPos, true);
    if (!bProjected) return false;

    int32 SizeX = 0, SizeY = 0;
    PC->GetViewportSize(SizeX, SizeY);
    return ScreenPos.X >= 0 && ScreenPos.X <= SizeX
        && ScreenPos.Y >= 0 && ScreenPos.Y <= SizeY;
}

void APickupItemActor::SetFullWidgetVisible(bool bVisible, APawn* /*Player*/)
{
    if (FullInteractionWidget)
        FullInteractionWidget->SetVisibility(bVisible);
}

FVector APickupItemActor::GetInteractionLocation() const
{
    return InteractionBox
        ? InteractionBox->GetComponentLocation()
        : GetActorLocation();
}

void APickupItemActor::DeactivateInteractionCamera()
{
    if (InteractionCamera) InteractionCamera->Deactivate();
}

/* ========== Inventory helper ========== */

FInventoryItem APickupItemActor::MakeInventoryItem() const
{
    // Use explicit InventoryMesh if set, otherwise grab the static mesh from the world mesh
    UStaticMesh* MeshToUse = InventoryMesh;
    if (!MeshToUse && ItemMesh)
    {
        MeshToUse = ItemMesh->GetStaticMesh();
    }

    return FInventoryItem(ItemDisplayName, ItemTypeIndex, MeshToUse);
}