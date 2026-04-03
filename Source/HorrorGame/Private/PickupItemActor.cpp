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

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
    ItemMesh->SetupAttachment(Root);
    ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ItemMesh->SetCollisionResponseToAllChannels(ECR_Block);

    if (InteractionBox) { InteractionBox->SetupAttachment(Root); InteractionBox->SetBoxExtent(FVector(30.f,30.f,30.f)); InteractionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision); InteractionBox->SetGenerateOverlapEvents(false); }
    if (ArrowWidget) { ArrowWidget->SetupAttachment(InteractionBox); ArrowWidget->SetWidgetSpace(EWidgetSpace::World); ArrowWidget->SetDrawAtDesiredSize(true); ArrowWidget->SetVisibility(false); ArrowWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision); ArrowWidget->SetBlendMode(EWidgetBlendMode::Transparent); ArrowWidget->SetRenderCustomDepth(true); }
    if (FullInteractionWidget) { FullInteractionWidget->SetupAttachment(InteractionBox); FullInteractionWidget->SetWidgetSpace(EWidgetSpace::World); FullInteractionWidget->SetDrawAtDesiredSize(true); FullInteractionWidget->SetVisibility(false); FullInteractionWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision); FullInteractionWidget->SetBlendMode(EWidgetBlendMode::Transparent); FullInteractionWidget->SetRenderCustomDepth(true); }
    if (InteractionCamera) { InteractionCamera->SetupAttachment(Root); InteractionCamera->bAutoActivate = false; }
}

void APickupItemActor::BeginPlay() { Super::BeginPlay(); }

void APickupItemActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player) return;

    const bool bArrowShouldShow = CanShowInteraction(Player) && !CanShowFullInteraction(Player);
    if (ArrowWidget) ArrowWidget->SetVisibility(bArrowShouldShow);

    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC || !PC->PlayerCameraManager) return;
    FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
    if (ArrowWidget && ArrowWidget->IsVisible()) FInteractableUtils::FaceWidgetTowardsCamera(ArrowWidget, CameraLocation);
    if (FullInteractionWidget && FullInteractionWidget->IsVisible()) FInteractableUtils::FaceWidgetTowardsCamera(FullInteractionWidget, CameraLocation);
}

bool APickupItemActor::CanShowInteraction(APawn* Player) const
{
    if (!Player || !InteractionBox) return false;
    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    if (Dist > InteractionMaxDistance) return false;
    return IsInteractionVisibleToPlayer(Player);
}

bool APickupItemActor::CanShowFullInteraction(APawn* Player) const
{
    if (!Player || !InteractionBox) return false;
    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    if (Dist > InteractionUseDistance) return false;
    return IsInteractionVisibleToPlayer(Player);
}

void APickupItemActor::SetFullWidgetVisible(bool bVisible, APawn*) { if (FullInteractionWidget) FullInteractionWidget->SetVisibility(bVisible); }

FVector APickupItemActor::GetInteractionLocation() const { return InteractionBox ? InteractionBox->GetComponentLocation() : GetActorLocation(); }
void APickupItemActor::DeactivateInteractionCamera() { if (InteractionCamera) InteractionCamera->Deactivate(); }

FInventoryItem APickupItemActor::MakeInventoryItem() const
{
    UStaticMesh* MeshToUse = InventoryMesh;
    if (!MeshToUse && ItemMesh) MeshToUse = ItemMesh->GetStaticMesh();
    return FInventoryItem(ItemDisplayName, ItemTypeIndex, MeshToUse);
}