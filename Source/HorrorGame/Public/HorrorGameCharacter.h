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
class APadlockActor;  // ← NEW

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

	// ===== NEW: Padlock Interaction =====

	/** IMC active while manipulating the padlock (A/D switch dials, W/S rotate, Q cancel). */
	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputMappingContext* IMC_PadlockInteraction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* PadlockDialPrevAction;   // A

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* PadlockDialNextAction;   // D

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* PadlockRotateUpAction;   // W

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* PadlockRotateDownAction; // S

	UPROPERTY()
	APadlockActor* CurrentPadlockTarget = nullptr;

	void BeginPadlockInteraction(APadlockActor* Padlock);
	void OnPadlockDialPrev();
	void OnPadlockDialNext();
	void OnPadlockRotateUp();
	void OnPadlockRotateDown();

public:
	/** Called by PadlockActor when unlock animation completes (or by Q cancel). */
	void EndPadlockInteraction();

	// ===== END NEW =====

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