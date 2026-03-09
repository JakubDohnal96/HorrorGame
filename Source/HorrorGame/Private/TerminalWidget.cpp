#include "TerminalWidget.h"
#include "Components/TextBlock.h"

FString UTerminalWidget::GetDisplayedText() const
{
    return DisplayedText;
}

void UTerminalWidget::SetDisplayedText(const FString& Text)
{
    DisplayedText = Text;

    UE_LOG(LogTemp, Warning, TEXT("SetDisplayedText called with: %s"), *Text);

    if (TextBlock_0)
    {
        UE_LOG(LogTemp, Warning, TEXT("TextBlock_0 found, setting text..."));
        TextBlock_0->SetText(FText::FromString(Text));
        UE_LOG(LogTemp, Warning, TEXT("Text set done."));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("TextBlock_0 is NULL — name mismatch or BindWidget failed"));
    }
}