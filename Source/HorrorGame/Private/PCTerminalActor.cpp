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
	GhostIndex = 0;
	PlayerIndex = 0;
	bChatActive = false;
	bWaitingForGhostReply = false;
	bShowPressEPrompt = false;

	if (GhostMessages.Num() == 0)
	{
		GhostMessages = { TEXT("> Peter, are you there?"), TEXT("> That's your grandpa kiddo.") };
	}
	if (PlayerMessages.Num() == 0)
	{
		PlayerMessages = { TEXT("> Who is this?"), TEXT("> What do you need grandpa?") };
	}
}

void APCTerminalActor::BeginPlay()
{
	Super::BeginPlay();
	SetupRenderTarget();
}

void APCTerminalActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player) return;

	if (InteractionCamera && InteractionCamera->IsActive())
	{
		if (ArrowWidget) ArrowWidget->SetVisibility(false);
		if (FullInteractionWidget) FullInteractionWidget->SetVisibility(false);
		return;
	}

	const bool bArrowShouldShow = CanShowInteraction(Player);
	if (ArrowWidget) ArrowWidget->SetVisibility(bArrowShouldShow);

	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC || !PC->PlayerCameraManager) return;
	FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();

	if (ArrowWidget && ArrowWidget->IsVisible())
		FInteractableUtils::FaceWidgetTowardsCamera(ArrowWidget, CameraLocation);
	if (FullInteractionWidget && FullInteractionWidget->IsVisible())
		FInteractableUtils::FaceWidgetTowardsCamera(FullInteractionWidget, CameraLocation);
}

bool APCTerminalActor::CanShowInteraction(APawn* Player) const
{
	if (!Player || !InteractionBox) return false;
	const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
	if (Dist > InteractionMaxDistance) return false;
	return IsInteractionVisibleToPlayer(Player);
}

bool APCTerminalActor::CanShowFullInteraction(APawn* Player) const
{
	if (!Player || !InteractionBox) return false;
	const float Dist = FVector::Dist(Player->GetActorLocation(), InteractionBox->GetComponentLocation());
	if (Dist > InteractionUseDistance) return false;
	return IsInteractionVisibleToPlayer(Player);
}

void APCTerminalActor::SetFullWidgetVisible(bool bVisible, APawn* Player)
{
	if (FullInteractionWidget)
		FullInteractionWidget->SetVisibility(bVisible);

	if (bVisible)
	{
		if (Player) PendingPlayerController = Cast<APlayerController>(Player->GetController());
		GetWorld()->GetTimerManager().ClearTimer(DelayedChatStartTimer);
		const float DelayBeforeChat = 0.15f;
		GetWorld()->GetTimerManager().SetTimer(DelayedChatStartTimer, this, &APCTerminalActor::BeginChatSession, DelayBeforeChat, false);
	}
	else
	{
		if (InteractionCamera && InteractionCamera->IsActive())
			return;

		GetWorld()->GetTimerManager().ClearTimer(DelayedChatStartTimer);
		PendingPlayerController = nullptr;

		if (bChatActive)
		{
			if (Player)
			{
				if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
					UnbindInputForPlayer(PC);
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
	if (bChatActive) return;
	bChatActive = true;
	StartChatIfNeeded();
	APlayerController* PC = PendingPlayerController.IsValid() ? PendingPlayerController.Get() : UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC) BindInputForPlayer(PC);
	PendingPlayerController = nullptr;
	bShowPressEPrompt = (PlayerIndex < PlayerMessages.Num()) && !bWaitingForGhostReply;
	ForceCanvasUpdate();
}

FVector APCTerminalActor::GetInteractionLocation() const
{
	return InteractionBox ? InteractionBox->GetComponentLocation() : GetActorLocation();
}

void APCTerminalActor::DeactivateInteractionCamera()
{
	if (InteractionCamera) InteractionCamera->Deactivate();
}

void APCTerminalActor::SetupRenderTarget()
{
	if (!ScreenBaseMaterial) return;
	DynMaterial = MonitorMesh->CreateDynamicMaterialInstance(ScreenMaterialIndex, ScreenBaseMaterial);
	if (!DynMaterial) return;
	CanvasRenderTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetWorld(), UCanvasRenderTarget2D::StaticClass(), RenderTargetWidth, RenderTargetHeight);
	if (!CanvasRenderTarget) return;
	CanvasRenderTarget->OnCanvasRenderTargetUpdate.AddDynamic(this, &APCTerminalActor::OnCanvasUpdate);
	DynMaterial->SetTextureParameterValue(FName(TEXT("ScreenTexture")), CanvasRenderTarget);
	CanvasRenderTarget->UpdateResource();
}

void APCTerminalActor::OnCanvasUpdate(UCanvas* Canvas, int32 Width, int32 Height)
{
	if (!Canvas) return;
	Canvas->K2_DrawBox(FVector2D(0, 0), FVector2D(Width, Height), 1.0f, FLinearColor::Black);
	UFont* Font = GEngine->GetSmallFont();
	FLinearColor TextColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
	float Scale = 1.0f;

	TArray<FString> Lines;
	TerminalText.ParseIntoArrayLines(Lines);
	for (const FString& L : ChatLines) Lines.Add(L);

	FVector2D DrawPos(Width * 0.075f, Height * 0.2f);
	float LineHeight = Font ? Font->GetMaxCharHeight() * Scale + 6.0f : 24.0f;

	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		Canvas->K2_DrawText(Font, Lines[i], DrawPos + FVector2D(0.0f, i * LineHeight),
			FVector2D(1.5f, 1.5f), TextColor, 1.0f, FLinearColor::Black, FVector2D::ZeroVector,
			false, false, false, FLinearColor::Black);
	}

	if (bShowPressEPrompt && !bWaitingForGhostReply)
	{
		FString Prompt = TEXT("> Press E to send message");
		FVector2D PromptPos(Width * 0.075f, Height * 0.78f);
		Canvas->K2_DrawText(Font, Prompt, PromptPos, FVector2D(1.2f, 1.2f), TextColor,
			1.0f, FLinearColor::Black, FVector2D::ZeroVector, false, false, false, FLinearColor::Black);
	}
}

void APCTerminalActor::AppendChatLine(const FString& NewLine)
{
	ChatLines.Add(NewLine);
	if (CanvasRenderTarget) CanvasRenderTarget->UpdateResource();
}

void APCTerminalActor::StartChatIfNeeded()
{
	if (GhostIndex < GhostMessages.Num())
	{
		AppendChatLine(GhostMessages[GhostIndex]);
		GhostIndex++;
		bShowPressEPrompt = (PlayerIndex < PlayerMessages.Num());
		ForceCanvasUpdate();
	}
}

void APCTerminalActor::OnPlayerSendMessage()
{
	if (bWaitingForGhostReply) return;
	if (PlayerIndex >= PlayerMessages.Num()) { bShowPressEPrompt = false; ForceCanvasUpdate(); return; }
	AppendChatLine(PlayerMessages[PlayerIndex]);
	PlayerIndex++;
	bWaitingForGhostReply = true;
	bShowPressEPrompt = false;
	ForceCanvasUpdate();
	if (GhostIndex < GhostMessages.Num())
		GetWorld()->GetTimerManager().SetTimer(GhostReplyTimer, this, &APCTerminalActor::AddGhostReplyNow, GhostReplyDelay, false);
	else { bWaitingForGhostReply = false; bShowPressEPrompt = (PlayerIndex < PlayerMessages.Num()); ForceCanvasUpdate(); }
}

void APCTerminalActor::AddGhostReplyNow()
{
	if (GhostIndex < GhostMessages.Num()) { AppendChatLine(GhostMessages[GhostIndex]); GhostIndex++; }
	bWaitingForGhostReply = false;
	bShowPressEPrompt = (PlayerIndex < PlayerMessages.Num());
	ForceCanvasUpdate();
}

void APCTerminalActor::BindInputForPlayer(APlayerController* PC)
{
	if (!PC) return;
	EnableInput(PC);
	if (!InputComponent) return;
	InputComponent->ClearActionBindings();
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent))
	{
		if (InteractInputAction) { EIC->BindAction(InteractInputAction, ETriggerEvent::Started, this, &APCTerminalActor::OnPlayerSendMessage); return; }
	}
	if (!InteractActionName.IsNone()) { InputComponent->BindAction(InteractActionName, IE_Pressed, this, &APCTerminalActor::OnPlayerSendMessage); return; }
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &APCTerminalActor::OnPlayerSendMessage);
}

void APCTerminalActor::UnbindInputForPlayer(APlayerController* PC)
{
	if (!PC) return;
	DisableInput(PC);
	GetWorld()->GetTimerManager().ClearTimer(GhostReplyTimer);
	ForceCanvasUpdate();
}

void APCTerminalActor::ForceCanvasUpdate()
{
	if (CanvasRenderTarget) CanvasRenderTarget->UpdateResource();
}