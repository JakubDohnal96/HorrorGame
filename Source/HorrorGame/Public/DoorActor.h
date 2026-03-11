#pragma once

#include "CoreMinimal.h"
#include "InteractableActor.h"
#include "DoorActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UWidgetComponent;
class UCameraComponent;
class UStaticMesh;

UENUM(BlueprintType)
enum class EDoorSymbol : uint8
{
    Symbol_A UMETA(DisplayName="Symbol_A"),
    Symbol_B UMETA(DisplayName="Symbol_B"),
    Symbol_C UMETA(DisplayName="Symbol_C"),
    Symbol_D UMETA(DisplayName="Symbol_D"),
};

UCLASS()
class HORRORGAME_API ADoorActor : public AInteractableActor
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

    virtual bool CanShowInteraction(APawn* Player) const override;
    virtual bool CanShowFullInteraction(APawn* Player) const override;
    virtual void SetFullWidgetVisible(bool bVisible, APawn* Player) override;
    virtual UBoxComponent* GetActiveInteractionBox(APawn* Player) const override;

    /* ===== Components ===== */

    UPROPERTY(VisibleAnywhere)
    USceneComponent* Root;

    UPROPERTY(VisibleAnywhere)
    USceneComponent* HingeAxis;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* DoorLeaf;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* Handle;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* Hinge;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* Backplate;

    UPROPERTY(VisibleAnywhere)
    UBoxComponent* BlockSensor;

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

    /* ===== Front interaction ===== */

    UPROPERTY(VisibleAnywhere)
    UBoxComponent* InteractionBox_Front;

    UPROPERTY(VisibleAnywhere)
    UWidgetComponent* ArrowWidget_Front;

    UPROPERTY(VisibleAnywhere)
    UWidgetComponent* InteractionWidget_Front;

    /* ===== Back interaction ===== */

    UPROPERTY(VisibleAnywhere)
    UBoxComponent* InteractionBox_Back;

    UPROPERTY(VisibleAnywhere)
    UWidgetComponent* ArrowWidget_Back;

    UPROPERTY(VisibleAnywhere)
    UWidgetComponent* InteractionWidget_Back;

    /* ===== Cameras ===== */

    UPROPERTY(VisibleAnywhere)
    UCameraComponent* InteractionCamera_Front;

    UPROPERTY(VisibleAnywhere)
    UCameraComponent* InteractionCamera_Back;

    /* ===== Symbol ===== */

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* SymbolMeshComponent;

    UPROPERTY(EditAnywhere)
    TArray<UStaticMesh*> SymbolMeshes;

    UPROPERTY(EditAnywhere)
    int32 SelectedSymbolIndex;

    int32 CurrentSymbolIndex;

    /* ===== Key insert ===== */

    UPROPERTY(VisibleAnywhere)
    USceneComponent* KeyInsertPoint;

    /* ===== Door state ===== */

    bool bIsOpen;
    bool bIsMoving;
    bool bBlockedByPlayer;
    bool bShouldBlockDuringThisMove;

    bool bWaitingForUnblock;

    float CurrentYaw;
    float BlockedTime;
    float LastTimePlayerNearby;

    /* ===== Settings ===== */

    UPROPERTY(EditAnywhere)
    float OpenAngle;

    UPROPERTY(EditAnywhere)
    float OpenSpeed;

    UPROPERTY(EditAnywhere)
    float InteractionMaxDistance;

    UPROPERTY(EditAnywhere)
    float InteractionUseDistance;

    UPROPERTY(EditAnywhere)
    float AutoCloseDistance;

    UPROPERTY(EditAnywhere)
    float AutoCloseDelay;

    UPROPERTY(EditAnywhere)
    float UnblockDelay;

    /* ===== Lock ===== */

    UPROPERTY(EditAnywhere)
    bool bStartsLocked;

    bool bLocked;

public:

    UFUNCTION(BlueprintCallable)
    void UnlockDoor();

    UFUNCTION(BlueprintCallable)
    bool IsLocked() const { return bLocked; }

    UFUNCTION(BlueprintCallable)
    bool SetSymbolByIndex(int32 Index);

    UFUNCTION(BlueprintCallable)
    UStaticMesh* GetCurrentSymbolMesh() const;

    UCameraComponent* GetActiveInteractionCamera(const APawn* Player) const;
    void DeactivateInteractionCameras();

    FTransform GetKeyInsertTransform() const;

	UFUNCTION(BlueprintCallable, Category="Door|Appearance")
	int32 GetCurrentSymbolIndex() const { return CurrentSymbolIndex; }

private:

    bool IsMovingTowardsPlayer(const AActor* Player) const;
    bool IsInteractionBoxOnScreen(APawn* Player, UBoxComponent* Box) const;
    UWidgetComponent* GetActiveInteractionWidget(const APawn* Player) const;
};