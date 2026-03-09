// DoorActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "DoorActor.generated.h"

class UCameraComponent;

UENUM(BlueprintType)
enum class EDoorSymbol : uint8
{
    Symbol_A UMETA(DisplayName="Symbol_A"),
    Symbol_B UMETA(DisplayName="Symbol_B"),
    Symbol_C UMETA(DisplayName="Symbol_C"),
    Symbol_D UMETA(DisplayName="Symbol_D"),
    // keep this in sync with the number of meshes you plan to assign
};

UCLASS()
class HORRORGAME_API ADoorActor : public AActor
{
	GENERATED_BODY()

public:
	ADoorActor();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

public:
	/* ===== Interaction ===== */

	UFUNCTION(BlueprintCallable, Category="Door")
	void ToggleDoor();

	// distance limit for the arrow (longer range)
	float GetInteractionMaxDistance() const { return InteractionMaxDistance; }

	// distance limit for the full interaction (shorter range)
	float GetInteractionUseDistance() const { return InteractionUseDistance; }

	// Character / external code uses this to decide whether the arrow should show
	bool CanShowInteraction(APawn* Player) const;

	// Character / external code uses this to decide whether the *full* widget is eligible on this door
	bool CanShowFullInteraction(APawn* Player) const;

	// Called by external code (character) each frame to toggle which door shows its full widget
	void SetFullWidgetVisible(bool bVisible, const APawn* Player);

protected:
	/* ===== Components ===== */

	UPROPERTY(VisibleAnywhere, Category="Door|Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, Category="Door|Components")
	USceneComponent* HingeAxis;

	UPROPERTY(VisibleAnywhere, Category="Door|Components")
	UStaticMeshComponent* DoorLeaf;

	UPROPERTY(VisibleAnywhere, Category="Door|Components")
	UStaticMeshComponent* Handle;

	UPROPERTY(VisibleAnywhere, Category="Door|Components")
	UStaticMeshComponent* Hinge;

	UPROPERTY(VisibleAnywhere, Category="Door|Components")
	UStaticMeshComponent* Backplate;

	// FRONT SIDE
	UPROPERTY(VisibleAnywhere, Category="Door|Interaction")
	UBoxComponent* InteractionBox_Front;

	// arrow-only widget (long-range indicator) — front side
	UPROPERTY(VisibleAnywhere, Category="Door|Interaction")
	UWidgetComponent* ArrowWidget_Front;

	// full widget (arrow + button) — front side
	UPROPERTY(VisibleAnywhere, Category="Door|Interaction")
	UWidgetComponent* InteractionWidget_Front;

	// BACK SIDE
	UPROPERTY(VisibleAnywhere, Category="Door|Interaction")
	UBoxComponent* InteractionBox_Back;

	// arrow-only widget (long-range indicator) — back side
	UPROPERTY(VisibleAnywhere, Category="Door|Interaction")
	UWidgetComponent* ArrowWidget_Back;

	// full widget (arrow + button) — back side
	UPROPERTY(VisibleAnywhere, Category="Door|Interaction")
	UWidgetComponent* InteractionWidget_Back;

	/* ===== Door State ===== */

	bool bIsOpen;
	bool bIsMoving;
	bool bShouldBlockDuringThisMove;
	bool bBlockedByPlayer;

	// Determines if door motion is towards the player (unchanged)
	bool IsMovingTowardsPlayer(const AActor* Player) const;

	float CurrentYaw;

	/* ===== Settings ===== */

	UPROPERTY(EditAnywhere, Category="Door|Settings")
	float OpenAngle;

	UPROPERTY(EditAnywhere, Category="Door|Settings")
	float OpenSpeed;

	// how far the arrow indicator can appear
	UPROPERTY(EditAnywhere, Category="Door|Settings")
	float InteractionMaxDistance;

	// how close the player must be for the full interaction (button) to appear
	UPROPERTY(EditAnywhere, Category="Door|Settings", meta=(ClampMin="0.0"))
	float InteractionUseDistance;

	UPROPERTY(EditAnywhere, Category="Door|Settings")
	float AutoCloseDistance;

	UPROPERTY(EditAnywhere, Category="Door|Settings")
	float AutoCloseDelay;

	float LastTimePlayerNearby;

	// How long player must be clear before door can continue (seconds)
	UPROPERTY(EditAnywhere, Category="Door|Settings", meta=(ClampMin="0.0"))
	float UnblockDelay;

	/* ===== Overlap Callbacks ===== */

	UFUNCTION()
	void OnDoorOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnDoorOverlapEnd(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, Category="Door|Components")
	UBoxComponent* BlockSensor;

	bool bWaitingForUnblock;
	float BlockedTime;

	/* ===== Helpers for active side (distance-based) ===== */

    /* ===== Symbol (new) ===== */

    // Static mesh component that will show the door symbol (subobject)
    UPROPERTY(VisibleAnywhere, Category="Door|Components")
    UStaticMeshComponent* SymbolMeshComponent;

    // Designer-assignable list of candidate meshes (assign 4 in editor)
    UPROPERTY(EditAnywhere, Category="Door|Appearance", meta=(ToolTip="Assign the 4 possible symbol meshes here (index 0...3)"))
    TArray<UStaticMesh*> SymbolMeshes;

    // Which symbol is selected for THIS door instance (index into SymbolMeshes).
    // You can expose this as an enum instead if you prefer: EDoorSymbol DefaultSymbol;
    UPROPERTY(EditAnywhere, Category="Door|Appearance", meta=(ClampMin="0"))
    int32 SelectedSymbolIndex;

    // Current runtime index (stored so you can query later)
    int32 CurrentSymbolIndex;

public:
	// Return the closest interaction box to the player (front or back)
	UBoxComponent* GetActiveInteractionBox(const APawn* Player) const;

	// Return the widget component corresponding to the active box (full widget)
	UWidgetComponent* GetActiveInteractionWidget(const APawn* Player) const;

	// Helper: project a given InteractionBox center to screen
	bool IsInteractionBoxOnScreen(APawn* Player, UBoxComponent* Box) const;

	// Set symbol by index (0..SymbolMeshes.Num()-1). Returns true if successful.
    UFUNCTION(BlueprintCallable, Category="Door|Appearance")
    bool SetSymbolByIndex(int32 Index);

    // Convenience: get current symbol index and mesh
    UFUNCTION(BlueprintCallable, Category="Door|Appearance")
    int32 GetCurrentSymbolIndex() const { return CurrentSymbolIndex; }

    UFUNCTION(BlueprintCallable, Category="Door|Appearance")
    UStaticMesh* GetCurrentSymbolMesh() const;

	// camera components for each side (position in BP)
	UPROPERTY(VisibleAnywhere, Category="Door|Interaction")
	UCameraComponent* InteractionCamera_Front;

	UPROPERTY(VisibleAnywhere, Category="Door|Interaction")
	UCameraComponent* InteractionCamera_Back;

	// helper to get the correct camera for a given player pawn
	UCameraComponent* GetActiveInteractionCamera(const APawn* Player) const;

	// helper to deactivate both cameras (safe cleanup)
	void DeactivateInteractionCameras();

	// ===== Locking =====
	UPROPERTY(EditAnywhere, Category="Door|Lock")
	bool bStartsLocked = true;      // settable in editor

	// runtime lock state
	bool bLocked = false;

	// API to unlock
	UFUNCTION(BlueprintCallable, Category="Door|Lock")
	void UnlockDoor();

	// Check state
	UFUNCTION(BlueprintCallable, Category="Door|Lock")
	bool IsLocked() const { return bLocked; }

	UPROPERTY(VisibleAnywhere, Category="Door|Interaction")
	USceneComponent* KeyInsertPoint;

	UFUNCTION(BlueprintCallable)
	FTransform GetKeyInsertTransform() const { return KeyInsertPoint ? KeyInsertPoint->GetComponentTransform() : GetActorTransform(); }
};