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
#include "Components/SphereComponent.h"

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

    // Give it a conservative default radius; we'll set the final radius in BeginPlay so the editable value is used
    InteractionSphere->InitSphereRadius(200.f);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    // Ignore everything by default, then overlap typical dynamic/static so we get actors
    InteractionSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    // Adjust channels as needed for your project. This is permissive; refine later.
    InteractionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECollisionResponse::ECR_Overlap);
    InteractionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);
    InteractionSphere->SetGenerateOverlapEvents(true);
}

void AHorrorGameCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Pre-populate with 4 keys (indices 0..3)
    if (InventoryComponent)
    {
    InventoryComponent->AddItem(
        FInventoryItem(FText::FromString("Key A"), 0, KeyMesh_A));

    InventoryComponent->AddItem(
        FInventoryItem(FText::FromString("Key B"), 1, KeyMesh_B));

    InventoryComponent->AddItem(
        FInventoryItem(FText::FromString("Key C"), 2, KeyMesh_C));

    InventoryComponent->AddItem(
        FInventoryItem(FText::FromString("Key D"), 3, KeyMesh_D));
    }

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

	// ---------- HUD: show generic interact HUD if any door shows arrow (long-range) ----------
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
        // Start PC interaction (camera, input mapping). If already interacting with this PC, end it.
        if (CurrentPCTerminalTarget == BestPC)
        {
            // toggle off
            EndPCInteraction();
        }
        else
        {
            BeginPCInteraction(BestPC);
        }
        return;
    }

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

    // Lazy create widget if needed
    if (!InventoryWidgetInstance && InventoryWidgetClass)
    {
        InventoryWidgetInstance = CreateWidget<UInventoryWidgetBase>(GetWorld(), InventoryWidgetClass);
    }

    if (!InventoryWidgetInstance || !InventoryComponent)
        return;

    bInventoryOpen = !bInventoryOpen;

    if (bInventoryOpen)
    {
        // 🟥 OPEN INVENTORY
        InventoryWidgetInstance->AddToViewport();

        // 🔑 THIS IS THE MISSING PART
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
        // 🟩 CLOSE INVENTORY
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
        // 🔒 IMPORTANT: stop controller from rotating camera
        PC->SetIgnoreLookInput(true);
    }

    UCameraComponent* UseCam = Door->GetActiveInteractionCamera(this);
    if (PC && UseCam)
    {
        UseCam->Activate(true);

        FViewTargetTransitionParams Params;
        Params.BlendTime = 0.15f;
        Params.BlendFunction = VTBlend_Cubic;

        PC->SetViewTarget(Door, Params);
    }

	if (Subsystem)
	{
		Subsystem->RemoveMappingContext(IMC_Gameplay);
		Subsystem->RemoveMappingContext(IMC_InventoryUI);          // in case it was open
		Subsystem->AddMappingContext(IMC_Interaction, 2);  // highest priority
	}

	// Open inventory UI visually (NO ToggleInventory)
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

    // 🔁 Enhanced Input subsystem
    UEnhancedInputLocalPlayerSubsystem* Subsystem = nullptr;
    if (PC && PC->GetLocalPlayer())
    {
        Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
    }

    // 1️⃣ Reset control rotation
    if (PC)
    {
        PC->SetControlRotation(GetActorRotation());
    }

    // 2️⃣ Close inventory UI
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

    // 4️⃣ Restore gameplay input mapping
    if (Subsystem)
    {
        Subsystem->RemoveMappingContext(IMC_Interaction);
        Subsystem->AddMappingContext(IMC_Gameplay, 0);
    }

    // 5️⃣ Blend camera back
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

    const FVector PlayerWorldLocation = GetActorLocation(); // or GetController()->GetPawn()->GetActorLocation()

    const FVector PlayerLocal = TargetTransform.InverseTransformPosition(PlayerWorldLocation);

    const float DistanceFromHole = 20.0f; // tweakable: how far from hole to spawn the key

    const float SideSign = (PlayerLocal.Y >= 0.f) ? +1.f : -1.f;

    // --------------------------------------------------
    // 🔧 STOP BEFORE KEYHOLE (SIDE-AWARE)
    // --------------------------------------------------

    const float StopBeforeHole = 3.0f; // centimeters

    // Offset along local insertion axis, SAME SIDE as start
    const FVector LocalEndOffset(
        0.f,
        SideSign * StopBeforeHole,
        0.f
    );

    // Convert to world space
    const FVector WorldEndOffset =
        TargetTransform.TransformVector(LocalEndOffset);

    // Apply to target
    TargetTransform.SetLocation(
        TargetTransform.GetLocation() + WorldEndOffset
    );

    const FVector LocalOffset(0.f, SideSign * DistanceFromHole, 0.f);

    const FVector StartLocation = TargetTransform.TransformPosition(LocalOffset);

    FTransform StartTransform = TargetTransform;
    StartTransform.SetLocation(StartLocation);

    // Door insert rotation (ground truth)
    const FQuat DoorInsertRotation = TargetTransform.GetRotation();

    // 🔑 BASE correction: key mesh is modeled backwards
    // This fixes "wrong end goes into keyhole"
    const FQuat MeshCorrectionQuat(FVector::UpVector, PI); // 180° yaw

    // Start with corrected base rotation
    FQuat FinalKeyRotation = MeshCorrectionQuat * DoorInsertRotation;

    // 👤 Player-side flip (already correct logic)
    if (SideSign < 0.f)
    {
        const FQuat SideFlipQuat(FVector::UpVector, PI); // another 180°
        FinalKeyRotation = SideFlipQuat * FinalKeyRotation;
    }

    // Apply ONLY to key transforms
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

    // Save target
    CurrentPCTerminalTarget = PCActor;

    // Disable free-look while interacting
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC)
    {
        PC->SetIgnoreLookInput(true);
    }

    // Activate PC's interaction camera & switch view
    UCameraComponent* UseCam = PCActor->GetInteractionCamera();
    if (UseCam && PC)
    {
        UseCam->Activate(true);

        FViewTargetTransitionParams Params;
        Params.BlendTime = 0.15f;
        Params.BlendFunction = VTBlend_Cubic;

        PC->SetViewTarget(PCActor, Params);
    }

    // Input mapping: remove gameplay and add PC-specific mapping context (IMC_PCInteraction)
    if (PC && PC->GetLocalPlayer())
    {
        UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
        if (Subsystem)
        {
            Subsystem->RemoveMappingContext(IMC_Gameplay);
            Subsystem->AddMappingContext(IMC_PCInteraction, 2);
        }
    }

    // Show mouse cursor & set UI mode if desired (for now we simply show/hide cursor)
    if (PC)
    {
        PC->bShowMouseCursor = true;
        FInputModeGameAndUI Mode;
        PC->SetInputMode(Mode);
    }

    // Optionally visually open an on-screen widget (we won't open inventory)
}

void AHorrorGameCharacter::EndPCInteraction(bool bSuccess /*=false*/)
{
    APlayerController* PC = Cast<APlayerController>(GetController());

    // Deactivate the interaction camera on the PC actor
    if (CurrentPCTerminalTarget)
    {
        CurrentPCTerminalTarget->DeactivateInteractionCamera();
    }

    // Restore view target to player
    if (PC)
    {
        PC->SetViewTargetWithBlend(this, 0.15f, EViewTargetBlendFunction::VTBlend_Cubic, 0.f, true);
        PC->SetIgnoreLookInput(false);

        // Restore input mapping contexts
        if (PC->GetLocalPlayer())
        {
            UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
            if (Subsystem)
            {
                Subsystem->RemoveMappingContext(IMC_PCInteraction);
                Subsystem->AddMappingContext(IMC_Gameplay, 0);
            }
        }

        // Hide mouse cursor
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }

    CurrentPCTerminalTarget = nullptr;
}

void AHorrorGameCharacter::PerformInteractionScan()
{
    if (!InteractionSphere) return;

    TArray<AActor*> OverlappingActors;
    InteractionSphere->GetOverlappingActors(OverlappingActors);

    AActor* BestActor = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();

    FVector MyLocation = GetActorLocation();

    for (AActor* Actor : OverlappingActors)
    {
        if (!Actor) continue;

        if (Actor->IsA(ADoorActor::StaticClass()) || Actor->IsA(APCTerminalActor::StaticClass()))
        {
            float DistSq = FVector::DistSquared(MyLocation, Actor->GetActorLocation());

            if (DistSq < BestDistSq)
            {
                BestDistSq = DistSq;
                BestActor = Actor;
            }
        }
    }

    CurrentInteractable = BestActor;
}