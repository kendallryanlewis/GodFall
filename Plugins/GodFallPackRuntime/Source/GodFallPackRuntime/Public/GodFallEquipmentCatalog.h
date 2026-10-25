#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GodFallEquipmentCatalog.generated.h"

/** Acquisition settings only. Definitions remain the authority for item stats and presentation. */
USTRUCT(BlueprintType)
struct GODFALLPACKRUNTIME_API FGodFallEquipmentEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equipment") TSoftObjectPtr<UObject> Armor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equipment") TSoftClassPtr<UObject> Weapon;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equipment") bool bEnabled = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equipment") bool bLoot = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equipment") bool bFreeStore = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equipment") bool bGenericWeaponLoot = true;
    UObject* Resolve() const;
    FString Key() const;
};

UCLASS(BlueprintType)
class GODFALLPACKRUNTIME_API UGodFallEquipmentCatalog : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equipment") TArray<FGodFallEquipmentEntry> Items;
    /** Stable instance-ID prefixes retired by permanent deletion. Never reuse them. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Equipment") TArray<FName> RetiredItemPrefixes;
    static UGodFallEquipmentCatalog* Get();
    const FGodFallEquipmentEntry* Find(const UObject* Definition) const;
    static FName DefinitionId(const UObject* Definition);
    static bool AllowsLoot(const UObject* Definition);
    static TArray<UClass*> WeaponClasses(bool bLootOnly);
};
