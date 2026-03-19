// PadlockActor.h
#pragma once

#include "CoreMinimal.h"
#include "InteractableActor.h"
#include "PadlockActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class ADoorActor;
class AHorrorGameCharacter;

UENUM()
enum class EPadlockAnimPhase : uint8
{
    None,
    ShackleRelease,   // PadlockRight slides -Y
    BodySwing,        // SwingPivot rotates around X (Roll)
    HideAndRotate,    // Hide padlock parts, rotate RotatableLockFrame around Z
    Complete
};

UCLASS()
class HORRORGAME_API APadlockActor : public AInteractableActor
{
    GENERATED_BODY()

public:
    APadlockActor();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /* ========== Components ========== */
public:
    UPROPERTY(VisibleAnywhere, Category = "Padlock|Components")
    USceneComponent* Root;

    // --- Frame parts (remain after unlock) ---

    /** Stays on the door frame, never moves. */
    UPROPERTY(VisibleAnywhere, Category = "Padlock|Components")
    UStaticMeshComponent* StaticLockFrame;

    /** Rotates 115° around Z during unlock. */
    UPROPERTY(VisibleAnywhere, Category = "Padlock|Components")
    UStaticMeshComponent* RotatableLockFrame;

    /**
     * Attached to the door leaf — moves with the door when it opens/closes.
     * In BeginPlay this component is re-parented to LinkedDoor->HingeAxis.
     */
    UPROPERTY(VisibleAnywhere, Category = "Padlock|Components")
    UStaticMeshComponent* StaticLockDoor;

    // --- Padlock body (hidden after unlock) ---

    /** U-shaped shackle. Does NOT swing; just hides after unlock. */
    UPROPERTY(VisibleAnywhere, Category = "Padlock|Components")
    UStaticMeshComponent* PadlockU;

    /**
     * Scene component positioned at PadlockLeft's pivot.
     * Everything that swings during unlock is a child of this.
     */
    UPROPERTY(VisibleAnywhere, Category = "Padlock|Components")
    USceneComponent* SwingPivot;

    UPROPERTY(VisibleAnywhere, Category = "Padlock|Components")
    UStaticMeshComponent* PadlockLeft;

    UPROPERTY(VisibleAnywhere, Category = "Padlock|Components")
    UStaticMeshComponent* PadlockRight;

    UPROPERTY(VisibleAnywhere, Category = "Padlock|Components")
    UStaticMeshComponent* PadlockRotatable1;

    UPROPERTY(VisibleAnywhere, Category = "Padlock|Components")
    UStaticMeshComponent* PadlockRotatable2;

    UPROPERTY(VisibleAnywhere, Category = "Padlock|Components")
    UStaticMeshComponent* PadlockRotatable3;

    UPROPERTY(VisibleAnywhere, Category = "Padlock|Components")
    UStaticMeshComponent* PadlockRotatable4;

    UPROPERTY(VisibleAnywhere, Category = "Padlock|Components")
    UStaticMeshComponent* PadlockSeparation;

    UPROPERTY(VisibleAnywhere, Category = "Padlock|Components")
    UStaticMeshComponent* PadlockBeams;

    /* ========== Interactable API ========== */

    virtual bool CanShowInteraction(APawn* Player) const override;
    virtual bool CanShowFullInteraction(APawn* Player) const override;
    virtual void SetFullWidgetVisible(bool bVisible, APawn* Player) override;
    virtual FVector GetInteractionLocation() const override;
    void DeactivateInteractionCamera();

    /* ========== Door link ========== */

    /** The door this padlock blocks. Set per-instance in the level. */
    UPROPERTY(EditInstanceOnly, Category = "Padlock|Door")
    ADoorActor* LinkedDoor = nullptr;

    /* ========== Puzzle ========== */

    /**
     * The correct combination (4 digits, each 0–9).
     * Default: 1, 9, 4, 7
     */
    UPROPERTY(EditAnywhere, Category = "Padlock|Puzzle")
    TArray<int32> CorrectCombination;

    /** Currently selected dial index (0–3). */
    int32 SelectedDialIndex = 0;

    /** Current value of each dial (0–9). All start at 0. */
    int32 DialValues[4];

    /** Has the puzzle been solved? */
    bool bSolved = false;

    /** Is the unlock animation playing? */
    bool bAnimating = false;

    /* ========== Dial API (called by character) ========== */

    void SelectNextDial();
    void SelectPreviousDial();
    void RotateSelectedDial(int32 Direction); // +1 = up, -1 = down

    /** Start the multi-phase unlock animation. */
    void StartUnlockAnimation(AHorrorGameCharacter* OwningCharacter);

    /* ========== Interaction distances ========== */

    UPROPERTY(EditAnywhere, Category = "Padlock|Interaction")
    float InteractionMaxDistance = 250.f;

    UPROPERTY(EditAnywhere, Category = "Padlock|Interaction")
    float InteractionUseDistance = 100.f;

    /* ========== Animation tuning ========== */

    UPROPERTY(EditAnywhere, Category = "Padlock|Animation")
    float ShackleSlideDistance = 0.3f;  // cm, along +Y

    UPROPERTY(EditAnywhere, Category = "Padlock|Animation")
    float ShackleSlideDuration = 0.6f;

    UPROPERTY(EditAnywhere, Category = "Padlock|Animation")
    float BodySwingDegrees = 15.f;

    UPROPERTY(EditAnywhere, Category = "Padlock|Animation")
    float BodySwingDuration = 1.0f;

    UPROPERTY(EditAnywhere, Category = "Padlock|Animation")
    float FrameRotateDegrees = 115.f;

    UPROPERTY(EditAnywhere, Category = "Padlock|Animation")
    float FrameRotateDuration = 1.6f;

    /** Degrees per dial step (360 / 10 = 36). */
    UPROPERTY(EditAnywhere, Category = "Padlock|Puzzle")
    float DegreesPerStep = 36.f;

    /**
     * Base rotation offset (degrees) applied to every dial so the mesh's "0"
     * mark aligns with the readable position. Set this in the editor until
     * the dials show "0" correctly at game start. Default: 234 (6.5 * 36).
     */
    UPROPERTY(EditAnywhere, Category = "Padlock|Puzzle")
    float BaseDialOffset = 126.f;

    /* ========== Dial highlight ========== */

    /** Emissive strength when a dial is selected. 0 = off, 1+ = glow. */
    UPROPERTY(EditAnywhere, Category = "Padlock|Highlight")
    float HighlightEmissiveStrength = 1.5f;

    /** Name of the scalar parameter in the dial material that controls emissive. */
    UPROPERTY(EditAnywhere, Category = "Padlock|Highlight")
    FName EmissiveParamName = FName(TEXT("EmissiveBoost"));

    /** Update which dial is highlighted. Called when selection changes or interaction starts/ends. */
    void UpdateDialHighlight(int32 ActiveIndex);

    /** Clear highlight from all dials (called when leaving interaction). */
    void ClearAllDialHighlights();

    /** Set by the character when interaction starts; cleared on end. */
    UPROPERTY()
    AHorrorGameCharacter* CallbackCharacter = nullptr;

    /** Whether we're currently in interaction mode (highlights should be active). */
    bool bInInteractionMode = false;

private:
    /* ========== Dial dynamic materials ========== */

    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> DialDynMaterials;

    void CreateDialDynamicMaterials();

    /* ========== Animation state ========== */

    EPadlockAnimPhase AnimPhase = EPadlockAnimPhase::None;
    float AnimElapsed = 0.f;

    // Shackle release
    FVector ShackleStartLoc;
    FVector ShackleTargetLoc;

    // Body swing
    FRotator SwingStartRot;
    FRotator SwingTargetRot;

    // Frame rotate
    FRotator FrameStartRot;
    FRotator FrameTargetRot;

    void FinishUnlockAnimation();
    bool CheckCombination() const;

    /** Quick access to dial mesh by index. */
    UStaticMeshComponent* GetDialMesh(int32 Index) const;

    /** Apply the visual rotation to a dial mesh based on its value. */
    void ApplyDialRotation(int32 Index);
};