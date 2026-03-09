// DoorActor.cpp
#include "DoorActor.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Camera/CameraComponent.h"

ADoorActor::ADoorActor()
{
	PrimaryActorTick.bCanEverTick = true;

	/* ===== Components ===== */

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	HingeAxis = CreateDefaultSubobject<USceneComponent>(TEXT("HingeAxis"));
	HingeAxis->SetupAttachment(Root);

	DoorLeaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorLeaf"));
	DoorLeaf->SetupAttachment(HingeAxis);

	BlockSensor = CreateDefaultSubobject<UBoxComponent>(TEXT("BlockSensor"));
	BlockSensor->SetupAttachment(HingeAxis);

	Handle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Handle"));
	Handle->SetupAttachment(HingeAxis);

	Hinge = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(HingeAxis);

	Backplate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Backplate"));
	Backplate->SetupAttachment(HingeAxis);

	/* ===== Defaults ===== */

	OpenAngle = 90.f;
	OpenSpeed = 120.f;
	InteractionMaxDistance = 250.f; // arrow range (longer)
	InteractionUseDistance = 100.f; // use/interact range (shorter)
	AutoCloseDistance = 600.f;
	AutoCloseDelay = 2.5f;

	bIsOpen = false;
	bIsMoving = false;
	bBlockedByPlayer = false;

	CurrentYaw = 0.f;
	LastTimePlayerNearby = -9999.f;

	/* ===== Collision ===== */

	DoorLeaf->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DoorLeaf->SetCollisionResponseToAllChannels(ECR_Block);
	DoorLeaf->SetGenerateOverlapEvents(true);

	Handle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Hinge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Backplate->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bWaitingForUnblock = false;
	BlockedTime = 0.f;
	UnblockDelay = 0.25f; // default

	// FRONT interaction (arrow + full)
	InteractionBox_Front = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox_Front"));
	InteractionBox_Front->SetupAttachment(HingeAxis);
	InteractionBox_Front->SetBoxExtent(FVector(12.f, 24.f, 40.f));
	InteractionBox_Front->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionBox_Front->SetGenerateOverlapEvents(false);

	ArrowWidget_Front = CreateDefaultSubobject<UWidgetComponent>(TEXT("ArrowWidget_Front"));
	ArrowWidget_Front->SetupAttachment(InteractionBox_Front);
	ArrowWidget_Front->SetWidgetSpace(EWidgetSpace::World);
	ArrowWidget_Front->SetDrawAtDesiredSize(true);
	ArrowWidget_Front->SetVisibility(false);
	ArrowWidget_Front->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionWidget_Front = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget_Front"));
	InteractionWidget_Front->SetupAttachment(InteractionBox_Front);
	InteractionWidget_Front->SetWidgetSpace(EWidgetSpace::World);
	InteractionWidget_Front->SetDrawAtDesiredSize(true);
	InteractionWidget_Front->SetVisibility(false);
	InteractionWidget_Front->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// BACK interaction (arrow + full)
	InteractionBox_Back = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox_Back"));
	InteractionBox_Back->SetupAttachment(HingeAxis);
	InteractionBox_Back->SetBoxExtent(FVector(12.f, 24.f, 40.f));
	InteractionBox_Back->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionBox_Back->SetGenerateOverlapEvents(false);

	ArrowWidget_Back = CreateDefaultSubobject<UWidgetComponent>(TEXT("ArrowWidget_Back"));
	ArrowWidget_Back->SetupAttachment(InteractionBox_Back);
	ArrowWidget_Back->SetWidgetSpace(EWidgetSpace::World);
	ArrowWidget_Back->SetDrawAtDesiredSize(true);
	ArrowWidget_Back->SetVisibility(false);
	ArrowWidget_Back->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionWidget_Back = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget_Back"));
	InteractionWidget_Back->SetupAttachment(InteractionBox_Back);
	InteractionWidget_Back->SetWidgetSpace(EWidgetSpace::World);
	InteractionWidget_Back->SetDrawAtDesiredSize(true);
	InteractionWidget_Back->SetVisibility(false);
	InteractionWidget_Back->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SymbolMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SymbolMesh"));
	SymbolMeshComponent->SetupAttachment(HingeAxis);

	SymbolMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SymbolMeshComponent->SetGenerateOverlapEvents(false);

	// default index
	SelectedSymbolIndex = 0;
	CurrentSymbolIndex = -1; // not initialized so we will set it in BeginPlay

	// create interaction cameras (front & back)
	InteractionCamera_Front = CreateDefaultSubobject<UCameraComponent>(TEXT("InteractionCamera_Front"));
	InteractionCamera_Front->SetupAttachment(HingeAxis);
	InteractionCamera_Front->bAutoActivate = false;

	InteractionCamera_Back = CreateDefaultSubobject<UCameraComponent>(TEXT("InteractionCamera_Back"));
	InteractionCamera_Back->SetupAttachment(HingeAxis);
	InteractionCamera_Back->bAutoActivate = false;

	KeyInsertPoint = CreateDefaultSubobject<USceneComponent>(TEXT("KeyInsertPoint"));
	KeyInsertPoint->SetupAttachment(HingeAxis); // or Root if you don't want it to move with hinge

	ArrowWidget_Front->SetRenderCustomDepth(true);
// Repeat for all four widget components:
	ArrowWidget_Back->SetRenderCustomDepth(true);
	InteractionWidget_Front->SetRenderCustomDepth(true);
	InteractionWidget_Back->SetRenderCustomDepth(true);

	ArrowWidget_Front->SetBlendMode(EWidgetBlendMode::Transparent);
	ArrowWidget_Back->SetBlendMode(EWidgetBlendMode::Transparent);
	InteractionWidget_Front->SetBlendMode(EWidgetBlendMode::Transparent);
	InteractionWidget_Back->SetBlendMode(EWidgetBlendMode::Transparent);
}

void ADoorActor::BeginPlay()
{
    Super::BeginPlay();

    BlockSensor->OnComponentBeginOverlap.AddDynamic(this, &ADoorActor::OnDoorOverlapBegin);
    BlockSensor->OnComponentEndOverlap.AddDynamic(this, &ADoorActor::OnDoorOverlapEnd);

    // Apply the designer-selected symbol (SelectedSymbolIndex) to the runtime index.
    // This ensures CurrentSymbolIndex is valid instead of staying -1.
    if (!SetSymbolByIndex(SelectedSymbolIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("DoorActor::BeginPlay - failed to set symbol index %d on %s. Check SelectedSymbolIndex and SymbolMeshes array."), SelectedSymbolIndex, *GetName());
    }

    // initial locked state
    bLocked = bStartsLocked;
}

void ADoorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	/* ===== Door rotation ===== */

	if (bIsMoving)
	{
		// Fully blocked
		if (bBlockedByPlayer)
			return;

		// Recently unblocked → wait before resuming
		if (bWaitingForUnblock)
		{
			const float Now = GetWorld()->GetTimeSeconds();
			if ((Now - BlockedTime) < UnblockDelay)
			{
				return;
			}

			// Delay expired
			bWaitingForUnblock = false;
		}

		// ✅ Safe to rotate
		const float TargetYaw = bIsOpen ? 0.f : OpenAngle;

		CurrentYaw = FMath::FInterpConstantTo(CurrentYaw, TargetYaw, DeltaTime, OpenSpeed);

		FRotator NewRot = HingeAxis->GetRelativeRotation();
		NewRot.Yaw = CurrentYaw;
		HingeAxis->SetRelativeRotation(NewRot);

		if (FMath::IsNearlyEqual(CurrentYaw, TargetYaw, 0.5f))
		{
			bIsMoving = false;
			bIsOpen = !bIsOpen;

			// Movement finished → reset
			bShouldBlockDuringThisMove = false;
		}
	}

	/* ===== Auto-close ===== */

	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player)
		return;

	const float Distance = FVector::Dist(Player->GetActorLocation(), HingeAxis->GetComponentLocation());
	const float TimeNow = GetWorld()->GetTimeSeconds();

	if (Distance <= AutoCloseDistance)
	{
		LastTimePlayerNearby = TimeNow;
	}

	if (bIsOpen && !bIsMoving && !bBlockedByPlayer)
	{
		if ((TimeNow - LastTimePlayerNearby) >= AutoCloseDelay)
		{
			ToggleDoor();
		}
	}

	/* ===== Arrow widget visibility (distance-based side selection) ===== */

	// Arrow widgets: show if this door satisfies the arrow rule (longer range)
	const bool bArrowShouldShow = CanShowInteraction(Player);

	// Choose which side is active for arrow (closest box)
	UBoxComponent* ActiveBox = GetActiveInteractionBox(Player);
	bool bFrontIsActive = (ActiveBox == InteractionBox_Front);

	// Set arrow visibility by side
	if (ArrowWidget_Front) ArrowWidget_Front->SetVisibility(bArrowShouldShow && bFrontIsActive);
	if (ArrowWidget_Back)  ArrowWidget_Back->SetVisibility(bArrowShouldShow && !bFrontIsActive);

	// NOTE: full widgets (InteractionWidget_* ) are controlled externally by character logic

	// ===== Rotate interaction widgets to face the player =====
	APawn* LocalPlayer = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (LocalPlayer)
	{
		APlayerController* PC = Cast<APlayerController>(LocalPlayer->GetController());
		if (PC && PC->PlayerCameraManager)
		{
			FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();

			// Helper lambda to rotate a widget component toward the camera
			auto FaceCamera = [&](UWidgetComponent* Widget)
			{
				if (!Widget || !Widget->IsVisible()) return;

				FVector ToCamera = (CameraLocation - Widget->GetComponentLocation()).GetSafeNormal();
				FRotator FaceRot = ToCamera.Rotation();
				FaceRot.Pitch = 0.f; // ignore up/down tilt
				Widget->SetWorldRotation(FaceRot);
			};

			FaceCamera(ArrowWidget_Front);
			FaceCamera(ArrowWidget_Back);
			FaceCamera(InteractionWidget_Front);
			FaceCamera(InteractionWidget_Back);
		}
	}
}

void ADoorActor::ToggleDoor()
{
	if (bIsMoving)
		return;

	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);

	// Decide ONCE whether blocking applies for this movement
	bShouldBlockDuringThisMove = IsMovingTowardsPlayer(Player);

	// Reset block state
	bBlockedByPlayer = false;
	bWaitingForUnblock = false;

	bIsMoving = true;
}

/* ===== Overlap handling ===== */

void ADoorActor::OnDoorOverlapBegin(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	if (!bShouldBlockDuringThisMove)
		return;

	if (OtherActor && OtherActor->IsA<ACharacter>())
	{
		bBlockedByPlayer = true;
		bWaitingForUnblock = false;
	}
}

void ADoorActor::OnDoorOverlapEnd(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32)
{
	if (!bShouldBlockDuringThisMove)
		return;

	if (OtherActor && OtherActor->IsA<ACharacter>())
	{
		bBlockedByPlayer = false;
		bWaitingForUnblock = true;
		BlockedTime = GetWorld()->GetTimeSeconds();
	}
}

bool ADoorActor::IsMovingTowardsPlayer(const AActor* Player) const
{
	if (!Player)
		return false;

	// Direction from hinge to player
	const FVector ToPlayer = (Player->GetActorLocation() - HingeAxis->GetComponentLocation()).GetSafeNormal();

	// Door swing direction (right vector is yaw positive)
	FVector DoorSwingDir = HingeAxis->GetRightVector();

	// If closing, reverse swing direction
	const bool bOpening = !bIsOpen;
	if (!bOpening)
	{
		DoorSwingDir *= -1.f;
	}

	// Dot > 0 → door moving towards player
	const float Dot = FVector::DotProduct(DoorSwingDir, ToPlayer);
	return Dot > 0.1f; // tolerance avoids edge jitter
}

bool ADoorActor::IsInteractionBoxOnScreen(APawn* Player, UBoxComponent* Box) const
{
	if (!Player || !Box)
		return false;

	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC)
		return false;

	// Project the box center to screen space
	FVector WorldLocation = Box->GetComponentLocation();
	FVector2D ScreenPosition;
	const bool bProjected = PC->ProjectWorldLocationToScreen(WorldLocation, ScreenPosition, true);

	if (!bProjected)
		return false;

	int32 SizeX = 0, SizeY = 0;
	PC->GetViewportSize(SizeX, SizeY);

	// Simple check: inside viewport bounds
	return ScreenPosition.X >= 0.f && ScreenPosition.X <= SizeX && ScreenPosition.Y >= 0.f && ScreenPosition.Y <= SizeY;
}

bool ADoorActor::CanShowInteraction(APawn* Player) const
{
	if (!Player)
		return false;

	// Don't show while door is moving
	if (bIsMoving)
		return false;

	// Use arrow-range distance & on-screen check
	UBoxComponent* Box = GetActiveInteractionBox(Player);
	if (!Box)
		return false;

	const float Distance = FVector::Dist(Player->GetActorLocation(), Box->GetComponentLocation());
	if (Distance > InteractionMaxDistance)
		return false;

	if (!IsInteractionBoxOnScreen(Player, Box))
		return false;

	return true;
}

bool ADoorActor::CanShowFullInteraction(APawn* Player) const
{
	if (!Player)
		return false;

	// Don't show while door is moving
	if (bIsMoving)
		return false;

	// Must be closer (use InteractionUseDistance)
	UBoxComponent* Box = GetActiveInteractionBox(Player);
	if (!Box)
		return false;

	const float Distance = FVector::Dist(Player->GetActorLocation(), Box->GetComponentLocation());
	if (Distance > InteractionUseDistance)
		return false;

	if (!IsInteractionBoxOnScreen(Player, Box))
		return false;

	return true;
}

void ADoorActor::SetFullWidgetVisible(bool bVisible, const APawn* Player)
{
	// Only affect *full* widgets. Arrow widgets are managed in Tick.
	if (!InteractionWidget_Front || !InteractionWidget_Back)
		return;

	// Hide both first
	InteractionWidget_Front->SetVisibility(false);
	InteractionWidget_Back->SetVisibility(false);

	if (!bVisible || !Player)
		return;

	// Show the full widget on the side closest to the player
	UWidgetComponent* ActiveFull = GetActiveInteractionWidget(Player);
	if (ActiveFull)
	{
		ActiveFull->SetVisibility(true);
	}
}

UBoxComponent* ADoorActor::GetActiveInteractionBox(const APawn* Player) const
{
	if (!Player)
	{
		// default to front if player missing
		return InteractionBox_Front ? InteractionBox_Front : InteractionBox_Back;
	}

	// If either box missing, return the existing one
	if (!InteractionBox_Front) return InteractionBox_Back;
	if (!InteractionBox_Back)  return InteractionBox_Front;

	// Choose the closer box to the player
	const float DistFront = FVector::Dist(Player->GetActorLocation(), InteractionBox_Front->GetComponentLocation());
	const float DistBack  = FVector::Dist(Player->GetActorLocation(), InteractionBox_Back->GetComponentLocation());

	return (DistFront <= DistBack) ? InteractionBox_Front : InteractionBox_Back;
}

UWidgetComponent* ADoorActor::GetActiveInteractionWidget(const APawn* Player) const
{
	// Map box → full widget (safe checks)
	UBoxComponent* ActiveBox = GetActiveInteractionBox(Player);
	if (ActiveBox == InteractionBox_Front) return InteractionWidget_Front;
	if (ActiveBox == InteractionBox_Back)  return InteractionWidget_Back;

	// fallback
	return InteractionWidget_Front ? InteractionWidget_Front : InteractionWidget_Back;
}


bool ADoorActor::SetSymbolByIndex(int32 Index)
{
    if (!SymbolMeshComponent)
        return false;

    if (!SymbolMeshes.IsValidIndex(Index))
    {
        // If invalid, clear mesh
        SymbolMeshComponent->SetStaticMesh(nullptr);
        CurrentSymbolIndex = -1;
        return false;
    }

    UStaticMesh* Mesh = SymbolMeshes[Index];
    if (!Mesh)
    {
        SymbolMeshComponent->SetStaticMesh(nullptr);
        CurrentSymbolIndex = -1;
        return false;
    }

    SymbolMeshComponent->SetStaticMesh(Mesh);
    CurrentSymbolIndex = Index;
    return true;
}

UStaticMesh* ADoorActor::GetCurrentSymbolMesh() const
{
    if (SymbolMeshes.IsValidIndex(CurrentSymbolIndex))
        return SymbolMeshes[CurrentSymbolIndex];

    return nullptr;
}

void ADoorActor::UnlockDoor()
{
    bLocked = false;

    // TODO: add feedback (sound / material change). For now, log:
    UE_LOG(LogTemp, Log, TEXT("Door %s unlocked."), *GetName());
}

UCameraComponent* ADoorActor::GetActiveInteractionCamera(const APawn* Player) const
{
    if (!Player)
    {
        // fallback: prefer front, then back
        return InteractionCamera_Front ? InteractionCamera_Front : InteractionCamera_Back;
    }

    UBoxComponent* ActiveBox = GetActiveInteractionBox(Player);
    if (ActiveBox == InteractionBox_Front)
    {
        return InteractionCamera_Front ? InteractionCamera_Front : InteractionCamera_Back;
    }
    else
    {
        return InteractionCamera_Back ? InteractionCamera_Back : InteractionCamera_Front;
    }
}

void ADoorActor::DeactivateInteractionCameras()
{
    if (InteractionCamera_Front) InteractionCamera_Front->Deactivate();
    if (InteractionCamera_Back) InteractionCamera_Back->Deactivate();
}