#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "GodFallFreeStore.generated.h"

class UTextBlock;
class UGodFallFreeStore;

UCLASS()
class UGodFallFreeStoreEntry : public UButton
{
    GENERATED_BODY()
public:
    void Configure(UUserWidget* InMenu);
private:
    UPROPERTY(Transient) TObjectPtr<UUserWidget> Menu;
    UFUNCTION() void OpenStore();
};

UCLASS()
class UGodFallFreeStoreChoice : public UButton
{
    GENERATED_BODY()
public:
    void Configure(UObject* InDefinition, UGodFallFreeStore* InStore);
private:
    UPROPERTY(Transient) TObjectPtr<UObject> Definition;
    UPROPERTY(Transient) TObjectPtr<UGodFallFreeStore> Store;
    UFUNCTION() void Claim();
};

/** A free catalog overlay; existing factories and purchase/save transaction own items. */
UCLASS()
class GODFALLPACKRUNTIME_API UGodFallFreeStore : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(UUserWidget* InMenu,UButton* InSource);
    void ConfigureWorldInteraction();
    void ClaimDefinition(UObject* Definition);
    UFUNCTION(BlueprintCallable, Category="GodFall|Store") void CloseStore();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
    UPROPERTY(Transient) TObjectPtr<UUserWidget> Menu;
    UPROPERTY(Transient) TObjectPtr<UButton> Source;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
    UPROPERTY(Transient) TObjectPtr<UButton> Close;
    UPROPERTY(Transient) TObjectPtr<UButton> FirstChoice;
    ESlateVisibility PreviousVisibility=ESlateVisibility::Visible;
    bool bWorldInteraction=false;
    void RestoreWorldInput();
};
