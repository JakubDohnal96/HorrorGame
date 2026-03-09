#pragma once

#include "Blueprint/UserWidget.h"
#include "InventoryComponent.h"
#include "InventoryWidgetBase.generated.h"

UCLASS()
class HORRORGAME_API UInventoryWidgetBase : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintImplementableEvent)
    void RefreshInventory(const TArray<FInventoryItem>& Items, int32 SelectedIndex);
};