#include "InventoryComponent.h"
#include "Kismet/GameplayStatics.h"

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SelectedIndex = -1;
    // optional: reserve slots: Items.SetNum(8); // to create empty slots
}

void UInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
}

int32 UInventoryComponent::AddItem(const FInventoryItem& Item)
{
    Items.Add(Item);
    if (SelectedIndex < 0) SelectedIndex = 0;
    OnInventoryChanged.Broadcast();
    return Items.Num()-1;
}

bool UInventoryComponent::RemoveItemAt(int32 Index)
{
    if (!Items.IsValidIndex(Index)) return false;
    Items.RemoveAt(Index);
    if (Items.Num() == 0) SelectedIndex = -1;
    else SelectedIndex = FMath::Clamp(SelectedIndex, 0, Items.Num()-1);
    OnInventoryChanged.Broadcast();
    return true;
}

bool UInventoryComponent::HasKeyWithIndex(int32 KeyIndex) const
{
    for (const FInventoryItem& I : Items)
    {
        if (I.KeyIndex == KeyIndex)
            return true;
    }
    return false;
}

bool UInventoryComponent::ConsumeKeyWithIndex(int32 KeyIndex)
{
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        if (Items[i].KeyIndex == KeyIndex)
        {
            Items.RemoveAt(i);
            if (Items.Num() == 0) SelectedIndex = -1;
            else SelectedIndex = FMath::Clamp(SelectedIndex, 0, Items.Num()-1);
            OnInventoryChanged.Broadcast();
            return true;
        }
    }
    return false;
}

void UInventoryComponent::NavigateSelection(int32 Delta)
{
    if (Items.Num() == 0)
    {
        SelectedIndex = -1;
        OnInventoryChanged.Broadcast();
        return;
    }
    if (SelectedIndex < 0) SelectedIndex = 0;
    SelectedIndex = (SelectedIndex + Delta) % Items.Num();
    if (SelectedIndex < 0) SelectedIndex += Items.Num();
    OnInventoryChanged.Broadcast();
}

void UInventoryComponent::SetSelectedIndex(int32 Index)
{
    if (Items.IsValidIndex(Index))
    {
        SelectedIndex = Index;
        OnInventoryChanged.Broadcast();
    }
    else if (Index < 0)
    {
        SelectedIndex = -1;
        OnInventoryChanged.Broadcast();
    }
}