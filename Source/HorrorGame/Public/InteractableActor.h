// Source/HorrorGame/Public/InteractableActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableActor.generated.h"

class UInteractableManager;
class UCameraComponent;
class UWidgetComponent;
class UBoxComponent;

/**
 * Base class for all interactable actors (doors, PC terminals, drawers, etc.)
 * Provides standardized widget components and simple helpers to show/hide interaction UI.
 */
UCLASS()
class HORRORGAME_API AInteractableActor : public AActor
{
    GENERATED_BODY()

public:
    AInteractableActor();

    // Called by the character (or manager) to test whether to show arrow (long-range) marker.
    virtual bool CanShowInteraction(APawn* Player) const;

    // Called to test whether the full interaction widget (use/inspect) should be shown.
    virtual bool CanShowFullInteraction(APawn* Player) const;

    // Called to show/hide the full interaction widget (character passes itself for context).
    virtual void SetFullWidgetVisible(bool bVisible, APawn* Player);

    // Return the box that denotes the interaction area for FullInteraction (if any).
    virtual UBoxComponent* GetActiveInteractionBox(APawn* Player) const;

    // Where the interaction camera should focus / where the interaction happens.
    virtual FVector GetInteractionLocation() const;

    virtual void UpdateArrowVisibility(APawn* Player);

    // Optional interaction camera (actors that switch camera for interaction can attach/use this)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interactable|Components")
    class UCameraComponent* InteractionCamera;

    virtual UCameraComponent* GetInteractionCamera(const APawn* Player) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Widgets / components common to many interactables
    // Keep them protected so derived classes may override or customize
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interactable|Components")
    class UWidgetComponent* ArrowWidget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interactable|Components")
    class UWidgetComponent* FullInteractionWidget;

    // Optional interaction box used to evaluate "closest" interactable location
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interactable|Components")
    UBoxComponent* InteractionBox;

    /** Returns true if the interaction box is on screen AND there's no wall blocking line of sight from the camera. */
    bool IsInteractionVisibleToPlayer(APawn* Player) const;

private:
    // Register/unregister with InteractableManager
    void RegisterSelf();
    void UnregisterSelf();
};