// Source/HorrorGame/Public/InteractableManager.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InteractableManager.generated.h"

class AInteractableActor;

UCLASS()
class HORRORGAME_API UInteractableManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // Static accessor
    static UInteractableManager* Get(UGameInstance* GI);

    void RegisterInteractable(AInteractableActor* Actor);
    void UnregisterInteractable(AInteractableActor* Actor);

    // Return const view of current interactables
    const TSet<TWeakObjectPtr<AInteractableActor>>& GetInteractables() const { return Interactables; }

private:
    TSet<TWeakObjectPtr<AInteractableActor>> Interactables;
};