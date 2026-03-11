// PCTerminalActor.h
#pragma once

#include "CoreMinimal.h"
#include "InteractableActor.h"    // <- new base
#include "PCTerminalActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UWidgetComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UCanvasRenderTarget2D;

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
    float GetInteractionMaxDistance() const { return InteractionMaxDistance; }
    float GetInteractionUseDistance() const { return InteractionUseDistance; }

    // Interactable API overrides
    virtual bool CanShowInteraction(APawn* Player) const override;
    virtual bool CanShowFullInteraction(APawn* Player) const override;
    virtual void SetFullWidgetVisible(bool bVisible, APawn* Player) override;
    virtual FVector GetInteractionLocation() const override;

    UCameraComponent* GetInteractionCamera() const { return InteractionCamera; }
    void DeactivateInteractionCamera();

protected:
    /* Components */
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

    /* Helpers */
    void SetupRenderTarget();
    UFUNCTION()
    void OnCanvasUpdate(UCanvas* Canvas, int32 Width, int32 Height);
};