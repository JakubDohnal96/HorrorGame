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
#include "TimerManager.h"

ABlackboardPuzzleActor::ABlackboardPuzzleActor()
{
    PrimaryActorTick.bCanEverTick = true;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
    BoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardMesh"));
    BoardMesh->SetupAttachment(Root);
    BoardMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BoardMesh->SetCollisionResponseToAllChannels(ECR_Block);
    LeftGridOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("LeftGridOrigin"));
    LeftGridOrigin->SetupAttachment(Root);
    RightGridOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("RightGridOrigin"));
    RightGridOrigin->SetupAttachment(Root);
    AssembledMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AssembledMesh"));
    AssembledMeshComp->SetupAttachment(Root);
    AssembledMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AssembledMeshComp->SetVisibility(false);
    if (InteractionBox) { InteractionBox->SetupAttachment(Root); InteractionBox->SetBoxExtent(FVector(40.f,40.f,40.f)); InteractionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision); InteractionBox->SetGenerateOverlapEvents(false); }
    if (ArrowWidget) { ArrowWidget->SetupAttachment(InteractionBox); ArrowWidget->SetWidgetSpace(EWidgetSpace::World); ArrowWidget->SetDrawAtDesiredSize(true); ArrowWidget->SetVisibility(false); ArrowWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision); ArrowWidget->SetBlendMode(EWidgetBlendMode::Transparent); ArrowWidget->SetRenderCustomDepth(true); }
    if (FullInteractionWidget) { FullInteractionWidget->SetupAttachment(InteractionBox); FullInteractionWidget->SetWidgetSpace(EWidgetSpace::World); FullInteractionWidget->SetDrawAtDesiredSize(true); FullInteractionWidget->SetVisibility(false); FullInteractionWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision); FullInteractionWidget->SetBlendMode(EWidgetBlendMode::Transparent); FullInteractionWidget->SetRenderCustomDepth(true); }
    if (InteractionCamera) { InteractionCamera->SetupAttachment(Root); InteractionCamera->bAutoActivate = false; }
    for (int32 i = 0; i < 9; ++i) { LeftGrid[i] = -1; RightGrid[i] = -1; PieceRotations[i] = 0; }
}

void ABlackboardPuzzleActor::BeginPlay()
{
    Super::BeginPlay();
    PiecePivots.SetNum(9);
    PieceMeshComps.SetNum(9);
    for (int32 i = 0; i < 9; ++i)
    {
        FName PivotName = *FString::Printf(TEXT("PiecePivot_%d"), i);
        USceneComponent* Pivot = NewObject<USceneComponent>(this, PivotName);
        Pivot->SetupAttachment(Root); Pivot->RegisterComponent(); Pivot->SetMobility(EComponentMobility::Movable);
        PiecePivots[i] = Pivot;
        FName MeshName = *FString::Printf(TEXT("PieceMesh_%d"), i);
        UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, MeshName);
        Comp->SetupAttachment(Pivot); Comp->RegisterComponent(); Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Comp->SetVisibility(false); Comp->SetMobility(EComponentMobility::Movable); Comp->SetRelativeRotation(PieceMeshBaseRotation);
        if (PieceMeshes.IsValidIndex(i) && PieceMeshes[i]) Comp->SetStaticMesh(PieceMeshes[i]);
        PieceMeshComps[i] = Comp;
    }
    CreatePieceDynamicMaterials();
}

void ABlackboardPuzzleActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (bSolved) { if (ArrowWidget) ArrowWidget->SetVisibility(false); if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false); return; }
    if (PuzzlePhase == EBBPuzzlePhase::WaitingForItem) { if (ArrowWidget) ArrowWidget->SetVisibility(false); if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false); return; }
    if (PuzzlePhase == EBBPuzzlePhase::Active && CallbackCharacter) { if (ArrowWidget) ArrowWidget->SetVisibility(false); if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false); return; }
    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player) { if (ArrowWidget) ArrowWidget->SetVisibility(false); if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false); return; }
    const bool bCanInteract = CanShowInteraction(Player);
    const bool bCanFull = CanShowFullInteraction(Player);
    if (ArrowWidget) ArrowWidget->SetVisibility(bCanInteract && !bCanFull);
    if (FullInteractionWidget) FullInteractionWidget->SetVisibility(bCanFull);
    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC || !PC->PlayerCameraManager) return;
    FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
    if (ArrowWidget && ArrowWidget->IsVisible()) FInteractableUtils::FaceWidgetTowardsCamera(ArrowWidget, CamLoc);
    if (FullInteractionWidget && FullInteractionWidget->IsVisible()) FInteractableUtils::FaceWidgetTowardsCamera(FullInteractionWidget, CamLoc);
}

bool ABlackboardPuzzleActor::CanShowInteraction(APawn* Player) const
{
    if (!Player || !InteractionBox) return false;
    if (bSolved || PuzzlePhase == EBBPuzzlePhase::WaitingForItem) return false;
    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    if (Dist > InteractionMaxDistance) return false;
    return IsInteractionVisibleToPlayer(Player);
}

bool ABlackboardPuzzleActor::CanShowFullInteraction(APawn* Player) const
{
    if (!Player || !InteractionBox) return false;
    if (bSolved || PuzzlePhase == EBBPuzzlePhase::WaitingForItem) return false;
    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    if (Dist > InteractionUseDistance) return false;
    return IsInteractionVisibleToPlayer(Player);
}

void ABlackboardPuzzleActor::SetFullWidgetVisible(bool bVisible, APawn*) { if (FullInteractionWidget) FullInteractionWidget->SetVisibility(bVisible); }
FVector ABlackboardPuzzleActor::GetInteractionLocation() const { return InteractionBox ? InteractionBox->GetComponentLocation() : GetActorLocation(); }
void ABlackboardPuzzleActor::DeactivateInteractionCamera() { if (InteractionCamera) InteractionCamera->Deactivate(); }

int32* ABlackboardPuzzleActor::GetGrid(EBlackboardSide Side) { return (Side == EBlackboardSide::Left) ? LeftGrid : RightGrid; }
const int32* ABlackboardPuzzleActor::GetGrid(EBlackboardSide Side) const { return (Side == EBlackboardSide::Left) ? LeftGrid : RightGrid; }

FVector ABlackboardPuzzleActor::GetSlotWorldPosition(EBlackboardSide Side, int32 Slot, bool bHovering) const
{
    const USceneComponent* Origin = (Side == EBlackboardSide::Left) ? LeftGridOrigin : RightGridOrigin;
    if (!Origin) return GetActorLocation();
    const int32 Row = Slot / 3; const int32 Col = Slot % 3;
    FVector Pos = Origin->GetComponentLocation();
    Pos -= Origin->GetRightVector() * Col * GridCellSpacing;
    Pos -= Origin->GetUpVector() * Row * GridCellSpacing;
    if (bHovering) Pos += Origin->GetForwardVector() * HoverOffset;
    return Pos;
}

FRotator ABlackboardPuzzleActor::GetPieceWorldRotation(int32 RotSteps) const
{
    const USceneComponent* Ref = RightGridOrigin ? RightGridOrigin : Root;
    FQuat GridQ = Ref->GetComponentRotation().Quaternion();
    FVector BoardNormal = Ref->GetForwardVector();
    FQuat StepQ(BoardNormal, FMath::DegreesToRadians(RotSteps * 90.f));
    return (StepQ * GridQ).Rotator();
}

void ABlackboardPuzzleActor::PlacePieceMeshAtSlot(int32 PieceIdx, EBlackboardSide Side, int32 Slot, bool bHovering)
{
    if (!PiecePivots.IsValidIndex(PieceIdx) || !PieceMeshComps.IsValidIndex(PieceIdx)) return;
    USceneComponent* Pivot = PiecePivots[PieceIdx]; UStaticMeshComponent* Comp = PieceMeshComps[PieceIdx];
    if (!Pivot || !Comp) return;
    Pivot->SetWorldLocation(GetSlotWorldPosition(Side, Slot, bHovering));
    Pivot->SetWorldRotation(GetPieceWorldRotation(PieceRotations[PieceIdx]));
    Comp->SetVisibility(true);
}

int32 ABlackboardPuzzleActor::FindFirstEmptySlot(EBlackboardSide Side) const { const int32* Grid = GetGrid(Side); for (int32 i=0;i<9;++i) { if (Grid[i]<0) return i; } return -1; }

int32 ABlackboardPuzzleActor::FindNearestOccupied(EBlackboardSide Side, int32 PreferredSlot) const
{
    const int32* Grid = GetGrid(Side);
    if (PreferredSlot >= 0 && PreferredSlot < 9 && Grid[PreferredSlot] >= 0) return PreferredSlot;
    int32 Best = -1; int32 BestDist = INT_MAX;
    const int32 PrefRow = PreferredSlot / 3; const int32 PrefCol = PreferredSlot % 3;
    for (int32 i = 0; i < 9; ++i) { if (Grid[i]<0) continue; const int32 D = FMath::Abs(i/3-PrefRow)+FMath::Abs(i%3-PrefCol); if (D<BestDist){BestDist=D;Best=i;} }
    return Best;
}

void ABlackboardPuzzleActor::CreatePieceDynamicMaterials()
{
    PieceDynMaterials.SetNum(9);
    for (int32 i=0;i<9;++i) { if (!PieceMeshComps.IsValidIndex(i)||!PieceMeshComps[i]) continue; UMaterialInstanceDynamic* DynMat=PieceMeshComps[i]->CreateDynamicMaterialInstance(0); if(DynMat) DynMat->SetScalarParameterValue(EmissiveParamName,0.f); PieceDynMaterials[i]=DynMat; }
}

void ABlackboardPuzzleActor::UpdateHighlight()
{
    ClearAllHighlights();
    if (PuzzlePhase != EBBPuzzlePhase::Active) return;
    const int32* Grid = GetGrid(CurrentSide); const int32 PieceAtSlot = Grid[CurrentSlot];
    int32 HighlightPiece = -1;
    if (NavState == EBBNavState::Holding) HighlightPiece = HeldPieceIndex;
    else if (PieceAtSlot >= 0) HighlightPiece = PieceAtSlot;
    if (HighlightPiece >= 0 && PieceDynMaterials.IsValidIndex(HighlightPiece) && PieceDynMaterials[HighlightPiece])
        PieceDynMaterials[HighlightPiece]->SetScalarParameterValue(EmissiveParamName, HighlightEmissiveStrength);
}

void ABlackboardPuzzleActor::ClearAllHighlights() { for (int32 i=0;i<9;++i) { if (PieceDynMaterials.IsValidIndex(i)&&PieceDynMaterials[i]) PieceDynMaterials[i]->SetScalarParameterValue(EmissiveParamName,0.f); } }

void ABlackboardPuzzleActor::ActivatePuzzle()
{
    PuzzlePhase = EBBPuzzlePhase::Active;
    TArray<int32> Indices; for (int32 i=0;i<9;++i) Indices.Add(i);
    for (int32 i=Indices.Num()-1;i>0;--i) { int32 j=FMath::RandRange(0,i); Indices.Swap(i,j); }
    for (int32 Slot=0;Slot<9;++Slot) { LeftGrid[Slot]=Indices[Slot]; RightGrid[Slot]=-1; }
    for (int32 i=0;i<9;++i) PieceRotations[i]=FMath::RandRange(0,3);
    for (int32 Slot=0;Slot<9;++Slot) PlacePieceMeshAtSlot(LeftGrid[Slot], EBlackboardSide::Left, Slot, false);
    CurrentSide=EBlackboardSide::Left; CurrentSlot=FindNearestOccupied(EBlackboardSide::Left,0); NavState=EBBNavState::Browsing; HeldPieceIndex=-1;
    UpdateHighlight();
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1,4.0f,FColor::Cyan,TEXT("Puzzle started! [WASD] navigate  [E] pick/place  [R] rotate  [Q] cancel"));
}

void ABlackboardPuzzleActor::Navigate(int32 DRow, int32 DCol)
{
    if (PuzzlePhase != EBBPuzzlePhase::Active) return;
    if (NavState == EBBNavState::Holding)
    {
        int32 Row=FMath::Clamp(CurrentSlot/3+DRow,0,2); int32 Col=FMath::Clamp(CurrentSlot%3+DCol,0,2); CurrentSlot=Row*3+Col;
        if (HeldPieceIndex>=0) PlacePieceMeshAtSlot(HeldPieceIndex,EBlackboardSide::Right,CurrentSlot,true);
        UpdateHighlight(); return;
    }
    const int32 CurRow=CurrentSlot/3; const int32 CurCol=CurrentSlot%3; const int32* Grid=GetGrid(CurrentSide);
    int32 BestSlot=-1; int32 BestDist=INT_MAX;
    for (int32 i=0;i<9;++i)
    {
        if (i==CurrentSlot||Grid[i]<0) continue;
        const int32 R=i/3; const int32 C=i%3; const int32 DR=R-CurRow; const int32 DC=C-CurCol;
        bool bValid=true;
        if (DRow!=0&&(DRow>0?DR<=0:DR>=0)) bValid=false;
        if (DCol!=0&&(DCol>0?DC<=0:DC>=0)) bValid=false;
        if (!bValid) continue;
        const int32 Dist=FMath::Abs(DR)+FMath::Abs(DC);
        if (Dist<BestDist){BestDist=Dist;BestSlot=i;}
    }
    if (BestSlot>=0){CurrentSlot=BestSlot;UpdateHighlight();return;}
    if (DCol>0)
    {
        int32 Target=FindNearestOccupied((CurrentSide==EBlackboardSide::Left)?EBlackboardSide::Right:EBlackboardSide::Left,CurRow*3+0);
        if (Target>=0){CurrentSide=(CurrentSide==EBlackboardSide::Left)?EBlackboardSide::Right:EBlackboardSide::Left;CurrentSlot=Target;UpdateHighlight();}
    }
    else if (DCol<0)
    {
        int32 Target=FindNearestOccupied((CurrentSide==EBlackboardSide::Right)?EBlackboardSide::Left:EBlackboardSide::Right,CurRow*3+2);
        if (Target>=0){CurrentSide=(CurrentSide==EBlackboardSide::Right)?EBlackboardSide::Left:EBlackboardSide::Right;CurrentSlot=Target;UpdateHighlight();}
    }
}

void ABlackboardPuzzleActor::InteractPiece()
{
    if (PuzzlePhase != EBBPuzzlePhase::Active) return;
    if (NavState == EBBNavState::Browsing)
    {
        int32* Grid=GetGrid(CurrentSide); const int32 PieceIdx=Grid[CurrentSlot]; if (PieceIdx<0) return;
        HeldPieceIndex=PieceIdx; HeldOriginSlot=CurrentSlot; HeldOriginSide=CurrentSide; Grid[CurrentSlot]=-1;
        if (CurrentSide==EBlackboardSide::Left) { int32 EmptySlot=FindFirstEmptySlot(EBlackboardSide::Right); if (EmptySlot<0) EmptySlot=0; CurrentSide=EBlackboardSide::Right; CurrentSlot=EmptySlot; }
        NavState=EBBNavState::Holding;
        PlacePieceMeshAtSlot(HeldPieceIndex,EBlackboardSide::Right,CurrentSlot,true); UpdateHighlight(); return;
    }
    if (NavState == EBBNavState::Holding && HeldPieceIndex >= 0)
    {
        int32 ExistingPiece=RightGrid[CurrentSlot]; RightGrid[CurrentSlot]=HeldPieceIndex;
        PlacePieceMeshAtSlot(HeldPieceIndex,EBlackboardSide::Right,CurrentSlot,false);
        if (ExistingPiece>=0)
        {
            if (HeldOriginSide==EBlackboardSide::Left){LeftGrid[HeldOriginSlot]=ExistingPiece;PlacePieceMeshAtSlot(ExistingPiece,EBlackboardSide::Left,HeldOriginSlot,false);}
            else{RightGrid[HeldOriginSlot]=ExistingPiece;PlacePieceMeshAtSlot(ExistingPiece,EBlackboardSide::Right,HeldOriginSlot,false);}
        }
        HeldPieceIndex=-1; NavState=EBBNavState::Browsing;
        int32 LeftFirst=FindNearestOccupied(EBlackboardSide::Left,0);
        if (LeftFirst>=0){CurrentSide=EBlackboardSide::Left;CurrentSlot=LeftFirst;}
        else{int32 RightFirst=FindNearestOccupied(EBlackboardSide::Right,0);if(RightFirst>=0){CurrentSide=EBlackboardSide::Right;CurrentSlot=RightFirst;}}
        UpdateHighlight();
        if (CheckSolution()) OnPuzzleSolved();
        return;
    }
}

void ABlackboardPuzzleActor::RotateHeldPiece()
{
    if (NavState!=EBBNavState::Holding||HeldPieceIndex<0) return;
    PieceRotations[HeldPieceIndex]=(PieceRotations[HeldPieceIndex]+1)%4;
    PlacePieceMeshAtSlot(HeldPieceIndex,EBlackboardSide::Right,CurrentSlot,true);
}

bool ABlackboardPuzzleActor::CancelAction()
{
    if (NavState==EBBNavState::Holding&&HeldPieceIndex>=0)
    {
        int32* OriginGrid=GetGrid(HeldOriginSide); OriginGrid[HeldOriginSlot]=HeldPieceIndex;
        PlacePieceMeshAtSlot(HeldPieceIndex,HeldOriginSide,HeldOriginSlot,false);
        HeldPieceIndex=-1; NavState=EBBNavState::Browsing; CurrentSide=HeldOriginSide; CurrentSlot=HeldOriginSlot; UpdateHighlight();
        return true;
    }
    return false;
}

bool ABlackboardPuzzleActor::CheckSolution() const
{
    for (int32 Slot=0;Slot<9;++Slot) { if (RightGrid[Slot]!=Slot) return false; if (PieceRotations[Slot]!=0) return false; }
    return true;
}

void ABlackboardPuzzleActor::OnPuzzleSolved()
{
    bSolved=true; PuzzlePhase=EBBPuzzlePhase::Solved; ClearAllHighlights();
    for (UStaticMeshComponent* Comp:PieceMeshComps) { if(Comp) Comp->SetVisibility(false); }
    if (AssembledMeshComp)
    {
        if (AssembledMesh) AssembledMeshComp->SetStaticMesh(AssembledMesh);
        AssembledMeshComp->SetMobility(EComponentMobility::Movable);
        AssembledMeshComp->bHiddenInGame=false;
        FVector CenterPos=GetSlotWorldPosition(EBlackboardSide::Right,4,false);
        AssembledMeshComp->SetWorldLocation(CenterPos);
        FRotator GridRot=GetPieceWorldRotation(0);
        FQuat FinalQ=GridRot.Quaternion()*PieceMeshBaseRotation.Quaternion();
        AssembledMeshComp->SetWorldRotation(FinalQ.Rotator());
        AssembledMeshComp->SetVisibility(true);
    }
    if (ArrowWidget) ArrowWidget->SetVisibility(false);
    if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false);
    GetWorld()->GetTimerManager().SetTimer(SolveDelayTimerHandle,this,&ABlackboardPuzzleActor::OnSolveDelayFinished,SolveViewDelay,false);
}

void ABlackboardPuzzleActor::OnSolveDelayFinished() { if (CallbackCharacter) CallbackCharacter->EndBlackboardInteraction(true); }