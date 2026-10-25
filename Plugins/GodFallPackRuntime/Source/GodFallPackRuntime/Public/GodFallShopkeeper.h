#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GodFallShopkeeper.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UTextRenderComponent;
class UAnimSequence;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UGodFallFreeStore;

/** Placed, non-combat quartermaster. Inventory factories still own every claim. */
UCLASS()
class GODFALLPACKRUNTIME_API AGodFallShopkeeper : public AActor
{
    GENERATED_BODY()
public:
    AGodFallShopkeeper();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintPure, Category="GodFall|Store") bool CanInteract() const;
    UFUNCTION(BlueprintCallable, Category="GodFall|Store") void Interact();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GodFall|Store") TObjectPtr<UCapsuleComponent> Capsule;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GodFall|Store") TObjectPtr<USkeletalMeshComponent> CharacterMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GodFall|Store") TObjectPtr<UTextRenderComponent> Prompt;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GodFall|Store") TObjectPtr<UAnimSequence> IdleAnimation;
    UPROPERTY(Transient, BlueprintReadOnly, Category="GodFall|Store") TObjectPtr<UInputAction> InteractAction;
    UPROPERTY(Transient, BlueprintReadOnly, Category="GodFall|Store") TObjectPtr<UGodFallFreeStore> ActiveStore;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void BindLocalInput();
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> InteractionMapping;
    UPROPERTY(Transient) TObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
    UPROPERTY(Transient) TObjectPtr<APlayerController> LocalController;
};
