// HorrorGameCharacter.h
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "InputActionValue.h"
#include "HorrorGameCharacter.generated.h"

class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UUserWidget;
class UInventoryComponent;
class UInventoryWidgetBase;
class UInputMappingContext;
class APCTerminalActor;
class USphereComponent;
class APickupItemActor;
class APadlockActor;
class ABlackboardPuzzleActor;   // ← NEW

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);


UCLASS(config=Game)
class HORRORGAME_API AHorrorGameCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHorrorGameCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/* ===== Components ===== */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Mesh)
	USkeletalMeshComponent* Mesh1P;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	UCameraComponent* FirstPersonCameraComponent;

	/* ===== Input Actions ===== */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	UInputAction* InteractAction;

	/* ===== Interaction ===== */

	UPROPERTY(EditAnywhere, Category="Interaction")
	float InteractionTraceDistance;

	UUserWidget* InteractWidgetInstance;

	/* ===== Input functions ===== */

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Interact();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Inventory
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
    UInventoryComponent* InventoryComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
    TSubclassOf<UUserWidget> InventoryWidgetClass;

	UPROPERTY()
	UInventoryWidgetBase* InventoryWidgetInstance;

    bool bInventoryOpen = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    UInputAction* InventoryToggleAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    UInputAction* InventoryNavUpAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    UInputAction* InventoryNavDownAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    UInputAction* InventoryNavLeftAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    UInputAction* InventoryNavRightAction;

	UFUNCTION()
	void ToggleInventory();

	UFUNCTION()
	void OnInventoryChanged();

	void OnNavUp();
	void OnNavDown();
	void OnNavLeft();
	void OnNavRight();

	UPROPERTY(EditAnywhere, Category="Input")
	class UInputMappingContext* IMC_Gameplay;

	UPROPERTY(EditAnywhere, Category="Input")
	class UInputMappingContext* IMC_InventoryUI;

	UPROPERTY(EditAnywhere, Category="Input")
	class UInputMappingContext* IMC_Interaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* InventoryUseAction;

	UPROPERTY()
	class ADoorActor* CurrentDoorUnlockTarget = nullptr;

	bool bDoorUnlockAnimPlaying = false;

	void UseSelectedItem();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* InventoryCancelAction;

	void CancelDoorUnlock();

	// ===== PC Terminal =====
	UPROPERTY(EditAnywhere, Category="Input") 
	class UInputMappingContext* IMC_PCInteraction;

	UPROPERTY(EditAnywhere, Category="Input") 
	UInputAction* PCInteractAction;

	UPROPERTY()
	class APCTerminalActor* CurrentPCTerminalTarget = nullptr;

	void BeginPCInteraction(APCTerminalActor* PCActor);
	void EndPCInteraction(bool bSuccess = false);

	UFUNCTION()
	void StartTerminalChat();

	FTimerHandle TerminalChatTimerHandle;

	// ===== Pickup Item Interaction =====

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputMappingContext* IMC_InteractionItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* PickupConfirmAction;

	UPROPERTY()
	APickupItemActor* CurrentPickupItemTarget = nullptr;

	void BeginItemInteraction(APickupItemActor* ItemActor);
	void EndItemInteraction(bool bPickedUp = false);
	void ConfirmPickupItem();

	// ===== Padlock Interaction =====

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputMappingContext* IMC_PadlockInteraction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* PadlockDialPrevAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* PadlockDialNextAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* PadlockRotateUpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* PadlockRotateDownAction;

	UPROPERTY()
	APadlockActor* CurrentPadlockTarget = nullptr;

	void BeginPadlockInteraction(APadlockActor* Padlock);
	void OnPadlockDialPrev();
	void OnPadlockDialNext();
	void OnPadlockRotateUp();
	void OnPadlockRotateDown();

public:
	void EndPadlockInteraction();

	// ==========================================================
	// ===== NEW: Blackboard Puzzle Interaction =================
	// ==========================================================

	/**
	 * IMC active while the puzzle grid is being manipulated
	 * (WASD navigate, E interact, R rotate, Q cancel).
	 */
	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputMappingContext* IMC_BlackboardPuzzle;

	/** Navigate up in the puzzle grid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* BBNavUpAction;

	/** Navigate down in the puzzle grid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* BBNavDownAction;

	/** Navigate left in the puzzle grid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* BBNavLeftAction;

	/** Navigate right in the puzzle grid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* BBNavRightAction;

	/** Pick up / place piece on the puzzle grid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* BBInteractAction;

	/** Rotate held piece 90° CW. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* BBRotateAction;

	/** Cancel (return piece / exit puzzle). Uses InventoryCancelAction or a dedicated one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* BBCancelAction;

	UPROPERTY()
	ABlackboardPuzzleActor* CurrentBlackboardTarget = nullptr;

	void BeginBlackboardInteraction(ABlackboardPuzzleActor* Board);
	void EndBlackboardInteraction(bool bSolved = false);

protected:
	// Puzzle-mode input handlers
	void OnBBNavUp();
	void OnBBNavDown();
	void OnBBNavLeft();
	void OnBBNavRight();
	void OnBBInteract();
	void OnBBRotate();
	void OnBBCancel();

	/** Called from UseSelectedItem when the target is a blackboard. */
	void UseItemOnBlackboard();

	// ===== END NEW =====

public:
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	void BeginDoorUnlockSequence(ADoorActor* Door);
	void EndDoorUnlockSequence(bool bSuccess);

protected:
    UPROPERTY(VisibleAnywhere, Category = "Interaction")
    class USphereComponent* InteractionSphere;

    UPROPERTY(EditAnywhere, Category = "Interaction")
    float InteractionScanRadius = 800.f;

    UPROPERTY(EditAnywhere, Category = "Interaction")
    float InteractionScanRate = 0.12f;

	FTimerHandle InteractionScanTimerHandle;
	void PerformInteractionScan();

private:
    float InteractionScanAccumulator = 0.f;
    TWeakObjectPtr<AActor> CurrentInteractable;
};