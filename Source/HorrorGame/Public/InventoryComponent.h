#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

USTRUCT(BlueprintType)
struct FInventoryItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item")
    int32 KeyIndex = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item")
    UStaticMesh* ItemMesh = nullptr;

    FInventoryItem()
        : DisplayName(FText::FromString("Empty"))
        , KeyIndex(-1)
        , ItemMesh(nullptr)
    {}

    FInventoryItem(const FText& Name, int32 InKeyIndex, UStaticMesh* InMesh = nullptr)
        : DisplayName(Name)
        , KeyIndex(InKeyIndex)
        , ItemMesh(InMesh)
    {}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class HORRORGAME_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInventoryComponent();

    // Inventory slots (visible in editor for default content if desired)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
    TArray<FInventoryItem> Items;

    // index of currently selected slot (-1 = none)
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 SelectedIndex;

    // Event broadcast when inventory changes (UI should bind to this)
    UPROPERTY(BlueprintAssignable, Category="Inventory")
    FOnInventoryChanged OnInventoryChanged;

    // Add an item (returns slot index)
    UFUNCTION(BlueprintCallable, Category="Inventory")
    int32 AddItem(const FInventoryItem& Item);

    // Remove item at slot
    UFUNCTION(BlueprintCallable, Category="Inventory")
    bool RemoveItemAt(int32 Index);

    // Return whether we have a key with this key index
    UFUNCTION(BlueprintCallable, Category="Inventory")
    bool HasKeyWithIndex(int32 KeyIndex) const;

    // Consume (remove) one key with given index (returns true if consumed)
    UFUNCTION(BlueprintCallable, Category="Inventory")
    bool ConsumeKeyWithIndex(int32 KeyIndex);

    // Navigate selection by delta (e.g., -1 left, +1 right). Wrap or clamp as you want.
    UFUNCTION(BlueprintCallable, Category="Inventory")
    void NavigateSelection(int32 Delta);

    UFUNCTION(BlueprintCallable, Category="Inventory")
    void SetSelectedIndex(int32 Index);

    UFUNCTION(BlueprintCallable, Category="Inventory")
    int32 GetSelectedIndex() const { return SelectedIndex; }

protected:
    virtual void BeginPlay() override;




};