#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "GodFallPackCharacterPicker.generated.h"

class UTextBlock;
class UGodFallPackCharacterPicker;

UCLASS()
class UGodFallPackCharacterChoice : public UButton
{
    GENERATED_BODY()
public:
    void ConfigureChoice(UObject* InProfile, UGodFallPackCharacterPicker* InPicker);
private:
    UPROPERTY(Transient) TObjectPtr<UObject> Profile;
    UPROPERTY(Transient) TObjectPtr<UGodFallPackCharacterPicker> Picker;
    UFUNCTION() void HandleChoice();
};

/** Additional character selection only; base-game carousel remains authored. */
UCLASS()
class GODFALLPACKRUNTIME_API UGodFallPackCharacterPicker : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(UUserWidget* InSelection, UButton* InSourceButton);
    void SelectProfile(UObject* Profile);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    UPROPERTY(Transient) TObjectPtr<UUserWidget> Selection;
    UPROPERTY(Transient) TObjectPtr<UButton> SourceButton;
    UPROPERTY(Transient) TObjectPtr<UObject> SelectedProfile;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
    UPROPERTY(Transient) TObjectPtr<UButton> Confirm;
    UPROPERTY(Transient) TObjectPtr<UButton> Back;
    ESlateVisibility PreviousVisibility = ESlateVisibility::Visible;
    UFUNCTION() void ConfirmSelection();
    UFUNCTION() void ClosePicker();
};
