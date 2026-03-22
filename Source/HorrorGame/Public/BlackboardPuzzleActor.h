// BlackboardPuzzleActor.h
#pragma once

#include "CoreMinimal.h"
#include "InteractableActor.h"
#include "BlackboardPuzzleActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class AHorrorGameCharacter;
class UMaterialInstanceDynamic;

/* ===== Enums ===== */

UENUM()
enum class EBlackboardSide : uint8
{
    Left,
    Right
};

UENUM()
enum class EBBNavState : uint8
{
    Browsing,   // navigating between occupied slots
    Holding     // carrying a piece, navigating right grid freely
};

UENUM()
enum class EBBPuzzlePhase : uint8
{
    Inactive,       // puzzle not started yet
    WaitingForItem, // inventory open, waiting for player to choose item
    Active,         // puzzle in progress
    Solved          // all pieces correctly placed
};

/**
 * A blackboard that hosts a 9-piece torn-paper reassembly puzzle.
 *
 * Layout:  LEFT 3x3 grid  |  RIGHT 3x3 grid
 *
 * Pieces start randomised on the left. The player transfers them one-by-one
 * to the right and arranges them into the correct positions and rotations.
 */
UCLASS()
class HORRORGAME_API ABlackboardPuzzleActor : public AInteractableActor
{
    GENERATED_BODY()

public:
    ABlackboardPuzzleActor();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /* ====================================================================
     *  COMPONENTS
     * ==================================================================== */
public:
    UPROPERTY(VisibleAnywhere, Category = "Blackboard|Components")
    USceneComponent* Root;

    /** The visible blackboard mesh. */
    UPROPERTY(VisibleAnywhere, Category = "Blackboard|Components")
    UStaticMeshComponent* BoardMesh;

    /**
     * Origin of the left 3x3 grid (top-left corner of slot [0,0]).
     * Place this in the BP where the top-left piece should sit.
     */
    UPROPERTY(VisibleAnywhere, Category = "Blackboard|Components")
    USceneComponent* LeftGridOrigin;

    /**
     * Origin of the right 3x3 grid (top-left corner of slot [0,0]).
     * Place this in the BP where the top-left assembly slot should sit.
     */
    UPROPERTY(VisibleAnywhere, Category = "Blackboard|Components")
    USceneComponent* RightGridOrigin;

    /**
     * Optional: a single assembled mesh shown after the puzzle is solved,
     * replacing the 9 individual pieces.  Leave nullptr to keep pieces visible.
     */
    UPROPERTY(VisibleAnywhere, Category = "Blackboard|Components")
    UStaticMeshComponent* AssembledMeshComp;

    /* ====================================================================
     *  INTERACTABLE API  (inherited overrides)
     * ==================================================================== */

    virtual bool CanShowInteraction(APawn* Player) const override;
    virtual bool CanShowFullInteraction(APawn* Player) const override;
    virtual void SetFullWidgetVisible(bool bVisible, APawn* Player) override;
    virtual FVector GetInteractionLocation() const override;
    void DeactivateInteractionCamera();

    /* ====================================================================
     *  CONFIGURATION  (set per-instance in BP)
     * ==================================================================== */

    /** 9 static meshes, one per torn piece (index 0–8 = correct grid order). */
    UPROPERTY(EditAnywhere, Category = "Blackboard|Puzzle")
    TArray<UStaticMesh*> PieceMeshes;

    /**
     * The ItemTypeIndex that the player's inventory item must have for the
     * puzzle to accept it (same system as door KeyIndex).
     */
    UPROPERTY(EditAnywhere, Category = "Blackboard|Puzzle")
    int32 RequiredItemIndex = 100;

    /** Optional assembled mesh shown on completion (set in BP). */
    UPROPERTY(EditAnywhere, Category = "Blackboard|Puzzle")
    UStaticMesh* AssembledMesh = nullptr;

    /**
     * Extra rotation applied to every piece mesh so its flat face aligns
     * with the board surface.  Adjust in the BP until pieces lie flat.
     * Example: if the mesh normal points +Z but the board normal is +X,
     * set this to (Pitch=0, Yaw=0, Roll=-90) — or whatever makes them flat.
     */
    UPROPERTY(EditAnywhere, Category = "Blackboard|Grid")
    FRotator PieceMeshBaseRotation = FRotator::ZeroRotator;

    /** Spacing between slot centres (cm). */
    UPROPERTY(EditAnywhere, Category = "Blackboard|Grid")
    float GridCellSpacing = 14.f;

    /** How far a held piece hovers in front of the board surface (cm). */
    UPROPERTY(EditAnywhere, Category = "Blackboard|Grid")
    float HoverOffset = 1.0f;

    /* ===== Interaction distances ===== */

    UPROPERTY(EditAnywhere, Category = "Blackboard|Interaction")
    float InteractionMaxDistance = 250.f;

    UPROPERTY(EditAnywhere, Category = "Blackboard|Interaction")
    float InteractionUseDistance = 100.f;

    /* ===== Highlight / emissive ===== */

    UPROPERTY(EditAnywhere, Category = "Blackboard|Highlight")
    float HighlightEmissiveStrength = 1.5f;

    UPROPERTY(EditAnywhere, Category = "Blackboard|Highlight")
    FName EmissiveParamName = FName(TEXT("EmissiveBoost"));

    /* ====================================================================
     *  PUZZLE STATE  (public so character can read / drive)
     * ==================================================================== */

    EBBPuzzlePhase PuzzlePhase = EBBPuzzlePhase::Inactive;

    /** Currently selected side and slot. */
    EBlackboardSide CurrentSide = EBlackboardSide::Left;
    int32           CurrentSlot = 0;

    /** Navigation / interaction state. */
    EBBNavState NavState = EBBNavState::Browsing;

    /** Index of the piece currently held (-1 = none). */
    int32 HeldPieceIndex = -1;

    /** The slot the held piece was picked up from (for cancel / swap). */
    int32 HeldOriginSlot = -1;
    EBlackboardSide HeldOriginSide = EBlackboardSide::Left;

    /**
     * Grid contents.  Value = piece index (0–8), or -1 if empty.
     * LeftGrid[slot], RightGrid[slot].
     */
    int32 LeftGrid[9];
    int32 RightGrid[9];

    /** Rotation of each piece in 90° steps (0 = correct, 1 = 90°, 2 = 180°, 3 = 270°). */
    int32 PieceRotations[9];

    bool bSolved = false;

    /** Character reference for callbacks. */
    UPROPERTY()
    AHorrorGameCharacter* CallbackCharacter = nullptr;

    /* ====================================================================
     *  PUZZLE API  (called by the character)
     * ==================================================================== */

    /** Spawn the 9 pieces on the left grid in random order & rotation. */
    void ActivatePuzzle();

    /** WASD navigation.  DRow/DCol are ±1. */
    void Navigate(int32 DRow, int32 DCol);

    /** Interact: pick up / place piece. */
    void InteractPiece();

    /** Rotate the currently held piece 90° CW. */
    void RotateHeldPiece();

    /** Cancel: if holding → return piece; otherwise exit handled by character. */
    bool CancelAction();

    /** Check if all 9 pieces are in the correct slot with rotation 0 on the right grid. */
    bool CheckSolution() const;

    /* ====================================================================
     *  HIGHLIGHT
     * ==================================================================== */

    void UpdateHighlight();
    void ClearAllHighlights();

private:
    /* ===== Piece mesh components (created at BeginPlay) ===== */
    /** Pivot scene components — one per piece. World rotation = grid + puzzle steps. */
    UPROPERTY()
    TArray<USceneComponent*> PiecePivots;

    /** Mesh components — children of pivots. Relative rotation = PieceMeshBaseRotation. */
    UPROPERTY()
    TArray<UStaticMeshComponent*> PieceMeshComps;

    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> PieceDynMaterials;

    /* ===== Helpers ===== */

    /** World position of a grid slot. */
    FVector GetSlotWorldPosition(EBlackboardSide Side, int32 Slot, bool bHovering = false) const;

    /** World rotation for a piece given its rotation steps. */
    FRotator GetPieceWorldRotation(int32 RotSteps) const;

    /** Move a piece mesh to a slot (or hovering above it). */
    void PlacePieceMeshAtSlot(int32 PieceIdx, EBlackboardSide Side, int32 Slot, bool bHovering = false);

    /** Find the first empty slot on a side (-1 if full). */
    int32 FindFirstEmptySlot(EBlackboardSide Side) const;

    /** Find nearest occupied slot to a preferred slot on a given side (-1 if none). */
    int32 FindNearestOccupied(EBlackboardSide Side, int32 PreferredSlot) const;

    /** Create dynamic material instances for highlight support. */
    void CreatePieceDynamicMaterials();

    /** Called when puzzle is solved. */
    void OnPuzzleSolved();

    /** Delayed end of interaction after solve (so player sees the assembled result). */
    void OnSolveDelayFinished();

    FTimerHandle SolveDelayTimerHandle;

    /** How long (seconds) the camera stays on the assembled paper before exiting. */
    UPROPERTY(EditAnywhere, Category = "Blackboard|Puzzle")
    float SolveViewDelay = 2.5f;

    /** Reference to grid array by side. */
    int32* GetGrid(EBlackboardSide Side);
    const int32* GetGrid(EBlackboardSide Side) const;
};