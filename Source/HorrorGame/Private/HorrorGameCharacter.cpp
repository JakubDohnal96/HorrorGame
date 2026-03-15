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
#include "PickupItemActor.h"          // ← NEW
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include <limits>

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AHorrorGameCharacter

AHorrorGameCharacter::AHorrorGameCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.f);

	// Camera
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// First-person mesh
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	// Defaults
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
    {
        InventoryWidgetInstance = CreateWidget<UInventoryWidgetBase>(GetWorld(), InventoryWidgetClass);
    }

	APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    ULocalPlayer* LP = PC->GetLocalPlayer();
    if (!LP) return;

    UEnhancedInputLocalPlayerSubsystem* Subsystem =
        LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!Subsystem) return;

    Subsystem->AddMappingContext(IMC_Gameplay, 0);

    if (InteractionSphere)
    {
        InteractionSphere->SetSphereRadius(InteractionScanRadius);
    }

    GetWorldTimerManager().SetTimer(
        InteractionScanTimerHandle,
        this,
        &AHorrorGameCharacter::PerformInteractionScan,
        InteractionScanRate,
        true
    );
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
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHorrorGameCharacter::Move);
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHorrorGameCharacter::Look);

		if (InteractAction)
		{
			EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AHorrorGameCharacter::Interact);
		}
		
		if (InventoryToggleAction)
		{
			EnhancedInput->BindAction(InventoryToggleAction, ETriggerEvent::Started, this, &AHorrorGameCharacter::ToggleInventory);
		}

		// navigation
		if (InventoryNavUpAction)
			EnhancedInput->BindAction(InventoryNavUpAction, ETriggerEvent::Started, this, &AHorrorGameCharacter::OnNavUp);
		if (InventoryNavDownAction)
			EnhancedInput->BindAction(InventoryNavDownAction, ETriggerEvent::Started, this, &AHorrorGameCharacter::OnNavDown);
		if (InventoryNavLeftAction)
			EnhancedInput->BindAction(InventoryNavLeftAction, ETriggerEvent::Started, this, &AHorrorGameCharacter::OnNavLeft);
		if (InventoryNavRightAction)
			EnhancedInput->BindAction(InventoryNavRightAction, ETriggerEvent::Started, this, &AHorrorGameCharacter::OnNavRight);
		
		if (InventoryUseAction)
		{
			EnhancedInput->BindAction(InventoryUseAction, ETriggerEvent::Started, this, &AHorrorGameCharacter::UseSelectedItem);
		}

		if (InventoryCancelAction)
		{
			EnhancedInput->BindAction(InventoryCancelAction, ETriggerEvent::Started, this, &AHorrorGameCharacter::CancelDoorUnlock);
		}

		// ===== NEW: Pickup confirm action =====
		if (PickupConfirmAction)
		{
			EnhancedInput->BindAction(PickupConfirmAction, ETriggerEvent::Started, this, &AHorrorGameCharacter::ConfirmPickupItem);
		}
		// ===== END NEW =====
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("EnhancedInputComponent not found on %s"), *GetName());
	}
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
    // ---------- PC terminal check first ----------
    TArray<AActor*> FoundPCs;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APCTerminalActor::StaticClass(), FoundPCs);

    APCTerminalActor* BestPC = nullptr;
    float BestPCDistance = TNumericLimits<float>::Max();

    for (AActor* Actor : FoundPCs)
    {
        APCTerminalActor* PCActor = Cast<APCTerminalActor>(Actor);
        if (!PCActor || !PCActor->CanShowFullInteraction(this)) continue;

        const float Dist = FVector::Dist(GetActorLocation(), PCActor->GetInteractionLocation());
        if (Dist < BestPCDistance)
        {
            BestPCDistance = Dist;
            BestPC = PCActor;
        }
    }

    if (BestPC)
    {
        if (CurrentPCTerminalTarget == BestPC)
        {
            EndPCInteraction();
        }
        else
        {
            BeginPCInteraction(BestPC);
        }
        return;
    }

    // ===== NEW: Pickup item check (before doors) =====
    {
        TArray<AActor*> FoundItems;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), APickupItemActor::StaticClass(), FoundItems);

        APickupItemActor* BestItem = nullptr;
        float BestItemDist = TNumericLimits<float>::Max();

        for (AActor* Actor : FoundItems)
        {
            APickupItemActor* Item = Cast<APickupItemActor>(Actor);
            if (!Item || !Item->CanShowFullInteraction(this)) continue;

            const float Dist = FVector::Dist(GetActorLocation(), Item->GetInteractionLocation());
            if (Dist < BestItemDist)
            {
                BestItemDist = Dist;
                BestItem = Item;
            }
        }

        if (BestItem)
        {
            if (CurrentPickupItemTarget == BestItem)
            {
                // Already inspecting this item → treat E as pick-up confirm
                ConfirmPickupItem();
            }
            else
            {
                BeginItemInteraction(BestItem);
            }
            return;
        }
    }
    // ===== END NEW =====

    // ---------- Door fallback (existing logic) ----------
    TArray<AActor*> FoundDoors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADoorActor::StaticClass(), FoundDoors);

    ADoorActor* BestDoor = nullptr;
    float BestDistance = TNumericLimits<float>::Max();

    for (AActor* Actor : FoundDoors)
    {
        ADoorActor* Door = Cast<ADoorActor>(Actor);
        if (!Door || !Door->CanShowFullInteraction(this)) continue;

        UBoxComponent* Box = Door->GetActiveInteractionBox(this);
        if (!Box) continue;

        const float Dist = FVector::Dist(GetActorLocation(), Box->GetComponentLocation());
        if (Dist < BestDistance) { BestDistance = Dist; BestDoor = Door; }
    }

    if (BestDoor)
    {
        if (BestDoor->IsLocked())
            BeginDoorUnlockSequence(BestDoor);
        else
            BestDoor->ToggleDoor();
    }
}

void AHorrorGameCharacter::ToggleInventory()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    ULocalPlayer* LP = PC->GetLocalPlayer();
    if (!LP) return;

    UEnhancedInputLocalPlayerSubsystem* Subsystem =
        LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!Subsystem) return;

    if (!InventoryWidgetInstance && InventoryWidgetClass)
    {
        InventoryWidgetInstance = CreateWidget<UInventoryWidgetBase>(GetWorld(), InventoryWidgetClass);
    }

    if (!InventoryWidgetInstance || !InventoryComponent)
        return;

    bInventoryOpen = !bInventoryOpen;

    if (bInventoryOpen)
    {
        InventoryWidgetInstance->AddToViewport();
        InventoryWidgetInstance->RefreshInventory(
            InventoryComponent->Items,
            InventoryComponent->GetSelectedIndex()
        );

        InventoryComponent->OnInventoryChanged.AddDynamic(
            this,
            &AHorrorGameCharacter::OnInventoryChanged
        );

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

        InventoryComponent->OnInventoryChanged.RemoveDynamic(
            this,
            &AHorrorGameCharacter::OnInventoryChanged
        );

        Subsystem->RemoveMappingContext(IMC_InventoryUI);
        Subsystem->AddMappingContext(IMC_Gameplay, 0);

        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }
}

void AHorrorGameCharacter::OnInventoryChanged()
{
    if (!bInventoryOpen)
        return;

    if (!InventoryWidgetInstance || !InventoryComponent)
        return;

    InventoryWidgetInstance->RefreshInventory(
        InventoryComponent->Items,
        InventoryComponent->GetSelectedIndex()
    );
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
	{
		if (ULocalPlayer* LP = PC_Sub->GetLocalPlayer())
		{
			Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP);
		}
	}

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC)
    {
        PC->SetIgnoreLookInput(true);
    }

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
			InventoryWidgetInstance->RefreshInventory(
				InventoryComponent->Items,
				InventoryComponent->GetSelectedIndex()
			);
		}

		InventoryComponent->OnInventoryChanged.AddDynamic(
			this,
			&AHorrorGameCharacter::OnInventoryChanged
		);
	}
}

void AHorrorGameCharacter::EndDoorUnlockSequence(bool bSuccess)
{
    APlayerController* PC = Cast<APlayerController>(GetController());

    UEnhancedInputLocalPlayerSubsystem* Subsystem = nullptr;
    if (PC && PC->GetLocalPlayer())
    {
        Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
    }

    if (PC)
    {
        PC->SetControlRotation(GetActorRotation());
    }

    if (bInventoryOpen && InventoryWidgetInstance && InventoryComponent)
    {
        bInventoryOpen = false;

        InventoryWidgetInstance->RemoveFromParent();
        InventoryComponent->OnInventoryChanged.RemoveDynamic(
            this,
            &AHorrorGameCharacter::OnInventoryChanged
        );
    }

    if (CurrentDoorUnlockTarget)
    {
        CurrentDoorUnlockTarget->DeactivateInteractionCameras();
    }

    if (Subsystem)
    {
        Subsystem->RemoveMappingContext(IMC_Interaction);
        Subsystem->AddMappingContext(IMC_Gameplay, 0);
    }

    if (PC)
    {
        PC->SetViewTargetWithBlend(
            this,
            0.15f,
            EViewTargetBlendFunction::VTBlend_Cubic,
            0.f,
            true
        );

        PC->SetIgnoreLookInput(false);
    }

    CurrentDoorUnlockTarget = nullptr;
}

void AHorrorGameCharacter::UseSelectedItem()
{
    if (!bInventoryOpen)
    {
        return;
    }

    if (!InventoryComponent)
    {
        UE_LOG(LogTemplateCharacter, Warning, TEXT("UseSelectedItem: InventoryComponent missing"));
        return;
    }

    if (!CurrentDoorUnlockTarget)
    {
        UE_LOG(LogTemplateCharacter, Log, TEXT("UseSelectedItem: no door target"));
        return;
    }

    const int32 SelectedIndex = InventoryComponent->GetSelectedIndex();
    if (!InventoryComponent->Items.IsValidIndex(SelectedIndex))
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("No item selected"));
        }
        return;
    }

    const FInventoryItem& Item = InventoryComponent->Items[SelectedIndex];
    const int32 DoorSymbol = CurrentDoorUnlockTarget->GetCurrentSymbolIndex();

    if (Item.KeyIndex != DoorSymbol)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Red, TEXT("Not the right item"));
        }
        return;
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Green, TEXT("Right item!"));
    }

    FTransform TargetTransform =
        CurrentDoorUnlockTarget->GetKeyInsertTransform();

    if (!TargetTransform.Equals(FTransform::Identity, 0.0001f) == false)
    {
    }

    const FVector PlayerWorldLocation = GetActorLocation();
    const FVector PlayerLocal = TargetTransform.InverseTransformPosition(PlayerWorldLocation);

    const float DistanceFromHole = 20.0f;
    const float SideSign = (PlayerLocal.Y >= 0.f) ? +1.f : -1.f;

    const float StopBeforeHole = 3.0f;

    const FVector LocalEndOffset(0.f, SideSign * StopBeforeHole, 0.f);
    const FVector WorldEndOffset = TargetTransform.TransformVector(LocalEndOffset);
    TargetTransform.SetLocation(TargetTransform.GetLocation() + WorldEndOffset);

    const FVector LocalOffset(0.f, SideSign * DistanceFromHole, 0.f);
    const FVector StartLocation = TargetTransform.TransformPosition(LocalOffset);

    FTransform StartTransform = TargetTransform;
    StartTransform.SetLocation(StartLocation);

    const FQuat DoorInsertRotation = TargetTransform.GetRotation();
    const FQuat MeshCorrectionQuat(FVector::UpVector, PI);
    FQuat FinalKeyRotation = MeshCorrectionQuat * DoorInsertRotation;

    if (SideSign < 0.f)
    {
        const FQuat SideFlipQuat(FVector::UpVector, PI);
        FinalKeyRotation = SideFlipQuat * FinalKeyRotation;
    }

    StartTransform.SetRotation(FinalKeyRotation);
    TargetTransform.SetRotation(FinalKeyRotation);

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AKeyActor* Key = GetWorld()->SpawnActor<AKeyActor>(
        AKeyActor::StaticClass(),
        StartTransform,
        Params
    );

    if (!Key)
    {
        UE_LOG(LogTemplateCharacter, Error, TEXT("UseSelectedItem: Failed to spawn AKeyActor"));
        return;
    }

    if (Item.ItemMesh)
    {
        Key->SetKeyMesh(Item.ItemMesh);
    }
    else
    {
        UE_LOG(LogTemplateCharacter, Warning, TEXT("UseSelectedItem: item has no ItemMesh assigned"));
    }

    Key->StartInsertAnimation(
        StartTransform,
        TargetTransform,
        1.2f,
        CurrentDoorUnlockTarget,
        this
    );
}

void AHorrorGameCharacter::CancelDoorUnlock()
{
    // ===== NEW: Pickup item interaction cancel =====
    if (CurrentPickupItemTarget)
    {
        EndItemInteraction(false);
        return;
    }
    // ===== END NEW =====

    // If currently interacting with a PC terminal, end that interaction first
    if (CurrentPCTerminalTarget)
    {
        EndPCInteraction(false);
        return;
    }

    // Door unlock sequence (existing)
    if (CurrentDoorUnlockTarget)
    {
        EndDoorUnlockSequence(false);
        return;
    }

    // Plain inventory close
    if (bInventoryOpen)
    {
        ToggleInventory();
    }
}

void AHorrorGameCharacter::BeginPCInteraction(APCTerminalActor* PCActor)
{
    if (!PCActor) return;

    CurrentPCTerminalTarget = PCActor;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    PC->SetIgnoreLookInput(true);

    UCameraComponent* UseCam = PCActor->GetInteractionCamera();
    if (UseCam)
    {
        UseCam->Activate(true);

        FViewTargetTransitionParams Params;
        Params.BlendTime = 0.15f;
        Params.BlendFunction = VTBlend_Cubic;

        PC->SetViewTarget(PCActor, Params);

        GetWorldTimerManager().ClearTimer(TerminalChatTimerHandle);

        GetWorldTimerManager().SetTimer(
            TerminalChatTimerHandle,
            this,
            &AHorrorGameCharacter::StartTerminalChat,
            Params.BlendTime + 0.05f,
            false
        );
    }

    if (PC->GetLocalPlayer())
    {
        UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());

        if (Subsystem)
        {
            Subsystem->RemoveMappingContext(IMC_Gameplay);
            Subsystem->AddMappingContext(IMC_PCInteraction, 2);
        }
    }

    PC->bShowMouseCursor = true;

    FInputModeGameAndUI Mode;
    PC->SetInputMode(Mode);
}

void AHorrorGameCharacter::EndPCInteraction(bool bSuccess /*=false*/)
{
    APlayerController* PC = Cast<APlayerController>(GetController());

    if (CurrentPCTerminalTarget)
    {
        CurrentPCTerminalTarget->DeactivateInteractionCamera();
    }

    if (PC)
    {
        PC->SetViewTargetWithBlend(this, 0.15f, EViewTargetBlendFunction::VTBlend_Cubic, 0.f, true);
        PC->SetIgnoreLookInput(false);

        if (PC->GetLocalPlayer())
        {
            UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
            if (Subsystem)
            {
                Subsystem->RemoveMappingContext(IMC_PCInteraction);
                Subsystem->AddMappingContext(IMC_Gameplay, 0);
            }
        }

        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }

    CurrentPCTerminalTarget = nullptr;
}

// ===================================================================
// NEW: Pickup item interaction (inspect → pick up or cancel)
// ===================================================================

void AHorrorGameCharacter::BeginItemInteraction(APickupItemActor* ItemActor)
{
    if (!ItemActor) return;

    CurrentPickupItemTarget = ItemActor;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    // Lock camera rotation
    PC->SetIgnoreLookInput(true);

    // Activate the item's interaction camera
    UCameraComponent* UseCam = ItemActor->GetInteractionCamera(this);
    if (UseCam)
    {
        UseCam->Activate(true);

        FViewTargetTransitionParams Params;
        Params.BlendTime = 0.15f;
        Params.BlendFunction = VTBlend_Cubic;
        PC->SetViewTarget(ItemActor, Params);
    }

    // Switch IMC: remove gameplay, add the item-interaction context
    if (PC->GetLocalPlayer())
    {
        UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
        if (Subsystem)
        {
            Subsystem->RemoveMappingContext(IMC_Gameplay);
            Subsystem->RemoveMappingContext(IMC_InventoryUI);  // just in case
            Subsystem->AddMappingContext(IMC_InteractionItem, 2);
        }
    }

    // No inventory, no cursor — just camera + two-button choice
    PC->bShowMouseCursor = false;
    PC->SetInputMode(FInputModeGameOnly());

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
            FString::Printf(TEXT("Inspecting: %s  [E] Pick up  [Q] Cancel"),
                *ItemActor->ItemDisplayName.ToString()));
    }
}

void AHorrorGameCharacter::EndItemInteraction(bool bPickedUp)
{
    APlayerController* PC = Cast<APlayerController>(GetController());

    // Deactivate the item camera
    if (CurrentPickupItemTarget)
    {
        CurrentPickupItemTarget->DeactivateInteractionCamera();
    }

    // Blend camera back to player
    if (PC)
    {
        PC->SetViewTargetWithBlend(
            this,
            0.15f,
            EViewTargetBlendFunction::VTBlend_Cubic,
            0.f,
            true
        );
        PC->SetIgnoreLookInput(false);

        // Restore gameplay IMC
        if (PC->GetLocalPlayer())
        {
            UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
            if (Subsystem)
            {
                Subsystem->RemoveMappingContext(IMC_InteractionItem);
                Subsystem->AddMappingContext(IMC_Gameplay, 0);
            }
        }

        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }

    // If picked up, add to inventory and destroy the world actor
    if (bPickedUp && CurrentPickupItemTarget && InventoryComponent)
    {
        FInventoryItem NewItem = CurrentPickupItemTarget->MakeInventoryItem();
        InventoryComponent->AddItem(NewItem);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Green,
                FString::Printf(TEXT("Picked up: %s"), *NewItem.DisplayName.ToString()));
        }

        CurrentPickupItemTarget->Destroy();
    }

    CurrentPickupItemTarget = nullptr;
}

void AHorrorGameCharacter::ConfirmPickupItem()
{
    if (!CurrentPickupItemTarget) return;

    EndItemInteraction(true);  // true = picked up
}

// ===================================================================
// END NEW
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
                const FVector Loc = IA->GetInteractionLocation();
                const float DistSq = FVector::DistSquared(Loc, MyLoc);
                if (DistSq < BestDistSq)
                {
                    BestDistSq = DistSq;
                    BestActor = Actor;
                }
            }
        }
        else
        {
            // fallback for legacy actors not yet inheriting AInteractableActor
            if (ADoorActor* D = Cast<ADoorActor>(Actor))
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
            // NOTE: APickupItemActor inherits AInteractableActor, so
            // it is already caught by the first branch above.
        }
    }

    // Set full widget visibility
    for (AActor* Actor : OverlappingActors)
    {
        if (!Actor) continue;

        if (AInteractableActor* IA = Cast<AInteractableActor>(Actor))
        {
            IA->SetFullWidgetVisible(Actor == BestActor, this);
        }
        else if (ADoorActor* D = Cast<ADoorActor>(Actor))
        {
            D->SetFullWidgetVisible(Actor == BestActor, this);
        }
        else if (APCTerminalActor* P = Cast<APCTerminalActor>(Actor))
        {
            P->SetFullWidgetVisible(Actor == BestActor, this);
        }
    }

    CurrentInteractable = BestActor;
}

void AHorrorGameCharacter::StartTerminalChat()
{
    if (CurrentPCTerminalTarget)
    {
        CurrentPCTerminalTarget->BeginChatSession();
    }
}