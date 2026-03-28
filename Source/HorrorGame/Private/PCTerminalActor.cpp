// PCTerminalActor.cpp
#include "PCTerminalActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "InteractableUtils.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"

// Enhanced Input includes
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"

APCTerminalActor::APCTerminalActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MonitorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh"));
	MonitorMesh->SetupAttachment(Root);

	/* -------- Configure inherited components from AInteractableActor -------- */
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
	}

	if (FullInteractionWidget)
	{
		FullInteractionWidget->SetupAttachment(InteractionBox);
		FullInteractionWidget->SetWidgetSpace(EWidgetSpace::World);
		FullInteractionWidget->SetDrawAtDesiredSize(true);
		FullInteractionWidget->SetVisibility(false);
		FullInteractionWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FullInteractionWidget->SetBlendMode(EWidgetBlendMode::Transparent);
	}

	if (InteractionCamera)
	{
		InteractionCamera->SetupAttachment(Root);
		InteractionCamera->bAutoActivate = false;
	}

	RenderTargetWidth = 1024;
	RenderTargetHeight = 768;

	// chat defaults
	GhostIndex = 0;
	PlayerIndex = 0;
	bChatActive = false;
	bWaitingForGhostReply = false;
	bShowPressEPrompt = false;

	// Provide sensible defaults if arrays left empty (optional)
	if (GhostMessages.Num() == 0)
	{
		GhostMessages = {
			TEXT("> Peter, are you there?"),
			TEXT("> That's your grandpa kiddo.")
		};
	}
	if (PlayerMessages.Num() == 0)
	{
		PlayerMessages = {
			TEXT("> Who is this?"),
			TEXT("> What do you need grandpa?")
		};
	}
}

void APCTerminalActor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("PCTerminalActor::BeginPlay"));
	SetupRenderTarget();
}

void APCTerminalActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player) return;

    // If interaction camera is active, player is interacting — hide all widgets
    if (InteractionCamera && InteractionCamera->IsActive())
    {
        if (ArrowWidget) ArrowWidget->SetVisibility(false);
        if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false);
        return;
    }
	// Arrow visibility
	const bool bArrowShouldShow = CanShowInteraction(Player);
	if (ArrowWidget) ArrowWidget->SetVisibility(bArrowShouldShow);

	// Rotate widgets toward camera
	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC || !PC->PlayerCameraManager) return;
	FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();

	if (ArrowWidget && ArrowWidget->IsVisible())
	{
		FInteractableUtils::FaceWidgetTowardsCamera(ArrowWidget, CameraLocation);
	}
	if (FullInteractionWidget && FullInteractionWidget->IsVisible())
	{
		FInteractableUtils::FaceWidgetTowardsCamera(FullInteractionWidget, CameraLocation);
	}
}

bool APCTerminalActor::CanShowInteraction(APawn* Player) const
{
	if (!Player) return false;
	if (!InteractionBox) return false;
	const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
	return Dist <= InteractionMaxDistance;
}

bool APCTerminalActor::CanShowFullInteraction(APawn* Player) const
{
    if (!Player) return false;
    if (!InteractionBox) return false;

    const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
    return Dist <= InteractionUseDistance;
}

void APCTerminalActor::SetFullWidgetVisible(bool bVisible, APawn* Player)
{
	// show/hide the 3D widget
	if (FullInteractionWidget)
	{
		FullInteractionWidget->SetVisibility(bVisible);
	}

	// If opening, delay the chat-start slightly (so the same press that opened the terminal does not become a send)
	if (bVisible)
	{
		// cache PC so delayed BeginChatSession can bind properly
		if (Player)
		{
			PendingPlayerController = Cast<APlayerController>(Player->GetController());
		}

		// cancel previous timers
		GetWorld()->GetTimerManager().ClearTimer(DelayedChatStartTimer);

		// small delay prevents initial press from immediately sending
		const float DelayBeforeChat = 0.15f;
		GetWorld()->GetTimerManager().SetTimer(DelayedChatStartTimer, this, &APCTerminalActor::BeginChatSession, DelayBeforeChat, false);
	}
	else // closing
	{
		// Don't tear down chat if we're actively interacting (camera is on)
		if (InteractionCamera && InteractionCamera->IsActive())
			return;
			
		// cancel pending start
		GetWorld()->GetTimerManager().ClearTimer(DelayedChatStartTimer);
		PendingPlayerController = nullptr;

		// cleanup chat binding & timers
		if (bChatActive)
		{
			if (Player)
			{
				if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
				{
					UnbindInputForPlayer(PC);
				}
			}
			GetWorld()->GetTimerManager().ClearTimer(GhostReplyTimer);
			bWaitingForGhostReply = false;
			bShowPressEPrompt = false;
			bChatActive = false;
			ForceCanvasUpdate();
		}
	}
}

void APCTerminalActor::BeginChatSession()
{
	// If already active, do nothing
	if (bChatActive) return;

	bChatActive = true;

	// Start by appending the first ghost message (if any)
	StartChatIfNeeded();

	// Bind input now to accept "send" actions
	APlayerController* PC = PendingPlayerController.IsValid() ? PendingPlayerController.Get() : UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC)
	{
		BindInputForPlayer(PC);
	}
	PendingPlayerController = nullptr;

	// Show press prompt if player has messages
	bShowPressEPrompt = (PlayerIndex < PlayerMessages.Num()) && !bWaitingForGhostReply;
	ForceCanvasUpdate();

	UE_LOG(LogTemp, Log, TEXT("PCTerminalActor::BeginChatSession bound and started"));
}

FVector APCTerminalActor::GetInteractionLocation() const
{
	return InteractionBox ? InteractionBox->GetComponentLocation() : GetActorLocation();
}

void APCTerminalActor::DeactivateInteractionCamera()
{
	if (InteractionCamera) InteractionCamera->Deactivate();
}

/* ----------------- RenderTarget / Canvas code ----------------- */

void APCTerminalActor::SetupRenderTarget()
{
	if (!ScreenBaseMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("APCTerminalActor: ScreenBaseMaterial not assigned in %s"), *GetName());
		return;
	}

	DynMaterial = MonitorMesh->CreateDynamicMaterialInstance(ScreenMaterialIndex, ScreenBaseMaterial);
	if (!DynMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("APCTerminalActor: Failed to create dynamic material instance."));
		return;
	}

	CanvasRenderTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(
		this->GetWorld(),
		UCanvasRenderTarget2D::StaticClass(),
		RenderTargetWidth,
		RenderTargetHeight
	);
	if (!CanvasRenderTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("APCTerminalActor: Failed to create CanvasRenderTarget2D."));
		return;
	}

	CanvasRenderTarget->OnCanvasRenderTargetUpdate.AddDynamic(this, &APCTerminalActor::OnCanvasUpdate);
	FName ParamName = FName(TEXT("ScreenTexture"));
	DynMaterial->SetTextureParameterValue(ParamName, CanvasRenderTarget);
	CanvasRenderTarget->UpdateResource();
}

void APCTerminalActor::OnCanvasUpdate(UCanvas* Canvas, int32 Width, int32 Height)
{
	if (!Canvas) return;

	// background
	Canvas->K2_DrawBox(FVector2D(0, 0), FVector2D(Width, Height), 1.0f, FLinearColor::Black);

	// font and color (green text)
	UFont* Font = GEngine->GetSmallFont();
	if (!Font)
	{
		UE_LOG(LogTemp, Warning, TEXT("APCTerminalActor: No font found."));
	}
	FLinearColor TextColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
	float Scale = 1.0f;

	// collect lines: start with TerminalText then append ChatLines
	TArray<FString> Lines;
	TerminalText.ParseIntoArrayLines(Lines);

	for (const FString& L : ChatLines)
	{
		Lines.Add(L);
	}

	FVector2D DrawPos(Width * 0.075f, Height * 0.2f);
	float LineHeight = Font ? Font->GetMaxCharHeight() * Scale + 6.0f : 24.0f;

	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const FString& Line = Lines[i];
		Canvas->K2_DrawText(
			Font,
			Line,
			DrawPos + FVector2D(0.0f, i * LineHeight),
			FVector2D(1.5f, 1.5f),
			TextColor,
			1.0f,
			FLinearColor::Black,
			FVector2D::ZeroVector,
			false,
			false,
			false,
			FLinearColor::Black
		);
	}

	// Draw the "Press [interact] to send message" prompt near bottom-left if applicable
	if (bShowPressEPrompt && !bWaitingForGhostReply)
	{
		FString Prompt;
		// If we have an Enhanced Input action assigned, use a generic name; otherwise show the fallback key name
		if (InteractInputAction)
		{
			Prompt = TEXT("> Press E to send message");
		}
		else if (!InteractActionName.IsNone())
		{
			Prompt = FString::Printf(TEXT("> Press %s to send message"), *InteractActionName.ToString());
		}
		else
		{
			Prompt = TEXT("> Press E to send message");
		}

		// position (moved slightly up by default so UVs/cropping less likely to hide it)
		FVector2D PromptPos(Width * 0.075f, Height * 0.78f);
		Canvas->K2_DrawText(
			Font,
			Prompt,
			PromptPos,
			FVector2D(1.2f, 1.2f),
			TextColor,
			1.0f,
			FLinearColor::Black,
			FVector2D::ZeroVector,
			false,
			false,
			false,
			FLinearColor::Black
		);
	}
}

/* ----------------- Chat logic ----------------- */

void APCTerminalActor::AppendChatLine(const FString& NewLine)
{
	ChatLines.Add(NewLine);

	// force the render target to update so the new text shows immediately
	if (CanvasRenderTarget)
	{
		CanvasRenderTarget->UpdateResource();
	}
}

void APCTerminalActor::StartChatIfNeeded()
{
	// If chat hasn't started and we have ghost messages, append the first ghost line
	if (GhostIndex < GhostMessages.Num())
	{
		AppendChatLine(GhostMessages[GhostIndex]);
		GhostIndex++;
		// show press prompt (unless we've exhausted player's messages)
		bShowPressEPrompt = (PlayerIndex < PlayerMessages.Num());
		ForceCanvasUpdate();
	}
}

void APCTerminalActor::OnPlayerSendMessage()
{
	UE_LOG(LogTemp, Log, TEXT("PCTerminalActor::OnPlayerSendMessage triggered"));

	// Called when player presses Interact while terminal is open.
	if (bWaitingForGhostReply) return; // ignore repeated presses while waiting
	if (PlayerIndex >= PlayerMessages.Num())
	{
		// no more player messages configured
		bShowPressEPrompt = false;
		ForceCanvasUpdate();
		return;
	}

	// Append player's message immediately
	AppendChatLine(PlayerMessages[PlayerIndex]);
	PlayerIndex++;

	// Hide prompt while waiting for ghost reply
	bWaitingForGhostReply = true;
	bShowPressEPrompt = false;
	ForceCanvasUpdate();

	// Schedule ghost reply (if available)
	if (GhostIndex < GhostMessages.Num())
	{
		GetWorld()->GetTimerManager().SetTimer(GhostReplyTimer, this, &APCTerminalActor::AddGhostReplyNow, GhostReplyDelay, false);
	}
	else
	{
		// no ghost reply available; stop waiting and show prompt if player still has messages
		bWaitingForGhostReply = false;
		bShowPressEPrompt = (PlayerIndex < PlayerMessages.Num());
		ForceCanvasUpdate();
	}
}

void APCTerminalActor::AddGhostReplyNow()
{
	// append ghost message
	if (GhostIndex < GhostMessages.Num())
	{
		AppendChatLine(GhostMessages[GhostIndex]);
		GhostIndex++;
	}

	// finished waiting; allow player to press Interact again (if player has further messages)
	bWaitingForGhostReply = false;
	bShowPressEPrompt = (PlayerIndex < PlayerMessages.Num());
	ForceCanvasUpdate();
}

/* ----------------- Input binding helpers ----------------- */

void APCTerminalActor::BindInputForPlayer(APlayerController* PC)
{
	if (!PC) return;

	// Enable input on this actor for that player controller
	EnableInput(PC);

	if (!InputComponent) return;

	// Clear previous bindings to avoid duplicates
	InputComponent->ClearActionBindings();

	// 1) Try Enhanced Input binding first (if the player's InputComponent is an EnhancedInputComponent)
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent))
	{
		if (InteractInputAction)
		{
			EIC->BindAction(InteractInputAction, ETriggerEvent::Started, this, &APCTerminalActor::OnPlayerSendMessage);
			UE_LOG(LogTemp, Log, TEXT("PCTerminalActor: bound EnhancedInput action for chat."));
			return;
		}
	}

	// 2) Fallback: classic action mapping name (legacy input system)
	if (!InteractActionName.IsNone())
	{
		InputComponent->BindAction(InteractActionName, IE_Pressed, this, &APCTerminalActor::OnPlayerSendMessage);
		UE_LOG(LogTemp, Log, TEXT("PCTerminalActor: bound legacy action name '%s' for chat."), *InteractActionName.ToString());
		return;
	}

	// 3) Final fallback: raw key E
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &APCTerminalActor::OnPlayerSendMessage);
	UE_LOG(LogTemp, Log, TEXT("PCTerminalActor: fallback bound raw E key for chat."));
}

void APCTerminalActor::UnbindInputForPlayer(APlayerController* PC)
{
	if (!PC) return;

	// disable input to remove bindings
	DisableInput(PC);

	// ensure timer cleared
	GetWorld()->GetTimerManager().ClearTimer(GhostReplyTimer);
	ForceCanvasUpdate();
}

void APCTerminalActor::ForceCanvasUpdate()
{
	if (CanvasRenderTarget)
	{
		CanvasRenderTarget->UpdateResource();
	}
}

