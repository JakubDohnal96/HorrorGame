// PCTerminalActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PCTerminalActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UWidgetComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UCanvasRenderTarget2D;

UCLASS()
class HORRORGAME_API APCTerminalActor : public AActor
{
    GENERATED_BODY()

public:
    APCTerminalActor();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

public:
    // Interaction query helpers (same style as DoorActor)
    float GetInteractionMaxDistance() const { return InteractionMaxDistance; }
    float GetInteractionUseDistance() const { return InteractionUseDistance; }

    // Whether the arrow/indicator should show (long range)
    bool CanShowInteraction(APawn* Player) const;

    // Whether the full interaction (button) can appear (short range)
    bool CanShowFullInteraction(APawn* Player) const;

    // Show or hide the full interaction widget (called by character selection logic)
    void SetFullWidgetVisible(bool bVisible, const APawn* Player);

    // Interaction camera
    UCameraComponent* GetInteractionCamera() const { return InteractionCamera; }
    void DeactivateInteractionCamera();

    // For other systems: get the world location of the interaction box center
    FVector GetInteractionLocation() const;

protected:
    /* Components */
    UPROPERTY(VisibleAnywhere, Category="PC|Components")
    USceneComponent* Root;

    UPROPERTY(VisibleAnywhere, Category="PC|Components")
    UStaticMeshComponent* MonitorMesh;

    // Interaction area
    UPROPERTY(VisibleAnywhere, Category="PC|Interaction")
    UBoxComponent* InteractionBox;

    // Arrow-only widget (long-range indicator)
    UPROPERTY(VisibleAnywhere, Category="PC|Interaction")
    UWidgetComponent* ArrowWidget;

    // Full widget (button + label) — shown when in use distance
    UPROPERTY(VisibleAnywhere, Category="PC|Interaction")
    UWidgetComponent* InteractionWidget;

    // Camera used when interacting with PC
    UPROPERTY(VisibleAnywhere, Category="PC|Interaction")
    UCameraComponent* InteractionCamera;

    /* Render target / monitor material (from your previous code) */
    UPROPERTY(EditAnywhere, Category="PC|Monitor")
    UMaterialInterface* ScreenBaseMaterial;

    // material index for monitor screen slot (set in blueprint)
    UPROPERTY(EditAnywhere, Category="PC|Monitor")
    int32 ScreenMaterialIndex = 0;

    UPROPERTY(Transient)
    UMaterialInstanceDynamic* DynMaterial;

    UPROPERTY(Transient)
    UCanvasRenderTarget2D* CanvasRenderTarget;

    // Terminal text / render target settings (you already have these)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PC|Monitor")
    FString TerminalText = TEXT("> booting...\n> system init...\n> ready.");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PC|Monitor")
    int32 RenderTargetWidth = 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PC|Monitor")
    int32 RenderTargetHeight = 768;

    /* Interaction settings */
    UPROPERTY(EditAnywhere, Category="PC|Interaction")
    float InteractionMaxDistance = 300.f; // arrow range

    UPROPERTY(EditAnywhere, Category="PC|Interaction")
    float InteractionUseDistance = 120.f; // use range (button appears)

    /* Helpers */
    void SetupRenderTarget(); // create and bind render target (reuse your existing implementation)
    UFUNCTION()
    void OnCanvasUpdate(UCanvas* Canvas, int32 Width, int32 Height);

    

};