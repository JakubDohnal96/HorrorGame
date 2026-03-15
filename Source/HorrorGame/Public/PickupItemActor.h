// PickupItemActor.h
#pragma once

#include "CoreMinimal.h"
#include "InteractableActor.h"
#include "InventoryComponent.h"
#include "PickupItemActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UWidgetComponent;
class UCameraComponent;

/**
 * A world-placed item that the player can inspect (camera blend) and pick up
 * into their inventory.  Inherits the standard arrow / full-interaction widget
 * pipeline from AInteractableActor.
 */
UCLASS()
class HORRORGAME_API APickupItemActor : public AInteractableActor
{
    GENERATED_BODY()

public:
    APickupItemActor();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /* ========== Components ========== */
public:
    UPROPERTY(VisibleAnywhere, Category = "Pickup|Components")
    USceneComponent* Root;

    /** The visible mesh of the item sitting in the world. */
    UPROPERTY(VisibleAnywhere, Category = "Pickup|Components")
    UStaticMeshComponent* ItemMesh;

    /* ========== Interactable API overrides ========== */

    virtual bool CanShowInteraction(APawn* Player) const override;
    virtual bool CanShowFullInteraction(APawn* Player) const override;
    virtual void SetFullWidgetVisible(bool bVisible, APawn* Player) override;
    virtual FVector GetInteractionLocation() const override;

    /** Deactivate the interaction camera (called when ending interaction). */
    void DeactivateInteractionCamera();

    /* ========== Item data (what goes into the inventory) ========== */

    /** Display name shown in inventory UI. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Item")
    FText ItemDisplayName = FText::FromString(TEXT("Item"));

    /**
     * Unique item-type identifier.
     * Works the same way as KeyIndex on doors:
     *   Key 0 opens doors with symbol 0, screwdriver (5) opens vent panels (5), etc.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Item")
    int32 ItemTypeIndex = -1;

    /**
     * The mesh that will be shown in the inventory thumbnail / key-insert animation.
     * If left empty, ItemMesh's static mesh is used as fallback.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Item")
    UStaticMesh* InventoryMesh = nullptr;

    /** Build an FInventoryItem from this actor's configured data. */
    FInventoryItem MakeInventoryItem() const;

    /* ========== Interaction distances ========== */

    /** Distance at which the arrow widget appears. */
    UPROPERTY(EditAnywhere, Category = "Pickup|Interaction")
    float InteractionMaxDistance = 250.f;

    /** Distance at which the full "press E" widget appears and interaction is allowed. */
    UPROPERTY(EditAnywhere, Category = "Pickup|Interaction")
    float InteractionUseDistance = 100.f;
};