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
#include "Engine/World.h"

APCTerminalActor::APCTerminalActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MonitorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh"));
	MonitorMesh->SetupAttachment(Root);

	/* -------- Configure inherited components from AInteractableActor -------- */

	// Interaction box
	if (InteractionBox)
	{
		InteractionBox->SetupAttachment(Root);
		InteractionBox->SetBoxExtent(FVector(40.f, 40.f, 40.f));
		InteractionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		InteractionBox->SetGenerateOverlapEvents(false);
	}

	// Arrow widget (far distance)
	if (ArrowWidget)
	{
		ArrowWidget->SetupAttachment(InteractionBox);
		ArrowWidget->SetWidgetSpace(EWidgetSpace::World);
		ArrowWidget->SetDrawAtDesiredSize(true);
		ArrowWidget->SetVisibility(false);
		ArrowWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ArrowWidget->SetBlendMode(EWidgetBlendMode::Transparent);
	}

	// Full interaction widget (close distance)
	if (FullInteractionWidget)
	{
		FullInteractionWidget->SetupAttachment(InteractionBox);
		FullInteractionWidget->SetWidgetSpace(EWidgetSpace::World);
		FullInteractionWidget->SetDrawAtDesiredSize(true);
		FullInteractionWidget->SetVisibility(false);
		FullInteractionWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FullInteractionWidget->SetBlendMode(EWidgetBlendMode::Transparent);
	}

	// Interaction camera
	if (InteractionCamera)
	{
		InteractionCamera->SetupAttachment(Root);
		InteractionCamera->bAutoActivate = false;
	}

	RenderTargetWidth = 1024;
	RenderTargetHeight = 768;

	// Chat defaults
	GhostIndex = 0;
	PlayerIndex = 0;
	bChatActive = false;
	bWaitingForGhostReply = false;
	bShowPressEPrompt = false;

	// sensible defaults for messages (can be overridden in editor)
	if (GhostMessages.Num() == 0)
	{
		GhostMessages = {
			TEXT("> Peter, are you there?"),
			TEXT("> That's your grandpa kiddo."),
		};
	}
	if (PlayerMessages.Num() == 0)
	{
		PlayerMessages = {
			TEXT("> Who is this?"),
			TEXT("> What do you need grandpa?"),
		};
	}
}

void APCTerminalActor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("PCTerminal BeginPlay running"));
	SetupRenderTarget();
}

void APCTerminalActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player) return;

	// -------- Arrow visibility --------
	const bool bArrowShouldShow = CanShowInteraction(Player);
	if (ArrowWidget) ArrowWidget->SetVisibility(bArrowShouldShow);

	// -------- Rotate widgets toward camera --------
	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC || !PC->PlayerCameraManager) return;
	FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	if (ArrowWidget && ArrowWidget->IsVisible()) FInteractableUtils::FaceWidgetTowardsCamera(ArrowWidget, CameraLocation);
	if (FullInteractionWidget && FullInteractionWidget->IsVisible()) FInteractableUtils::FaceWidgetTowardsCamera(FullInteractionWidget, CameraLocation);
}

bool APCTerminalActor::CanShowInteraction(APawn* Player) const
{
	if (!Player) return false;
	const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
	return Dist <= InteractionMaxDistance;
}

bool APCTerminalActor::CanShowFullInteraction(APawn* Player) const
{
	if (!Player) return false;
	const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
	if (Dist > InteractionUseDistance) return false;

	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC) return true;

	FVector2D ScreenPos;
	const bool bProjected = PC->ProjectWorldLocationToScreen(InteractionBox->GetComponentLocation(), ScreenPos, true);
	if (!bProjected) return false;

	int32 SizeX = 0, SizeY = 0;
	PC->GetViewportSize(SizeX, SizeY);
	return ScreenPos.X >= 0 && ScreenPos.X <= SizeX && ScreenPos.Y >= 0 && ScreenPos.Y <= SizeY;
}

void APCTerminalActor::SetFullWidgetVisible(bool bVisible, APawn* Player)
{
	if (FullInteractionWidget)
	{
		FullInteractionWidget->SetVisibility(bVisible);
	}

    if (bVisible)
    {
        // store the player controller to start chat after a short delay
        if (Player)
        {
            PendingPlayerController = Cast<APlayerController>(Player->GetController());
        }
        // ensure we clear any previous pending timer
        GetWorld()->GetTimerManager().ClearTimer(DelayedChatStartTimer);

        // small delay prevents the same E press which opened the PC from also being captured
        const float DelayBeforeChat = 0.15f;
        GetWorld()->GetTimerManager().SetTimer(DelayedChatStartTimer, this, &APCTerminalActor::BeginChatSession, DelayBeforeChat, false);
    }
    else // closing
    {
        // cancel pending start and cleanup
        GetWorld()->GetTimerManager().ClearTimer(DelayedChatStartTimer);
        if (PendingPlayerController.IsValid()) PendingPlayerController = nullptr;

        if (bChatActive)
        {
            if (APlayerController* PC = Cast<APlayerController>(Player ? Player->GetController() : nullptr))
            {
                UnbindInputForPlayer(PC);
            }
            GetWorld()->GetTimerManager().ClearTimer(GhostReplyTimer);
            bWaitingForGhostReply = false;
            bShowPressEPrompt = false;
            ForceCanvasUpdate();
        }
    }
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
	Canvas->K2_DrawBox(FVector2D(0,0), FVector2D(Width,Height), 1.0f, FLinearColor::Black);

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

	// Draw the "Press E to send message" prompt near bottom-left if applicable
	if (bShowPressEPrompt && !bWaitingForGhostReply)
	{
		const FString Prompt = TEXT("> Press E to send message");
		FVector2D PromptPos(Width * 0.075f, Height * 0.88f);
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
		// show press E prompt (unless we've exhausted player's messages)
		bShowPressEPrompt = (PlayerIndex < PlayerMessages.Num());
	}
}

void APCTerminalActor::OnPlayerSendMessage()
{
	// Called when player presses E while terminal is open.
	if (bWaitingForGhostReply) return; // ignore repeated presses while waiting
	if (PlayerIndex >= PlayerMessages.Num()) 
	{
		// no more player messages configured
		bShowPressEPrompt = false;
		return;
	}

	// Append player's message immediately
	AppendChatLine(PlayerMessages[PlayerIndex]);
	PlayerIndex++;

	// Hide prompt while waiting for ghost reply
	bWaitingForGhostReply = true;
	bShowPressEPrompt = false;

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
	}

    ForceCanvasUpdate();
}

void APCTerminalActor::AddGhostReplyNow()
{
	// append ghost message now
	if (GhostIndex < GhostMessages.Num())
	{
		AppendChatLine(GhostMessages[GhostIndex]);
		GhostIndex++;
	}

	// finished waiting; allow player to press E again (if player has further messages)
	bWaitingForGhostReply = false;
	bShowPressEPrompt = (PlayerIndex < PlayerMessages.Num());
    ForceCanvasUpdate();
}

void APCTerminalActor::TriggerGhostReply()
{
	// Not used in this implementation (kept for future)
}

/* ----------------- Input binding helpers ----------------- */

void APCTerminalActor::BindInputForPlayer(APlayerController* PC)
{
    if (!PC) return;

    EnableInput(PC);

    if (InputComponent)
    {
        // Clear previous bindings to avoid duplicates
        InputComponent->ClearActionBindings();

        // Bind the project action mapping instead of hard-coded E
        if (!InteractActionName.IsNone())
        {
            InputComponent->BindAction(InteractActionName, IE_Pressed, this, &APCTerminalActor::OnPlayerSendMessage);
        }
        else
        {
            // fallback to raw key if mapping missing
            InputComponent->BindKey(EKeys::E, IE_Pressed, this, &APCTerminalActor::OnPlayerSendMessage);
        }
    }
}

void APCTerminalActor::UnbindInputForPlayer(APlayerController* PC)
{
	if (!PC) return;

	// disable input to remove bindings
	DisableInput(PC);

	// ensure timer cleared
	GetWorld()->GetTimerManager().ClearTimer(GhostReplyTimer);
}

void APCTerminalActor::BeginChatSession()
{
    if (bChatActive) return;

    bChatActive = true;

    // Append the first ghost message (if any)
    StartChatIfNeeded();

    // Bind the interact action to chat-send only now
    APlayerController* PC = PendingPlayerController.IsValid() ? PendingPlayerController.Get() : UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        BindInputForPlayer(PC);
    }

    // Show prompt if player has messages to send
    bShowPressEPrompt = (PlayerIndex < PlayerMessages.Num()) && !bWaitingForGhostReply;
    ForceCanvasUpdate();

    // done with pending PC
    PendingPlayerController = nullptr;
}

void APCTerminalActor::ForceCanvasUpdate()
{
    if (CanvasRenderTarget)
    {
        CanvasRenderTarget->UpdateResource();
    }
}