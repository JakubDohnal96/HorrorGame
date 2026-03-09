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

	/* ===== UI ===== */

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

    // Inventory widget
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
    TSubclassOf<UUserWidget> InventoryWidgetClass;

	UPROPERTY()
	UInventoryWidgetBase* InventoryWidgetInstance;

    // Inventory open flag
    bool bInventoryOpen = false;

    // Input actions (if using Enhanced Input, add UInputAction* properties in header)
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

	// Inventory UI control
	UFUNCTION()
	void ToggleInventory();

	UFUNCTION()
	void OnInventoryChanged();

	// Inventory navigation
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
	UInputAction* InventoryUseAction; // mapped to E inside IMC_UI

	// Current door we are trying to unlock (set while inventory open for unlocking)
	UPROPERTY()
	class ADoorActor* CurrentDoorUnlockTarget = nullptr;

	// Function to try using currently selected item
	void UseSelectedItem();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* InventoryCancelAction;

	// cancel handler
	void CancelDoorUnlock();

	UPROPERTY(EditAnywhere, Category="Keys")
	UStaticMesh* KeyMesh_A;

	UPROPERTY(EditAnywhere, Category="Keys")
	UStaticMesh* KeyMesh_B;

	UPROPERTY(EditAnywhere, Category="Keys")
	UStaticMesh* KeyMesh_C;

	UPROPERTY(EditAnywhere, Category="Keys")
	UStaticMesh* KeyMesh_D;

	// ===== PC Terminal =====
	UPROPERTY(EditAnywhere, Category="Input") 
	class UInputMappingContext* IMC_PCInteraction;

	UPROPERTY(EditAnywhere, Category="Input") 
	UInputAction* PCInteractAction;

	UPROPERTY()
	class APCTerminalActor* CurrentPCTerminalTarget = nullptr;

	// Begin/End PC interaction
	void BeginPCInteraction(APCTerminalActor* PCActor);
	void EndPCInteraction(bool bSuccess = false);

public:
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	// Start/End unlock sequence helpers
	void BeginDoorUnlockSequence(ADoorActor* Door);
	void EndDoorUnlockSequence(bool bSuccess);
};