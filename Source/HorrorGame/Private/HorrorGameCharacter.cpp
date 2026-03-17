// HorrorGameCharacter.cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#include "HorrorGameCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Blueprint/UserWidget.h"
#include "DoorActor.h"
#include "Kismet/GameplayStatics.h"
#include "InventoryComponent.h"
#include "GameFramework/PlayerController.h"
#include "InventoryWidgetBase.h"
#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "KeyActor.h"
#include "Camera/PlayerCameraManager.h"
#include "PCTerminalActor.h"
#include "PickupItemActor.h"
#include "PadlockActor.h"             // ← NEW
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include <limits>

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AHorrorGameCharacter

AHorrorGameCharacter::AHorrorGameCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.f);

	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	InteractionTraceDistance = 250.f;

	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->InitSphereRadius(200.f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECollisionResponse::ECR_Overlap);
    InteractionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);
    InteractionSphere->SetGenerateOverlapEvents(true);
}

void AHorrorGameCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (InventoryWidgetClass)
        InventoryWidgetInstance = CreateWidget<UInventoryWidgetBase>(GetWorld(), InventoryWidgetClass);

	APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    ULocalPlayer* LP = PC->GetLocalPlayer();
    if (!LP) return;

    UEnhancedInputLocalPlayerSubsystem* Subsystem =
        LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!Subsystem) return;

    Subsystem->AddMappingContext(IMC_Gameplay, 0);

    if (InteractionSphere)
        InteractionSphere->SetSphereRadius(InteractionScanRadius);

    GetWorldTimerManager().SetTimer(
        InteractionScanTimerHandle, this,
        &AHorrorGameCharacter::PerformInteractionScan,
        InteractionScanRate, true);
}

void AHorrorGameCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	bool bShouldShowWidget = false;
}

//////////////////////////////////////////////////////////////////////////
// Input

void AHorrorGameCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EI = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EI)
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("EnhancedInputComponent not found on %s"), *GetName());
		return;
	}

	EI->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
	EI->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	EI->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHorrorGameCharacter::Move);
	EI->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHorrorGameCharacter::Look);

	if (InteractAction)
		EI->BindAction(InteractAction, ETriggerEvent::Started, this, &AHorrorGameCharacter::Interact);
	if (InventoryToggleAction)
		EI->BindAction(InventoryToggleAction, ETriggerEvent::Started, this, &AHorrorGameCharacter::ToggleInventory);

	if (InventoryNavUpAction)    EI->BindAction(InventoryNavUpAction,    ETriggerEvent::Started, this, &AHorrorGameCharacter::OnNavUp);
	if (InventoryNavDownAction)  EI->BindAction(InventoryNavDownAction,  ETriggerEvent::Started, this, &AHorrorGameCharacter::OnNavDown);
	if (InventoryNavLeftAction)  EI->BindAction(InventoryNavLeftAction,  ETriggerEvent::Started, this, &AHorrorGameCharacter::OnNavLeft);
	if (InventoryNavRightAction) EI->BindAction(InventoryNavRightAction, ETriggerEvent::Started, this, &AHorrorGameCharacter::OnNavRight);

	if (InventoryUseAction)    EI->BindAction(InventoryUseAction,    ETriggerEvent::Started, this, &AHorrorGameCharacter::UseSelectedItem);
	if (InventoryCancelAction) EI->BindAction(InventoryCancelAction, ETriggerEvent::Started, this, &AHorrorGameCharacter::CancelDoorUnlock);
	if (PickupConfirmAction)   EI->BindAction(PickupConfirmAction,   ETriggerEvent::Started, this, &AHorrorGameCharacter::ConfirmPickupItem);

	// ===== NEW: Padlock dial actions =====
	if (PadlockDialPrevAction)   EI->BindAction(PadlockDialPrevAction,   ETriggerEvent::Started, this, &AHorrorGameCharacter::OnPadlockDialPrev);
	if (PadlockDialNextAction)   EI->BindAction(PadlockDialNextAction,   ETriggerEvent::Started, this, &AHorrorGameCharacter::OnPadlockDialNext);
	if (PadlockRotateUpAction)   EI->BindAction(PadlockRotateUpAction,   ETriggerEvent::Started, this, &AHorrorGameCharacter::OnPadlockRotateUp);
	if (PadlockRotateDownAction) EI->BindAction(PadlockRotateDownAction, ETriggerEvent::Started, this, &AHorrorGameCharacter::OnPadlockRotateDown);
}

void AHorrorGameCharacter::Move(const FInputActionValue& Value)
{
	FVector2D Movement = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Movement.Y);
	AddMovementInput(GetActorRightVector(), Movement.X);
}

void AHorrorGameCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxis = Value.Get<FVector2D>();
	AddControllerYawInput(LookAxis.X);
	AddControllerPitchInput(LookAxis.Y);
}

void AHorrorGameCharacter::Interact()
{
    // ---------- 1. PC terminal ----------
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), APCTerminalActor::StaticClass(), Found);

        APCTerminalActor* Best = nullptr;
        float BestDist = TNumericLimits<float>::Max();
        for (AActor* A : Found)
        {
            APCTerminalActor* P = Cast<APCTerminalActor>(A);
            if (!P || !P->CanShowFullInteraction(this)) continue;
            float D = FVector::Dist(GetActorLocation(), P->GetInteractionLocation());
            if (D < BestDist) { BestDist = D; Best = P; }
        }
        if (Best)
        {
            if (CurrentPCTerminalTarget == Best) EndPCInteraction();
            else BeginPCInteraction(Best);
            return;
        }
    }

    // ---------- 2. Padlock (NEW) ----------
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), APadlockActor::StaticClass(), Found);

        APadlockActor* Best = nullptr;
        float BestDist = TNumericLimits<float>::Max();
        for (AActor* A : Found)
        {
            APadlockActor* P = Cast<APadlockActor>(A);
            if (!P || !P->CanShowFullInteraction(this)) continue;
            float D = FVector::Dist(GetActorLocation(), P->GetInteractionLocation());
            if (D < BestDist) { BestDist = D; Best = P; }
        }
        if (Best)
        {
            BeginPadlockInteraction(Best);
            return;
        }
    }

    // ---------- 3. Pickup item ----------
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), APickupItemActor::StaticClass(), Found);

        APickupItemActor* Best = nullptr;
        float BestDist = TNumericLimits<float>::Max();
        for (AActor* A : Found)
        {
            APickupItemActor* Item = Cast<APickupItemActor>(A);
            if (!Item || !Item->CanShowFullInteraction(this)) continue;
            float D = FVector::Dist(GetActorLocation(), Item->GetInteractionLocation());
            if (D < BestDist) { BestDist = D; Best = Item; }
        }
        if (Best)
        {
            if (CurrentPickupItemTarget == Best) ConfirmPickupItem();
            else BeginItemInteraction(Best);
            return;
        }
    }

    // ---------- 4. Door ----------
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADoorActor::StaticClass(), Found);

        ADoorActor* Best = nullptr;
        float BestDist = TNumericLimits<float>::Max();
        for (AActor* A : Found)
        {
            ADoorActor* Door = Cast<ADoorActor>(A);
            if (!Door || !Door->CanShowFullInteraction(this)) continue;
            UBoxComponent* Box = Door->GetActiveInteractionBox(this);
            if (!Box) continue;
            float D = FVector::Dist(GetActorLocation(), Box->GetComponentLocation());
            if (D < BestDist) { BestDist = D; Best = Door; }
        }
        if (Best)
        {
            if (Best->IsLocked()) BeginDoorUnlockSequence(Best);
            else Best->ToggleDoor();
        }
    }
}

// ===================================================================
// Padlock interaction
// ===================================================================

void AHorrorGameCharacter::BeginPadlockInteraction(APadlockActor* Padlock)
{
    if (!Padlock) return;

    CurrentPadlockTarget = Padlock;

    // Store character reference on padlock so it can call EndPadlockInteraction when animation finishes
    Padlock->CallbackCharacter = this;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    PC->SetIgnoreLookInput(true);

    // Camera
    UCameraComponent* UseCam = Padlock->GetInteractionCamera(this);
    if (UseCam)
    {
        UseCam->Activate(true);
        FViewTargetTransitionParams Params;
        Params.BlendTime = 0.15f;
        Params.BlendFunction = VTBlend_Cubic;
        PC->SetViewTarget(Padlock, Params);
    }

    // Switch IMC
    if (PC->GetLocalPlayer())
    {
        UEnhancedInputLocalPlayerSubsystem* Sub =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
        if (Sub)
        {
            Sub->RemoveMappingContext(IMC_Gameplay);
            Sub->RemoveMappingContext(IMC_InventoryUI);
            Sub->AddMappingContext(IMC_PadlockInteraction, 2);
        }
    }

    PC->bShowMouseCursor = false;
    PC->SetInputMode(FInputModeGameOnly());

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan,
            TEXT("Padlock: [A/D] switch dial  [W/S] rotate  [Q] cancel"));
    }
}

void AHorrorGameCharacter::EndPadlockInteraction()
{
    APlayerController* PC = Cast<APlayerController>(GetController());

    if (CurrentPadlockTarget)
    {
        CurrentPadlockTarget->DeactivateInteractionCamera();
        CurrentPadlockTarget->CallbackCharacter = nullptr;
    }

    if (PC)
    {
        PC->SetViewTargetWithBlend(this, 0.15f, EViewTargetBlendFunction::VTBlend_Cubic, 0.f, true);
        PC->SetIgnoreLookInput(false);

        if (PC->GetLocalPlayer())
        {
            UEnhancedInputLocalPlayerSubsystem* Sub =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
            if (Sub)
            {
                Sub->RemoveMappingContext(IMC_PadlockInteraction);
                Sub->AddMappingContext(IMC_Gameplay, 0);
            }
        }

        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }

    CurrentPadlockTarget = nullptr;
}

void AHorrorGameCharacter::OnPadlockDialPrev()
{
    if (CurrentPadlockTarget) CurrentPadlockTarget->SelectPreviousDial();
}

void AHorrorGameCharacter::OnPadlockDialNext()
{
    if (CurrentPadlockTarget) CurrentPadlockTarget->SelectNextDial();
}

void AHorrorGameCharacter::OnPadlockRotateUp()
{
    if (CurrentPadlockTarget) CurrentPadlockTarget->RotateSelectedDial(+1);
}

void AHorrorGameCharacter::OnPadlockRotateDown()
{
    if (CurrentPadlockTarget) CurrentPadlockTarget->RotateSelectedDial(-1);
}

// ===================================================================

void AHorrorGameCharacter::ToggleInventory()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;
    ULocalPlayer* LP = PC->GetLocalPlayer();
    if (!LP) return;
    UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!Subsystem) return;

    if (!InventoryWidgetInstance && InventoryWidgetClass)
        InventoryWidgetInstance = CreateWidget<UInventoryWidgetBase>(GetWorld(), InventoryWidgetClass);
    if (!InventoryWidgetInstance || !InventoryComponent) return;

    bInventoryOpen = !bInventoryOpen;

    if (bInventoryOpen)
    {
        InventoryWidgetInstance->AddToViewport();
        InventoryWidgetInstance->RefreshInventory(InventoryComponent->Items, InventoryComponent->GetSelectedIndex());
        InventoryComponent->OnInventoryChanged.AddDynamic(this, &AHorrorGameCharacter::OnInventoryChanged);
        Subsystem->RemoveMappingContext(IMC_Gameplay);
        Subsystem->AddMappingContext(IMC_InventoryUI, 1);
        PC->bShowMouseCursor = true;
        FInputModeGameAndUI Mode;
        Mode.SetWidgetToFocus(InventoryWidgetInstance->TakeWidget());
        PC->SetInputMode(Mode);
    }
    else
    {
        InventoryWidgetInstance->RemoveFromParent();
        InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &AHorrorGameCharacter::OnInventoryChanged);
        Subsystem->RemoveMappingContext(IMC_InventoryUI);
        Subsystem->AddMappingContext(IMC_Gameplay, 0);
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }
}

void AHorrorGameCharacter::OnInventoryChanged()
{
    if (!bInventoryOpen || !InventoryWidgetInstance || !InventoryComponent) return;
    InventoryWidgetInstance->RefreshInventory(InventoryComponent->Items, InventoryComponent->GetSelectedIndex());
}

void AHorrorGameCharacter::OnNavUp()    { InventoryComponent->NavigateSelection(-4); }
void AHorrorGameCharacter::OnNavDown()  { InventoryComponent->NavigateSelection(+4); }
void AHorrorGameCharacter::OnNavLeft()  { InventoryComponent->NavigateSelection(-1); }
void AHorrorGameCharacter::OnNavRight() { InventoryComponent->NavigateSelection(+1); }

void AHorrorGameCharacter::BeginDoorUnlockSequence(ADoorActor* Door)
{
    if (!Door) return;
    CurrentDoorUnlockTarget = Door;

    UEnhancedInputLocalPlayerSubsystem* Subsystem = nullptr;
    if (APlayerController* PC_Sub = Cast<APlayerController>(GetController()))
        if (ULocalPlayer* LP = PC_Sub->GetLocalPlayer())
            Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP);

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC) PC->SetIgnoreLookInput(true);

    Door->DeactivateInteractionCameras();
    UCameraComponent* UseCam = Door->GetActiveInteractionCamera(this);
    if (PC && UseCam)
    {
        UseCam->Activate(true);
        if (Door->InteractionCamera) Door->InteractionCamera->Deactivate();
        FViewTargetTransitionParams Params;
        Params.BlendTime = 0.15f;
        Params.BlendFunction = VTBlend_Cubic;
        PC->SetViewTarget(Door, Params);
    }

    if (Subsystem)
    {
        Subsystem->RemoveMappingContext(IMC_Gameplay);
        Subsystem->RemoveMappingContext(IMC_InventoryUI);
        Subsystem->AddMappingContext(IMC_Interaction, 2);
    }

    if (!bInventoryOpen)
    {
        bInventoryOpen = true;
        if (InventoryWidgetInstance)
        {
            InventoryWidgetInstance->AddToViewport();
            InventoryWidgetInstance->RefreshInventory(InventoryComponent->Items, InventoryComponent->GetSelectedIndex());
        }
        InventoryComponent->OnInventoryChanged.AddDynamic(this, &AHorrorGameCharacter::OnInventoryChanged);
    }
}

void AHorrorGameCharacter::EndDoorUnlockSequence(bool bSuccess)
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    UEnhancedInputLocalPlayerSubsystem* Subsystem = nullptr;
    if (PC && PC->GetLocalPlayer())
        Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());

    if (PC) PC->SetControlRotation(GetActorRotation());

    if (bInventoryOpen && InventoryWidgetInstance && InventoryComponent)
    {
        bInventoryOpen = false;
        InventoryWidgetInstance->RemoveFromParent();
        InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &AHorrorGameCharacter::OnInventoryChanged);
    }

    if (CurrentDoorUnlockTarget) CurrentDoorUnlockTarget->DeactivateInteractionCameras();

    if (Subsystem)
    {
        Subsystem->RemoveMappingContext(IMC_Interaction);
        Subsystem->AddMappingContext(IMC_Gameplay, 0);
    }

    if (PC)
    {
        PC->SetViewTargetWithBlend(this, 0.15f, EViewTargetBlendFunction::VTBlend_Cubic, 0.f, true);
        PC->SetIgnoreLookInput(false);
    }

    CurrentDoorUnlockTarget = nullptr;
}

void AHorrorGameCharacter::UseSelectedItem()
{
    if (!bInventoryOpen) return;
    if (!InventoryComponent) { UE_LOG(LogTemplateCharacter, Warning, TEXT("UseSelectedItem: InventoryComponent missing")); return; }
    if (!CurrentDoorUnlockTarget) { UE_LOG(LogTemplateCharacter, Log, TEXT("UseSelectedItem: no door target")); return; }

    const int32 Sel = InventoryComponent->GetSelectedIndex();
    if (!InventoryComponent->Items.IsValidIndex(Sel))
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("No item selected"));
        return;
    }

    const FInventoryItem& Item = InventoryComponent->Items[Sel];
    const int32 DoorSymbol = CurrentDoorUnlockTarget->GetCurrentSymbolIndex();

    if (Item.KeyIndex != DoorSymbol)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Red, TEXT("Not the right item"));
        return;
    }

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Green, TEXT("Right item!"));

    FTransform TargetTransform = CurrentDoorUnlockTarget->GetKeyInsertTransform();
    if (!TargetTransform.Equals(FTransform::Identity, 0.0001f) == false) {}

    const FVector PlayerWorldLocation = GetActorLocation();
    const FVector PlayerLocal = TargetTransform.InverseTransformPosition(PlayerWorldLocation);
    const float DistanceFromHole = 20.0f;
    const float SideSign = (PlayerLocal.Y >= 0.f) ? +1.f : -1.f;
    const float StopBeforeHole = 3.0f;

    const FVector LocalEndOffset(0.f, SideSign * StopBeforeHole, 0.f);
    TargetTransform.SetLocation(TargetTransform.GetLocation() + TargetTransform.TransformVector(LocalEndOffset));

    const FVector LocalOffset(0.f, SideSign * DistanceFromHole, 0.f);
    FTransform StartTransform = TargetTransform;
    StartTransform.SetLocation(TargetTransform.TransformPosition(LocalOffset));

    const FQuat DoorInsertRotation = TargetTransform.GetRotation();
    const FQuat MeshCorrectionQuat(FVector::UpVector, PI);
    FQuat FinalKeyRotation = MeshCorrectionQuat * DoorInsertRotation;
    if (SideSign < 0.f) FinalKeyRotation = FQuat(FVector::UpVector, PI) * FinalKeyRotation;

    StartTransform.SetRotation(FinalKeyRotation);
    TargetTransform.SetRotation(FinalKeyRotation);

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AKeyActor* Key = GetWorld()->SpawnActor<AKeyActor>(AKeyActor::StaticClass(), StartTransform, Params);
    if (!Key) { UE_LOG(LogTemplateCharacter, Error, TEXT("UseSelectedItem: Failed to spawn AKeyActor")); return; }

    if (Item.ItemMesh) Key->SetKeyMesh(Item.ItemMesh);
    else UE_LOG(LogTemplateCharacter, Warning, TEXT("UseSelectedItem: item has no ItemMesh"));

    Key->StartInsertAnimation(StartTransform, TargetTransform, 1.2f, CurrentDoorUnlockTarget, this);
}

void AHorrorGameCharacter::CancelDoorUnlock()
{
    // ===== NEW: Padlock cancel =====
    if (CurrentPadlockTarget)
    {
        // Only allow cancel if NOT in the middle of unlock animation
        if (!CurrentPadlockTarget->bAnimating)
        {
            EndPadlockInteraction();
        }
        return;
    }

    if (CurrentPickupItemTarget)  { EndItemInteraction(false); return; }
    if (CurrentPCTerminalTarget)  { EndPCInteraction(false); return; }
    if (CurrentDoorUnlockTarget)  { EndDoorUnlockSequence(false); return; }
    if (bInventoryOpen)           { ToggleInventory(); }
}

void AHorrorGameCharacter::BeginPCInteraction(APCTerminalActor* PCActor)
{
    if (!PCActor) return;
    CurrentPCTerminalTarget = PCActor;
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    PC->SetIgnoreLookInput(true);
    UCameraComponent* UseCam = PCActor->GetInteractionCamera(this);
    if (UseCam)
    {
        UseCam->Activate(true);
        FViewTargetTransitionParams Params;
        Params.BlendTime = 0.15f;
        Params.BlendFunction = VTBlend_Cubic;
        PC->SetViewTarget(PCActor, Params);
        GetWorldTimerManager().ClearTimer(TerminalChatTimerHandle);
        GetWorldTimerManager().SetTimer(TerminalChatTimerHandle, this,
            &AHorrorGameCharacter::StartTerminalChat, Params.BlendTime + 0.05f, false);
    }

    if (PC->GetLocalPlayer())
    {
        UEnhancedInputLocalPlayerSubsystem* Sub =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
        if (Sub) { Sub->RemoveMappingContext(IMC_Gameplay); Sub->AddMappingContext(IMC_PCInteraction, 2); }
    }
    PC->bShowMouseCursor = true;
    PC->SetInputMode(FInputModeGameAndUI());
}

void AHorrorGameCharacter::EndPCInteraction(bool bSuccess)
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (CurrentPCTerminalTarget) CurrentPCTerminalTarget->DeactivateInteractionCamera();
    if (PC)
    {
        PC->SetViewTargetWithBlend(this, 0.15f, EViewTargetBlendFunction::VTBlend_Cubic, 0.f, true);
        PC->SetIgnoreLookInput(false);
        if (PC->GetLocalPlayer())
        {
            auto* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
            if (Sub) { Sub->RemoveMappingContext(IMC_PCInteraction); Sub->AddMappingContext(IMC_Gameplay, 0); }
        }
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }
    CurrentPCTerminalTarget = nullptr;
}

// ===================================================================
// Pickup item interaction
// ===================================================================

void AHorrorGameCharacter::BeginItemInteraction(APickupItemActor* ItemActor)
{
    if (!ItemActor) return;
    CurrentPickupItemTarget = ItemActor;
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    PC->SetIgnoreLookInput(true);
    UCameraComponent* UseCam = ItemActor->GetInteractionCamera(this);
    if (UseCam)
    {
        UseCam->Activate(true);
        FViewTargetTransitionParams Params;
        Params.BlendTime = 0.15f;
        Params.BlendFunction = VTBlend_Cubic;
        PC->SetViewTarget(ItemActor, Params);
    }

    if (PC->GetLocalPlayer())
    {
        auto* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
        if (Sub) { Sub->RemoveMappingContext(IMC_Gameplay); Sub->RemoveMappingContext(IMC_InventoryUI); Sub->AddMappingContext(IMC_InteractionItem, 2); }
    }
    PC->bShowMouseCursor = false;
    PC->SetInputMode(FInputModeGameOnly());

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
            FString::Printf(TEXT("Inspecting: %s  [E] Pick up  [Q] Cancel"), *ItemActor->ItemDisplayName.ToString()));
}

void AHorrorGameCharacter::EndItemInteraction(bool bPickedUp)
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (CurrentPickupItemTarget) CurrentPickupItemTarget->DeactivateInteractionCamera();
    if (PC)
    {
        PC->SetViewTargetWithBlend(this, 0.15f, EViewTargetBlendFunction::VTBlend_Cubic, 0.f, true);
        PC->SetIgnoreLookInput(false);
        if (PC->GetLocalPlayer())
        {
            auto* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
            if (Sub) { Sub->RemoveMappingContext(IMC_InteractionItem); Sub->AddMappingContext(IMC_Gameplay, 0); }
        }
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }

    if (bPickedUp && CurrentPickupItemTarget && InventoryComponent)
    {
        FInventoryItem NewItem = CurrentPickupItemTarget->MakeInventoryItem();
        InventoryComponent->AddItem(NewItem);
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Green,
                FString::Printf(TEXT("Picked up: %s"), *NewItem.DisplayName.ToString()));
        CurrentPickupItemTarget->Destroy();
    }
    CurrentPickupItemTarget = nullptr;
}

void AHorrorGameCharacter::ConfirmPickupItem()
{
    if (!CurrentPickupItemTarget) return;
    EndItemInteraction(true);
}

// ===================================================================

void AHorrorGameCharacter::PerformInteractionScan()
{
    if (!InteractionSphere || !GetWorld()) return;

    TArray<AActor*> OverlappingActors;
    InteractionSphere->GetOverlappingActors(OverlappingActors);

    AActor* BestActor = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();
    const FVector MyLoc = GetActorLocation();

    for (AActor* Actor : OverlappingActors)
    {
        if (!Actor) continue;
        if (AInteractableActor* IA = Cast<AInteractableActor>(Actor))
        {
            IA->UpdateArrowVisibility(this);
            if (IA->CanShowFullInteraction(this))
            {
                const float DistSq = FVector::DistSquared(IA->GetInteractionLocation(), MyLoc);
                if (DistSq < BestDistSq) { BestDistSq = DistSq; BestActor = Actor; }
            }
        }
        else if (ADoorActor* D = Cast<ADoorActor>(Actor))
        {
            D->UpdateArrowVisibility(this);
            if (D->CanShowFullInteraction(this))
            {
                float DistSq = FVector::DistSquared(D->GetActorLocation(), MyLoc);
                if (DistSq < BestDistSq) { BestDistSq = DistSq; BestActor = Actor; }
            }
        }
        else if (APCTerminalActor* P = Cast<APCTerminalActor>(Actor))
        {
            P->UpdateArrowVisibility(this);
            if (P->CanShowFullInteraction(this))
            {
                float DistSq = FVector::DistSquared(P->GetActorLocation(), MyLoc);
                if (DistSq < BestDistSq) { BestDistSq = DistSq; BestActor = Actor; }
            }
        }
    }

    for (AActor* Actor : OverlappingActors)
    {
        if (!Actor) continue;
        if (AInteractableActor* IA = Cast<AInteractableActor>(Actor))
            IA->SetFullWidgetVisible(Actor == BestActor, this);
        else if (ADoorActor* D = Cast<ADoorActor>(Actor))
            D->SetFullWidgetVisible(Actor == BestActor, this);
        else if (APCTerminalActor* P = Cast<APCTerminalActor>(Actor))
            P->SetFullWidgetVisible(Actor == BestActor, this);
    }
    CurrentInteractable = BestActor;
}

void AHorrorGameCharacter::StartTerminalChat()
{
    if (CurrentPCTerminalTarget)
        CurrentPCTerminalTarget->BeginChatSession();
}