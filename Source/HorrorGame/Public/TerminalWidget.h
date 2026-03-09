#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TerminalWidget.generated.h"

class UTextBlock;

UCLASS()
class HORRORGAME_API UTerminalWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable)
    void SetDisplayedText(const FString& Text);

    UFUNCTION(BlueprintPure)
    FString GetDisplayedText() const;

    UPROPERTY(BlueprintReadWrite, meta=(BindWidget))
    class UTextBlock* TextBlock_0;

private:

    FString DisplayedText;
};