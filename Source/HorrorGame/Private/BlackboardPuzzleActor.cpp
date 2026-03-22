// BlackboardPuzzleActor.cpp
#include "BlackboardPuzzleActor.h"
#include "HorrorGameCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "InteractableUtils.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"

ABlackboardPuzzleActor::ABlackboardPuzzleActor()
{
    PrimaryActorTick.bCanEverTick = true;

    /* ===== Root ===== */
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    /* ===== Board mesh ===== */
    BoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardMesh"));
    BoardMesh->SetupAttachment(Root);
    BoardMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BoardMesh->SetCollisionResponseToAllChannels(ECR_Block);

    /* ===== Grid origins (designer places these in BP) ===== */
    LeftGridOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("LeftGridOrigin"));
    LeftGridOrigin->SetupAttachment(Root);

    RightGridOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("RightGridOrigin"));
    RightGridOrigin->SetupAttachment(Root);

    /* ===== Assembled mesh (hidden until solved) ===== */
    AssembledMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AssembledMesh"));
    AssembledMeshComp->SetupAttachment(Root);
    AssembledMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AssembledMeshComp->SetVisibility(false);

    /* ===== Inherited InteractableActor components ===== */
    if (InteractionBox)
    {
        InteractionBox->SetupAttachment(Root);
        InteractionBox->SetBoxExtent(FVector(40.f, 40.f, 40.f));
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

    /* ===== Init grids to empty ===== */
    for (int32 i = 0; i < 9; ++i)
    {
        LeftGrid[i]  = -1;
        RightGrid[i] = -1;
        PieceRotations[i] = 0;
    }
}

/* ================================================================
 *  LIFECYCLE
 * ================================================================ */

void ABlackboardPuzzleActor::BeginPlay()
{
    Super::BeginPlay();

    // Pre-create 9 pivot + mesh pairs (hidden until puzzle activates)
    PiecePivots.SetNum(9);
    PieceMeshComps.SetNum(9);
    for (int32 i = 0; i < 9; ++i)
    {
        // Pivot: carries world position + grid/puzzle rotation
        FName PivotName = *FString::Printf(TEXT("PiecePivot_%d"), i);
        USceneComponent* Pivot = NewObject<USceneComponent>(this, PivotName);
        Pivot->SetupAttachment(Root);
        Pivot->RegisterComponent();
        Pivot->SetMobility(EComponentMobility::Movable);
        PiecePivots[i] = Pivot;

        // Mesh: child of pivot, carries the base mesh correction as relative rotation
        FName MeshName = *FString::Printf(TEXT("PieceMesh_%d"), i);
        UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, MeshName);
        Comp->SetupAttachment(Pivot);
        Comp->RegisterComponent();
        Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Comp->SetVisibility(false);
        Comp->SetMobility(EComponentMobility::Movable);
        Comp->SetRelativeRotation(PieceMeshBaseRotation);

        if (PieceMeshes.IsValidIndex(i) && PieceMeshes[i])
        {
            Comp->SetStaticMesh(PieceMeshes[i]);
        }

        PieceMeshComps[i] = Comp;
    }

    CreatePieceDynamicMaterials();
}

void ABlackboardPuzzleActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // --- Solved: hide widgets, bail
    if (bSolved)
    {
        if (ArrowWidget) ArrowWidget->SetVisibility(false);
        if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false);
        return;
    }

    // --- Active puzzle: no world widgets needed
    if (PuzzlePhase == EBBPuzzlePhase::Active || PuzzlePhase == EBBPuzzlePhase::WaitingForItem)
    {
        if (ArrowWidget) ArrowWidget->SetVisibility(false);
        if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false);
        return;
    }

    // --- Widget visibility for approach / interact prompt
    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player)
    {
        if (ArrowWidget) ArrowWidget->SetVisibility(false);
        if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false);
        return;
    }

    const bool bCanInteract = CanShowInteraction(Player);
    const bool bCanFull     = CanShowFullInteraction(Player);

    if (ArrowWidget) ArrowWidget->SetVisibility(bCanInteract && !bCanFull);
    if (FullInteractionWidget) FullInteractionWidget->SetVisibility(bCanFull);

    // Rotate widgets to face camera
    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC || !PC->PlayerCameraManager) return;

    FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
    if (ArrowWidget && ArrowWidget->IsVisible())
        FInteractableUtils::FaceWidgetTowardsCamera(ArrowWidget, CamLoc);
    if (FullInteractionWidget && FullInteractionWidget->IsVisible())
        FInteractableUtils::FaceWidgetTowardsCamera(FullInteractionWidget, CamLoc);
}

/* ================================================================
 *  INTERACTABLE API
 * ================================================================ */

bool ABlackboardPuzzleActor::CanShowInteraction(APawn* Player) const
{
    if (!Player || !InteractionBox) return false;
    if (bSolved || PuzzlePhase != EBBPuzzlePhase::Inactive) return false;

    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    return Dist <= InteractionMaxDistance;
}

bool ABlackboardPuzzleActor::CanShowFullInteraction(APawn* Player) const
{
    if (!Player || !InteractionBox) return false;
    if (bSolved || PuzzlePhase != EBBPuzzlePhase::Inactive) return false;

    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    if (Dist > InteractionUseDistance) return false;

    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC) return true;

    FVector2D ScreenPos;
    if (!PC->ProjectWorldLocationToScreen(InteractionBox->GetComponentLocation(), ScreenPos, true))
        return false;

    int32 SizeX = 0, SizeY = 0;
    PC->GetViewportSize(SizeX, SizeY);
    return ScreenPos.X >= 0 && ScreenPos.X <= SizeX
        && ScreenPos.Y >= 0 && ScreenPos.Y <= SizeY;
}

void ABlackboardPuzzleActor::SetFullWidgetVisible(bool bVisible, APawn* /*Player*/)
{
    if (FullInteractionWidget)
        FullInteractionWidget->SetVisibility(bVisible);
}

FVector ABlackboardPuzzleActor::GetInteractionLocation() const
{
    return InteractionBox ? InteractionBox->GetComponentLocation() : GetActorLocation();
}

void ABlackboardPuzzleActor::DeactivateInteractionCamera()
{
    if (InteractionCamera) InteractionCamera->Deactivate();
}

/* ================================================================
 *  GRID HELPERS
 * ================================================================ */

int32* ABlackboardPuzzleActor::GetGrid(EBlackboardSide Side)
{
    return (Side == EBlackboardSide::Left) ? LeftGrid : RightGrid;
}

const int32* ABlackboardPuzzleActor::GetGrid(EBlackboardSide Side) const
{
    return (Side == EBlackboardSide::Left) ? LeftGrid : RightGrid;
}

FVector ABlackboardPuzzleActor::GetSlotWorldPosition(EBlackboardSide Side, int32 Slot, bool bHovering) const
{
    const USceneComponent* Origin = (Side == EBlackboardSide::Left) ? LeftGridOrigin : RightGridOrigin;
    if (!Origin) return GetActorLocation();

    const int32 Row = Slot / 3;
    const int32 Col = Slot % 3;

    // Right = column direction, Down (-Up) = row direction
    FVector Pos = Origin->GetComponentLocation();
    Pos -= Origin->GetRightVector() * Col * GridCellSpacing;
    Pos -= Origin->GetUpVector()    * Row * GridCellSpacing;

    if (bHovering)
    {
        // Push slightly in front of the board surface (along board forward)
        Pos += Origin->GetForwardVector() * HoverOffset;
    }

    return Pos;
}

FRotator ABlackboardPuzzleActor::GetPieceWorldRotation(int32 RotSteps) const
{
    const USceneComponent* Ref = RightGridOrigin ? RightGridOrigin : Root;

    // Grid orientation (aligns to board surface)
    FQuat GridQ = Ref->GetComponentRotation().Quaternion();

    // Puzzle rotation steps around the board's forward (normal) axis
    FVector BoardNormal = Ref->GetForwardVector();
    FQuat StepQ(BoardNormal, FMath::DegreesToRadians(RotSteps * 90.f));

    // PieceMeshBaseRotation is handled separately as the mesh's relative rotation
    return (StepQ * GridQ).Rotator();
}

void ABlackboardPuzzleActor::PlacePieceMeshAtSlot(int32 PieceIdx, EBlackboardSide Side, int32 Slot, bool bHovering)
{
    if (!PiecePivots.IsValidIndex(PieceIdx) || !PieceMeshComps.IsValidIndex(PieceIdx)) return;
    USceneComponent* Pivot = PiecePivots[PieceIdx];
    UStaticMeshComponent* Comp = PieceMeshComps[PieceIdx];
    if (!Pivot || !Comp) return;

    // Set pivot's world transform (position + grid/puzzle rotation)
    Pivot->SetWorldLocation(GetSlotWorldPosition(Side, Slot, bHovering));
    Pivot->SetWorldRotation(GetPieceWorldRotation(PieceRotations[PieceIdx]));

    // Mesh relative rotation stays as PieceMeshBaseRotation (set once in BeginPlay)
    Comp->SetVisibility(true);
}

int32 ABlackboardPuzzleActor::FindFirstEmptySlot(EBlackboardSide Side) const
{
    const int32* Grid = GetGrid(Side);
    for (int32 i = 0; i < 9; ++i)
    {
        if (Grid[i] < 0) return i;
    }
    return -1;
}

int32 ABlackboardPuzzleActor::FindNearestOccupied(EBlackboardSide Side, int32 PreferredSlot) const
{
    const int32* Grid = GetGrid(Side);

    // Exact match first
    if (PreferredSlot >= 0 && PreferredSlot < 9 && Grid[PreferredSlot] >= 0)
        return PreferredSlot;

    // Expand outward
    int32 Best = -1;
    int32 BestDist = INT_MAX;
    const int32 PrefRow = PreferredSlot / 3;
    const int32 PrefCol = PreferredSlot % 3;

    for (int32 i = 0; i < 9; ++i)
    {
        if (Grid[i] < 0) continue;
        const int32 DR = FMath::Abs(i / 3 - PrefRow);
        const int32 DC = FMath::Abs(i % 3 - PrefCol);
        const int32 D  = DR + DC; // manhattan distance
        if (D < BestDist) { BestDist = D; Best = i; }
    }
    return Best;
}

/* ================================================================
 *  HIGHLIGHT  (emissive, same pattern as PadlockActor)
 * ================================================================ */

void ABlackboardPuzzleActor::CreatePieceDynamicMaterials()
{
    PieceDynMaterials.SetNum(9);
    for (int32 i = 0; i < 9; ++i)
    {
        if (!PieceMeshComps.IsValidIndex(i) || !PieceMeshComps[i]) continue;
        UMaterialInstanceDynamic* DynMat = PieceMeshComps[i]->CreateDynamicMaterialInstance(0);
        if (DynMat)
        {
            DynMat->SetScalarParameterValue(EmissiveParamName, 0.f);
        }
        PieceDynMaterials[i] = DynMat;
    }
}

void ABlackboardPuzzleActor::UpdateHighlight()
{
    // Clear all first
    ClearAllHighlights();

    if (PuzzlePhase != EBBPuzzlePhase::Active) return;

    const int32* Grid = GetGrid(CurrentSide);
    const int32 PieceAtSlot = Grid[CurrentSlot];

    // Highlight the piece at the current slot (if any)
    int32 HighlightPiece = -1;

    if (NavState == EBBNavState::Holding)
    {
        HighlightPiece = HeldPieceIndex;
    }
    else if (PieceAtSlot >= 0)
    {
        HighlightPiece = PieceAtSlot;
    }

    if (HighlightPiece >= 0 && PieceDynMaterials.IsValidIndex(HighlightPiece) && PieceDynMaterials[HighlightPiece])
    {
        PieceDynMaterials[HighlightPiece]->SetScalarParameterValue(EmissiveParamName, HighlightEmissiveStrength);
    }
}

void ABlackboardPuzzleActor::ClearAllHighlights()
{
    for (int32 i = 0; i < 9; ++i)
    {
        if (PieceDynMaterials.IsValidIndex(i) && PieceDynMaterials[i])
        {
            PieceDynMaterials[i]->SetScalarParameterValue(EmissiveParamName, 0.f);
        }
    }
}

/* ================================================================
 *  PUZZLE ACTIVATION
 * ================================================================ */

void ABlackboardPuzzleActor::ActivatePuzzle()
{
    PuzzlePhase = EBBPuzzlePhase::Active;

    // --- Randomize piece placement on the left grid ---
    TArray<int32> Indices;
    for (int32 i = 0; i < 9; ++i) Indices.Add(i);

    // Fisher–Yates shuffle
    for (int32 i = Indices.Num() - 1; i > 0; --i)
    {
        int32 j = FMath::RandRange(0, i);
        Indices.Swap(i, j);
    }

    for (int32 Slot = 0; Slot < 9; ++Slot)
    {
        LeftGrid[Slot]  = Indices[Slot];
        RightGrid[Slot] = -1;
    }

    // Random rotations (1, 2, or 3 steps — avoid 0 so at least some are wrong)
    // Actually, give each piece a random rotation 0-3; the shuffle already ensures
    // the puzzle isn't trivially solved even if some rotations are 0.
    for (int32 i = 0; i < 9; ++i)
    {
        PieceRotations[i] = FMath::RandRange(0, 3);
    }

    // Place meshes
    for (int32 Slot = 0; Slot < 9; ++Slot)
    {
        int32 PieceIdx = LeftGrid[Slot];
        PlacePieceMeshAtSlot(PieceIdx, EBlackboardSide::Left, Slot, false);
    }

    // Start browsing on left side, first slot with a piece
    CurrentSide = EBlackboardSide::Left;
    CurrentSlot = FindNearestOccupied(EBlackboardSide::Left, 0);
    NavState = EBBNavState::Browsing;
    HeldPieceIndex = -1;

    UpdateHighlight();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan,
            TEXT("Puzzle started! [WASD] navigate  [E] pick/place  [R] rotate  [Q] cancel"));
    }
}

/* ================================================================
 *  NAVIGATION
 * ================================================================ */

void ABlackboardPuzzleActor::Navigate(int32 DRow, int32 DCol)
{
    if (PuzzlePhase != EBBPuzzlePhase::Active) return;

    /* ----- HOLDING: free movement on right grid ----- */
    if (NavState == EBBNavState::Holding)
    {
        int32 Row = FMath::Clamp(CurrentSlot / 3 + DRow, 0, 2);
        int32 Col = FMath::Clamp(CurrentSlot % 3 + DCol, 0, 2);
        CurrentSlot = Row * 3 + Col;

        // Move the held piece visually
        if (HeldPieceIndex >= 0)
        {
            PlacePieceMeshAtSlot(HeldPieceIndex, EBlackboardSide::Right, CurrentSlot, true);
        }

        UpdateHighlight();
        return;
    }

    /* ----- BROWSING: navigate occupied slots, with side transitions ----- */
    int32 Row = CurrentSlot / 3;
    int32 Col = CurrentSlot % 3;
    int32 NewRow = Row + DRow;
    int32 NewCol = Col + DCol;

    // --- Side transitions ---
    // Left → Right (pressing D past col 2)
    if (CurrentSide == EBlackboardSide::Left && NewCol > 2)
    {
        int32 Target = FindNearestOccupied(EBlackboardSide::Right, Row * 3 + 0);
        if (Target >= 0)
        {
            CurrentSide = EBlackboardSide::Right;
            CurrentSlot = Target;
            UpdateHighlight();
        }
        return;
    }
    // Right → Left (pressing A past col 0)
    if (CurrentSide == EBlackboardSide::Right && NewCol < 0)
    {
        int32 Target = FindNearestOccupied(EBlackboardSide::Left, Row * 3 + 2);
        if (Target >= 0)
        {
            CurrentSide = EBlackboardSide::Left;
            CurrentSlot = Target;
            UpdateHighlight();
        }
        return;
    }

    // Clamp within grid
    NewRow = FMath::Clamp(NewRow, 0, 2);
    NewCol = FMath::Clamp(NewCol, 0, 2);

    int32 TargetSlot = NewRow * 3 + NewCol;

    const int32* Grid = GetGrid(CurrentSide);

    // If target slot is occupied, go there directly
    if (Grid[TargetSlot] >= 0)
    {
        CurrentSlot = TargetSlot;
        UpdateHighlight();
        return;
    }

    // If target slot is empty, search further in the same direction
    int32 SearchRow = NewRow + DRow;
    int32 SearchCol = NewCol + DCol;
    while (SearchRow >= 0 && SearchRow <= 2 && SearchCol >= 0 && SearchCol <= 2)
    {
        int32 SearchSlot = SearchRow * 3 + SearchCol;
        if (Grid[SearchSlot] >= 0)
        {
            CurrentSlot = SearchSlot;
            UpdateHighlight();
            return;
        }
        SearchRow += DRow;
        SearchCol += DCol;
    }

    // Couldn't find occupied slot in that direction — check side transition
    if (CurrentSide == EBlackboardSide::Left && DCol > 0)
    {
        int32 Target = FindNearestOccupied(EBlackboardSide::Right, Row * 3 + 0);
        if (Target >= 0)
        {
            CurrentSide = EBlackboardSide::Right;
            CurrentSlot = Target;
            UpdateHighlight();
        }
    }
    else if (CurrentSide == EBlackboardSide::Right && DCol < 0)
    {
        int32 Target = FindNearestOccupied(EBlackboardSide::Left, Row * 3 + 2);
        if (Target >= 0)
        {
            CurrentSide = EBlackboardSide::Left;
            CurrentSlot = Target;
            UpdateHighlight();
        }
    }
}

/* ================================================================
 *  INTERACT  (pick up / place)
 * ================================================================ */

void ABlackboardPuzzleActor::InteractPiece()
{
    if (PuzzlePhase != EBBPuzzlePhase::Active) return;

    /* ----- BROWSING → pick up piece ----- */
    if (NavState == EBBNavState::Browsing)
    {
        int32* Grid = GetGrid(CurrentSide);
        const int32 PieceIdx = Grid[CurrentSlot];
        if (PieceIdx < 0) return; // nothing to pick up

        HeldPieceIndex    = PieceIdx;
        HeldOriginSlot    = CurrentSlot;
        HeldOriginSide    = CurrentSide;

        // Remove from current grid
        Grid[CurrentSlot] = -1;

        if (CurrentSide == EBlackboardSide::Left)
        {
            // Transfer to right side, first empty slot
            int32 EmptySlot = FindFirstEmptySlot(EBlackboardSide::Right);
            if (EmptySlot < 0)
            {
                // Right grid full — place back on held origin slot 0 (shouldn't happen normally)
                EmptySlot = 0;
            }

            CurrentSide = EBlackboardSide::Right;
            CurrentSlot = EmptySlot;
        }
        // If picked up from right side, stay at same slot

        NavState = EBBNavState::Holding;

        // Visually hover
        PlacePieceMeshAtSlot(HeldPieceIndex, EBlackboardSide::Right, CurrentSlot, true);
        UpdateHighlight();

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
                FString::Printf(TEXT("Picked up piece %d — move & place with [E], rotate [R]"), HeldPieceIndex));
        }
        return;
    }

    /* ----- HOLDING → place piece ----- */
    if (NavState == EBBNavState::Holding && HeldPieceIndex >= 0)
    {
        int32 ExistingPiece = RightGrid[CurrentSlot];

        // Place held piece
        RightGrid[CurrentSlot] = HeldPieceIndex;
        PlacePieceMeshAtSlot(HeldPieceIndex, EBlackboardSide::Right, CurrentSlot, false);

        // Swap: if slot was occupied, send old piece to where held piece came from
        if (ExistingPiece >= 0)
        {
            if (HeldOriginSide == EBlackboardSide::Left)
            {
                LeftGrid[HeldOriginSlot] = ExistingPiece;
                PlacePieceMeshAtSlot(ExistingPiece, EBlackboardSide::Left, HeldOriginSlot, false);
            }
            else
            {
                // Came from right side — put old piece in the held origin slot
                RightGrid[HeldOriginSlot] = ExistingPiece;
                PlacePieceMeshAtSlot(ExistingPiece, EBlackboardSide::Right, HeldOriginSlot, false);
            }
        }

        HeldPieceIndex = -1;
        NavState = EBBNavState::Browsing;

        // Select first occupied slot on left side (if any), otherwise stay on right
        int32 LeftFirst = FindNearestOccupied(EBlackboardSide::Left, 0);
        if (LeftFirst >= 0)
        {
            CurrentSide = EBlackboardSide::Left;
            CurrentSlot = LeftFirst;
        }
        else
        {
            // All pieces on right — stay right, find first occupied
            int32 RightFirst = FindNearestOccupied(EBlackboardSide::Right, 0);
            if (RightFirst >= 0)
            {
                CurrentSide = EBlackboardSide::Right;
                CurrentSlot = RightFirst;
            }
        }

        UpdateHighlight();

        // Check solution
        if (CheckSolution())
        {
            OnPuzzleSolved();
        }
        return;
    }
}

/* ================================================================
 *  ROTATE HELD PIECE
 * ================================================================ */

void ABlackboardPuzzleActor::RotateHeldPiece()
{
    if (NavState != EBBNavState::Holding || HeldPieceIndex < 0) return;

    PieceRotations[HeldPieceIndex] = (PieceRotations[HeldPieceIndex] + 1) % 4;

    // Update visual
    PlacePieceMeshAtSlot(HeldPieceIndex, EBlackboardSide::Right, CurrentSlot, true);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan,
            FString::Printf(TEXT("Piece %d rotation: %d°"),
                HeldPieceIndex, PieceRotations[HeldPieceIndex] * 90));
    }
}

/* ================================================================
 *  CANCEL
 * ================================================================ */

bool ABlackboardPuzzleActor::CancelAction()
{
    if (NavState == EBBNavState::Holding && HeldPieceIndex >= 0)
    {
        // Return piece to origin
        int32* OriginGrid = GetGrid(HeldOriginSide);
        OriginGrid[HeldOriginSlot] = HeldPieceIndex;
        PlacePieceMeshAtSlot(HeldPieceIndex, HeldOriginSide, HeldOriginSlot, false);

        HeldPieceIndex = -1;
        NavState = EBBNavState::Browsing;

        // Go back to origin
        CurrentSide = HeldOriginSide;
        CurrentSlot = HeldOriginSlot;
        UpdateHighlight();
        return true; // handled, don't exit puzzle
    }

    // Not holding — signal that character should exit puzzle
    return false;
}

/* ================================================================
 *  SOLUTION CHECK
 * ================================================================ */

bool ABlackboardPuzzleActor::CheckSolution() const
{
    for (int32 Slot = 0; Slot < 9; ++Slot)
    {
        // Piece i must be in slot i on the right grid, with rotation 0
        if (RightGrid[Slot] != Slot) return false;
        if (PieceRotations[Slot] != 0) return false;
    }
    return true;
}

void ABlackboardPuzzleActor::OnPuzzleSolved()
{
    bSolved = true;
    PuzzlePhase = EBBPuzzlePhase::Solved;
    ClearAllHighlights();

    UE_LOG(LogTemp, Log, TEXT("BlackboardPuzzle: SOLVED!"));

    // Show assembled mesh if provided
    if (AssembledMesh && AssembledMeshComp)
    {
        AssembledMeshComp->SetStaticMesh(AssembledMesh);
        AssembledMeshComp->SetVisibility(true);

        // Hide individual pieces
        for (UStaticMeshComponent* Comp : PieceMeshComps)
        {
            if (Comp) Comp->SetVisibility(false);
        }
    }
    // else pieces stay visible in their solved positions

    if (ArrowWidget) ArrowWidget->SetVisibility(false);
    if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Puzzle solved!"));
    }

    // End interaction on character side
    if (CallbackCharacter)
    {
        CallbackCharacter->EndBlackboardInteraction(true);
    }
}