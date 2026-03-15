// PCTerminalActor.h
#pragma once

#include "CoreMinimal.h"
#include "InteractableActor.h"
#include "PCTerminalActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UWidgetComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UCanvasRenderTarget2D;
class UInputAction; // Enhanced Input action forward-declare

UCLASS()
class HORRORGAME_API APCTerminalActor : public AInteractableActor
{
	GENERATED_BODY()

public:
	APCTerminalActor();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	// distances (kept from your original)
	float GetInteractionMaxDistance() const { return InteractionMaxDistance; }
	float GetInteractionUseDistance() const { return InteractionUseDistance; }

	// Interactable API overrides (kept from original)
	virtual bool CanShowInteraction(APawn* Player) const override;
	virtual bool CanShowFullInteraction(APawn* Player) const override;
	virtual void SetFullWidgetVisible(bool bVisible, APawn* Player) override;
	virtual FVector GetInteractionLocation() const override;

	// Getter for interaction camera (no override because base doesn't declare)
	FORCEINLINE UCameraComponent* GetInteractionCamera() const { return InteractionCamera; }
	void DeactivateInteractionCamera();

	/** Explicitly start the chat session (BlueprintCallable). Call after camera transition. */
	UFUNCTION(BlueprintCallable, Category = "PC|Chat")
	void BeginChatSession();

protected:
	/* Components (kept as in your original/InteractableActor usage) */
	UPROPERTY(VisibleAnywhere, Category="PC|Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, Category="PC|Components")
	UStaticMeshComponent* MonitorMesh;

	/* Render target / monitor material */
	UPROPERTY(EditAnywhere, Category="PC|Monitor")
	UMaterialInterface* ScreenBaseMaterial;

	UPROPERTY(EditAnywhere, Category="PC|Monitor")
	int32 ScreenMaterialIndex = 0;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* DynMaterial;

	UPROPERTY(Transient)
	UCanvasRenderTarget2D* CanvasRenderTarget;

	/** The base startup terminal text (kept as before). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PC|Monitor")
	FString TerminalText = TEXT("> booting...\n> system init...\n> ready.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PC|Monitor")
	int32 RenderTargetWidth = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PC|Monitor")
	int32 RenderTargetHeight = 768;

	/* Interaction settings */
	UPROPERTY(EditAnywhere, Category="PC|Interaction")
	float InteractionMaxDistance = 300.f;

	UPROPERTY(EditAnywhere, Category="PC|Interaction")
	float InteractionUseDistance = 120.f;

	/* Chat / messaging system (editable in editor) */
	/** Messages the ghost will send in sequence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PC|Chat")
	TArray<FString> GhostMessages;

	/** Messages the player will 'send' (each press E / action consumes the next item). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PC|Chat")
	TArray<FString> PlayerMessages;

	/** Delay (seconds) between player message and ghost reply. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PC|Chat")
	float GhostReplyDelay = 4.0f;

	/* Input config */
	/** If using Enhanced Input, assign the UInputAction asset here (preferred). */
	UPROPERTY(EditAnywhere, Category="PC|Input")
	UInputAction* InteractInputAction = nullptr;

	/** Fallback: legacy action name (if you don't use Enhanced Input) */
	UPROPERTY(EditAnywhere, Category="PC|Input")
	FName InteractActionName = FName(TEXT("Interact"));

	/* Internal state */
	/** Lines appended during chat (kept separate from base TerminalText). */
	UPROPERTY(Transient)
	TArray<FString> ChatLines;

	/** Indices to step through the above arrays. */
	int32 GhostIndex;
	int32 PlayerIndex;

	/** Are we in a chat session (player has opened the terminal)? */
	bool bChatActive;

	/** Are we currently waiting for the ghost reply (player pressed E and we are waiting)? */
	bool bWaitingForGhostReply;

	/** Whether to draw the 'Press E to send message' prompt on the canvas. */
	bool bShowPressEPrompt;

	/** Timer handle for delayed ghost reply. */
	FTimerHandle GhostReplyTimer;

	/** Timer handle for delayed chat auto-start (prevents double-press). */
	FTimerHandle DelayedChatStartTimer;

	/** Cached player controller while waiting for delayed start. */
	TWeakObjectPtr<APlayerController> PendingPlayerController;

	/* Helpers */
	void SetupRenderTarget();
	UFUNCTION()
	void OnCanvasUpdate(UCanvas* Canvas, int32 Width, int32 Height);

	/** Append a new line to the chat (and mark canvas dirty). */
	void AppendChatLine(const FString& NewLine);

	/** Called when player presses the interact/send action while terminal is open. */
	UFUNCTION()
	void OnPlayerSendMessage();

	/** Helper to start chat (called when full widget is first shown or from blueprint). */
	void StartChatIfNeeded();

	/** Enable/disable input binding for send-action while terminal is open. */
	void BindInputForPlayer(APlayerController* PC);
	void UnbindInputForPlayer(APlayerController* PC);

	/** Add ghost reply immediately (called from timer). */
	UFUNCTION()
	void AddGhostReplyNow();

	/** Force canvas refresh */
	void ForceCanvasUpdate();
};