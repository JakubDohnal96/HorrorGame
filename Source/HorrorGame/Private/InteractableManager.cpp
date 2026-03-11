// Source/HorrorGame/Private/InteractableManager.cpp
#include "InteractableManager.h"
#include "InteractableActor.h"
#include "Engine/GameInstance.h"

UInteractableManager* UInteractableManager::Get(UGameInstance* GI)
{
    if (!GI) return nullptr;
    return GI->GetSubsystem<UInteractableManager>();
}

void UInteractableManager::RegisterInteractable(AInteractableActor* Actor)
{
    if (Actor)
    {
        Interactables.Add(Actor);
    }
}

void UInteractableManager::UnregisterInteractable(AInteractableActor* Actor)
{
    if (Actor)
    {
        Interactables.Remove(Actor);
    }
} 