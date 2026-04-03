// DoorActor.cpp
#include "DoorActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

#include "InteractableUtils.h"


ADoorActor::ADoorActor()
{
    PrimaryActorTick.bCanEverTick = true;

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

    OpenAngle = 90.f; OpenSpeed = 120.f;
    InteractionMaxDistance = 250.f; InteractionUseDistance = 100.f;
    AutoCloseDistance = 600.f; AutoCloseDelay = 2.5f;
    bIsOpen = false; bIsMoving = false; bBlockedByPlayer = false;
    CurrentYaw = 0.f; LastTimePlayerNearby = -9999.f;

    DoorLeaf->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    DoorLeaf->SetCollisionResponseToAllChannels(ECR_Block);
    DoorLeaf->SetGenerateOverlapEvents(true);
    Handle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Hinge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Backplate->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    bWaitingForUnblock = false; BlockedTime = 0.f; UnblockDelay = 0.25f;

    InteractionBox_Front = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox_Front"));
    InteractionBox_Front->SetupAttachment(HingeAxis);
    InteractionBox_Front->SetBoxExtent(FVector(12.f, 24.f, 40.f));
    InteractionBox_Front->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    InteractionBox_Front->SetGenerateOverlapEvents(false);

    ArrowWidget_Front = CreateDefaultSubobject<UWidgetComponent>(TEXT("ArrowWidget_Front"));
    ArrowWidget_Front->SetupAttachment(InteractionBox_Front);
    ArrowWidget_Front->SetWidgetSpace(EWidgetSpace::World); ArrowWidget_Front->SetDrawAtDesiredSize(true);
    ArrowWidget_Front->SetVisibility(false); ArrowWidget_Front->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    InteractionWidget_Front = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget_Front"));
    InteractionWidget_Front->SetupAttachment(InteractionBox_Front);
    InteractionWidget_Front->SetWidgetSpace(EWidgetSpace::World); InteractionWidget_Front->SetDrawAtDesiredSize(true);
    InteractionWidget_Front->SetVisibility(false); InteractionWidget_Front->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    InteractionBox_Back = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox_Back"));
    InteractionBox_Back->SetupAttachment(HingeAxis);
    InteractionBox_Back->SetBoxExtent(FVector(12.f, 24.f, 40.f));
    InteractionBox_Back->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    InteractionBox_Back->SetGenerateOverlapEvents(false);

    ArrowWidget_Back = CreateDefaultSubobject<UWidgetComponent>(TEXT("ArrowWidget_Back"));
    ArrowWidget_Back->SetupAttachment(InteractionBox_Back);
    ArrowWidget_Back->SetWidgetSpace(EWidgetSpace::World); ArrowWidget_Back->SetDrawAtDesiredSize(true);
    ArrowWidget_Back->SetVisibility(false); ArrowWidget_Back->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    InteractionWidget_Back = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget_Back"));
    InteractionWidget_Back->SetupAttachment(InteractionBox_Back);
    InteractionWidget_Back->SetWidgetSpace(EWidgetSpace::World); InteractionWidget_Back->SetDrawAtDesiredSize(true);
    InteractionWidget_Back->SetVisibility(false); InteractionWidget_Back->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    SymbolMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SymbolMesh"));
    SymbolMeshComponent->SetupAttachment(HingeAxis);
    SymbolMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SymbolMeshComponent->SetGenerateOverlapEvents(false);
    SelectedSymbolIndex = 0; CurrentSymbolIndex = -1;

    InteractionCamera_Front = CreateDefaultSubobject<UCameraComponent>(TEXT("InteractionCamera_Front"));
    InteractionCamera_Front->SetupAttachment(HingeAxis); InteractionCamera_Front->bAutoActivate = false;
    InteractionCamera_Back = CreateDefaultSubobject<UCameraComponent>(TEXT("InteractionCamera_Back"));
    InteractionCamera_Back->SetupAttachment(HingeAxis); InteractionCamera_Back->bAutoActivate = false;

    KeyInsertPoint = CreateDefaultSubobject<USceneComponent>(TEXT("KeyInsertPoint"));
    KeyInsertPoint->SetupAttachment(HingeAxis);

    ArrowWidget_Front->SetRenderCustomDepth(true); ArrowWidget_Back->SetRenderCustomDepth(true);
    InteractionWidget_Front->SetRenderCustomDepth(true); InteractionWidget_Back->SetRenderCustomDepth(true);
    ArrowWidget_Front->SetBlendMode(EWidgetBlendMode::Transparent); ArrowWidget_Back->SetBlendMode(EWidgetBlendMode::Transparent);
    InteractionWidget_Front->SetBlendMode(EWidgetBlendMode::Transparent); InteractionWidget_Back->SetBlendMode(EWidgetBlendMode::Transparent);
}

void ADoorActor::BeginPlay()
{
    Super::BeginPlay();
    BlockSensor->OnComponentBeginOverlap.AddDynamic(this, &ADoorActor::OnDoorOverlapBegin);
    BlockSensor->OnComponentEndOverlap.AddDynamic(this, &ADoorActor::OnDoorOverlapEnd);
    if (!SetSymbolByIndex(SelectedSymbolIndex))
        UE_LOG(LogTemp, Warning, TEXT("DoorActor::BeginPlay - failed to set symbol index %d on %s."), SelectedSymbolIndex, *GetName());
    bLocked = bStartsLocked;
}

void ADoorActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsMoving)
    {
        if (bBlockedByPlayer) return;
        if (bWaitingForUnblock) { if ((GetWorld()->GetTimeSeconds() - BlockedTime) < UnblockDelay) return; bWaitingForUnblock = false; }
        const float TargetYaw = bIsOpen ? 0.f : OpenAngle;
        CurrentYaw = FMath::FInterpConstantTo(CurrentYaw, TargetYaw, DeltaTime, OpenSpeed);
        FRotator NewRot = HingeAxis->GetRelativeRotation(); NewRot.Yaw = CurrentYaw;
        HingeAxis->SetRelativeRotation(NewRot);
        if (FMath::IsNearlyEqual(CurrentYaw, TargetYaw, 0.5f)) { bIsMoving = false; bIsOpen = !bIsOpen; bShouldBlockDuringThisMove = false; }
    }

    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player) return;

    const float Distance = FVector::Dist(Player->GetActorLocation(), HingeAxis->GetComponentLocation());
    const float TimeNow = GetWorld()->GetTimeSeconds();
    if (Distance <= AutoCloseDistance) LastTimePlayerNearby = TimeNow;
    if (bIsOpen && !bIsMoving && !bBlockedByPlayer && (TimeNow - LastTimePlayerNearby) >= AutoCloseDelay) ToggleDoor();

    // Hide widgets during interaction
    const bool bInDoorInteraction =
        (InteractionCamera_Front && InteractionCamera_Front->IsActive()) ||
        (InteractionCamera_Back && InteractionCamera_Back->IsActive());
    if (bInDoorInteraction)
    {
        if (ArrowWidget_Front) ArrowWidget_Front->SetVisibility(false);
        if (ArrowWidget_Back) ArrowWidget_Back->SetVisibility(false);
        if (InteractionWidget_Front) InteractionWidget_Front->SetVisibility(false);
        if (InteractionWidget_Back) InteractionWidget_Back->SetVisibility(false);
        return;
    }

    const bool bArrowShouldShow = CanShowInteraction(Player);
    UBoxComponent* ActiveBox = GetActiveInteractionBox(Player);
    bool bFrontIsActive = (ActiveBox == InteractionBox_Front);
    if (ArrowWidget_Front) ArrowWidget_Front->SetVisibility(bArrowShouldShow && bFrontIsActive);
    if (ArrowWidget_Back) ArrowWidget_Back->SetVisibility(bArrowShouldShow && !bFrontIsActive);

    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (PC && PC->PlayerCameraManager)
    {
        FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
        if (ArrowWidget_Front && ArrowWidget_Front->IsVisible()) FInteractableUtils::FaceWidgetTowardsCamera(ArrowWidget_Front, CameraLocation);
        if (ArrowWidget_Back && ArrowWidget_Back->IsVisible()) FInteractableUtils::FaceWidgetTowardsCamera(ArrowWidget_Back, CameraLocation);
        if (InteractionWidget_Front && InteractionWidget_Front->IsVisible()) FInteractableUtils::FaceWidgetTowardsCamera(InteractionWidget_Front, CameraLocation);
        if (InteractionWidget_Back && InteractionWidget_Back->IsVisible()) FInteractableUtils::FaceWidgetTowardsCamera(InteractionWidget_Back, CameraLocation);
    }
}

void ADoorActor::ToggleDoor()
{
    if (bIsMoving) return;
    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    bShouldBlockDuringThisMove = IsMovingTowardsPlayer(Player);
    bBlockedByPlayer = false; bWaitingForUnblock = false; bIsMoving = true;
}

void ADoorActor::OnDoorOverlapBegin(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!bShouldBlockDuringThisMove) return;
    if (OtherActor && OtherActor->IsA(ACharacter::StaticClass())) { bBlockedByPlayer = true; bWaitingForUnblock = false; }
}

void ADoorActor::OnDoorOverlapEnd(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
    if (!bShouldBlockDuringThisMove) return;
    if (OtherActor && OtherActor->IsA(ACharacter::StaticClass())) { bBlockedByPlayer = false; bWaitingForUnblock = true; BlockedTime = GetWorld()->GetTimeSeconds(); }
}

bool ADoorActor::IsMovingTowardsPlayer(const AActor* Player) const
{
    if (!Player) return false;
    const FVector ToPlayer = (Player->GetActorLocation() - HingeAxis->GetComponentLocation()).GetSafeNormal();
    FVector DoorSwingDir = HingeAxis->GetRightVector();
    if (bIsOpen) DoorSwingDir *= -1.f;
    return FVector::DotProduct(DoorSwingDir, ToPlayer) > 0.1f;
}

bool ADoorActor::IsInteractionBoxOnScreen(APawn* Player, UBoxComponent* Box) const
{
    if (!Player || !Box) return false;
    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (!PC || !PC->PlayerCameraManager) return false;

    FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
    FVector CameraForward = PC->PlayerCameraManager->GetCameraRotation().Vector();
    FVector ToBox = (Box->GetComponentLocation() - CameraLocation).GetSafeNormal();

    // Facing check
    if (FVector::DotProduct(CameraForward, ToBox) < 0.3f)
        return false;

    // Line-of-sight check
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Player);
    Params.AddIgnoredActor(this);
    if (GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, Box->GetComponentLocation(), ECC_Visibility, Params))
        return false;

    return true;
}

bool ADoorActor::CanShowInteraction(APawn* Player) const
{
    if (!Player) return false;
    if (bIsMoving) return false;
    if (bBlockedByExternalLock) return false;
    UBoxComponent* Box = const_cast<ADoorActor*>(this)->GetActiveInteractionBox(Player);
    if (!Box) return false;
    const float Distance = FVector::Dist(Player->GetActorLocation(), Box->GetComponentLocation());
    if (Distance > InteractionMaxDistance) return false;
    if (!IsInteractionBoxOnScreen(Player, Box)) return false;
    return true;
}

bool ADoorActor::CanShowFullInteraction(APawn* Player) const
{
    if (!Player) return false;
    if (bIsMoving) return false;
    if (bBlockedByExternalLock) return false;
    UBoxComponent* Box = const_cast<ADoorActor*>(this)->GetActiveInteractionBox(Player);
    if (!Box) return false;
    const float Distance = FVector::Dist(Player->GetActorLocation(), Box->GetComponentLocation());
    if (Distance > InteractionUseDistance) return false;
    if (!IsInteractionBoxOnScreen(Player, Box)) return false;
    return true;
}

void ADoorActor::SetFullWidgetVisible(bool bVisible, APawn* Player)
{
    if (!InteractionWidget_Front || !InteractionWidget_Back) return;
    InteractionWidget_Front->SetVisibility(false); InteractionWidget_Back->SetVisibility(false);
    if (!bVisible || !Player) return;
    UWidgetComponent* ActiveFull = GetActiveInteractionWidget(Player);
    if (ActiveFull) ActiveFull->SetVisibility(true);
}

UBoxComponent* ADoorActor::GetActiveInteractionBox(APawn* Player) const
{
    if (!Player) return InteractionBox_Front ? InteractionBox_Front : InteractionBox_Back;
    if (!InteractionBox_Front) return InteractionBox_Back;
    if (!InteractionBox_Back) return InteractionBox_Front;
    const float DistFront = FVector::Dist(Player->GetActorLocation(), InteractionBox_Front->GetComponentLocation());
    const float DistBack = FVector::Dist(Player->GetActorLocation(), InteractionBox_Back->GetComponentLocation());
    return (DistFront <= DistBack) ? InteractionBox_Front : InteractionBox_Back;
}

UWidgetComponent* ADoorActor::GetActiveInteractionWidget(const APawn* Player) const
{
    UBoxComponent* ActiveBox = GetActiveInteractionBox(const_cast<APawn*>(Player));
    if (ActiveBox == InteractionBox_Front) return InteractionWidget_Front;
    if (ActiveBox == InteractionBox_Back) return InteractionWidget_Back;
    return InteractionWidget_Front ? InteractionWidget_Front : InteractionWidget_Back;
}

bool ADoorActor::SetSymbolByIndex(int32 Index)
{
    if (!SymbolMeshComponent) return false;
    if (!SymbolMeshes.IsValidIndex(Index)) { SymbolMeshComponent->SetStaticMesh(nullptr); CurrentSymbolIndex = -1; return false; }
    UStaticMesh* Mesh = SymbolMeshes[Index];
    if (!Mesh) { SymbolMeshComponent->SetStaticMesh(nullptr); CurrentSymbolIndex = -1; return false; }
    SymbolMeshComponent->SetStaticMesh(Mesh); CurrentSymbolIndex = Index; return true;
}

UStaticMesh* ADoorActor::GetCurrentSymbolMesh() const { return SymbolMeshes.IsValidIndex(CurrentSymbolIndex) ? SymbolMeshes[CurrentSymbolIndex] : nullptr; }
void ADoorActor::UnlockDoor() { bLocked = false; }

UCameraComponent* ADoorActor::GetActiveInteractionCamera(const APawn* Player) const
{
    if (!Player) return InteractionCamera_Front ? InteractionCamera_Front : InteractionCamera_Back;
    UBoxComponent* ActiveBox = GetActiveInteractionBox(const_cast<APawn*>(Player));
    if (ActiveBox == InteractionBox_Front) return InteractionCamera_Front ? InteractionCamera_Front : InteractionCamera_Back;
    return InteractionCamera_Back ? InteractionCamera_Back : InteractionCamera_Front;
}

void ADoorActor::DeactivateInteractionCameras()
{
    if (InteractionCamera_Front) InteractionCamera_Front->Deactivate();
    if (InteractionCamera_Back) InteractionCamera_Back->Deactivate();
    if (InteractionCamera) InteractionCamera->Deactivate();
}

FTransform ADoorActor::GetKeyInsertTransform() const { return KeyInsertPoint ? KeyInsertPoint->GetComponentTransform() : GetActorTransform(); }