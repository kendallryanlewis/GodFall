#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GodFallPackLibrary.generated.h"

class UButton;
class UUserWidget;

/** Authored acquisition rows extend a named base table without replacing its rows. */
USTRUCT(BlueprintType)
struct GODFALLPACKRUNTIME_API FGodFallPackItemTable
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GodFall Pack") TSoftObjectPtr<UDataTable> TargetTable;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GodFall Pack") TSoftObjectPtr<UDataTable> AdditionalRows;
};

/** One cooked pack's entry point. References use existing GodFall schemas. */
UCLASS(BlueprintType)
class GODFALLPACKRUNTIME_API UGodFallPackDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GodFall Pack")
    FName PackId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GodFall Pack", meta=(AssetBundles="Runtime"))
    TArray<TSoftObjectPtr<UObject>> CharacterProfiles;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GodFall Pack", meta=(AssetBundles="Runtime"))
    TArray<TSoftObjectPtr<UObject>> ClothingDefinitions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GodFall Pack", meta=(AssetBundles="Runtime"))
    TArray<TSoftClassPtr<UObject>> WeaponDefinitions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GodFall Pack", meta=(AssetBundles="Runtime"))
    TArray<FGodFallPackItemTable> ItemTables;
};

/** Strong ownership of loaded definitions without editing authored assets. */
UCLASS(Transient)
class UGodFallPackRegistryState : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TArray<TObjectPtr<UGodFallPackDefinition>> Catalogs;
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> Characters;
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> Clothing;
    UPROPERTY(Transient) TArray<TObjectPtr<UClass>> Weapons;
    UPROPERTY(Transient) TArray<FGodFallPackItemTable> ItemTables;
    UPROPERTY(Transient) TArray<TObjectPtr<UDataTable>> AcquisitionTables;
    UPROPERTY(Transient) TMap<TObjectPtr<UObject>, TObjectPtr<UObject>> RuntimeCopies;
    FString Source;
    bool bRemoteAcquisitionAvailable = true;
    TArray<FString> Issues;
};

UCLASS()
class GODFALLPACKRUNTIME_API UGodFallPackLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Compatibility API: automatic pack synchronization has been removed; always false. */
    UFUNCTION(BlueprintPure, Category="GodFall|Packs")
    static bool IsEditorPackSyncAllowed();

    /** Append-only external profiles; the base game's profiles retain their ownership. */
    UFUNCTION(BlueprintPure, Category="GodFall|Packs")
    static TArray<UObject*> GetExternalCharacterProfiles();

    UFUNCTION(BlueprintPure, Category="GodFall|Packs", meta=(DeterminesOutputType="ProfileClass"))
    static UObject* FindExternalCharacterProfile(FName CharacterId, TSubclassOf<UObject> ProfileClass = nullptr);

    /** Legacy/base IDs preserve existing behavior; absent GFEXT_ IDs cannot continue. */
    UFUNCTION(BlueprintPure, Category="GodFall|Packs")
    static bool IsSavedCharacterAvailable(FName CharacterId);

    UFUNCTION(BlueprintPure, Category="GodFall|Packs")
    static int32 GetExternalClothingCount();

    UFUNCTION(BlueprintPure, Category="GodFall|Packs", meta=(DeterminesOutputType="DefinitionClass"))
    static UObject* GetExternalClothingDefinition(int32 Index, TSubclassOf<UObject> DefinitionClass = nullptr);

    /** A transient catalog copy includes pack definitions; authored arrays remain untouched. */
    UFUNCTION(BlueprintPure, Category="GodFall|Packs", meta=(DeterminesOutputType="CatalogClass"))
    static UObject* GetRuntimeClothingCatalog(UObject* AuthoredCatalog, TSubclassOf<UObject> CatalogClass);

    /** Includes authored pack rows using the existing S_Item acquisition schema and rules. */
    UFUNCTION(BlueprintPure, Category="GodFall|Packs")
    static UDataTable* GetRuntimeItemTable(UDataTable* AuthoredTable);

    /** Compatibility API: GitHub acquisition is retired; always false. */
    UFUNCTION(BlueprintPure, Category="GodFall|Packs")
    static bool ShouldPreferGitHubItems();

    /** Validate a definition from the Unreal-authored acquisition lists. */
    UFUNCTION(BlueprintPure, Category="GodFall|Packs")
    static bool IsPreferredLootDefinition(UObject* Definition);

    /** Validate the authored pickup pool without replacing it. False means no candidate. */
    UFUNCTION(BlueprintCallable, Category="GodFall|Packs", meta=(DefaultToSelf="Pickup"))
    static bool ExtendGenericWeaponPickupPool(UObject* Pickup);

    UFUNCTION(BlueprintPure, Category="GodFall|Packs")
    static TArray<UClass*> GetExternalWeaponDefinitions();

    /** Acquisition row names match authored weapon DefinitionIds; returned policy stays immutable. */
    UFUNCTION(BlueprintPure, Category="GodFall|Packs", meta=(DeterminesOutputType="DefinitionClass"))
    static UObject* FindExternalWeaponForItemId(FName ItemId, TSubclassOf<UObject> DefinitionClass = nullptr);

    /** Populate only a newly constructed runtime registry's six metadata maps. */
    UFUNCTION(BlueprintCallable, Category="GodFall|Packs")
    static bool RegisterExternalWeaponMetadata(UObject* RuntimeRegistry, FString& Error);

    UFUNCTION(BlueprintPure, Category="GodFall|Packs")
    static FString GetPackRegistryDiagnostics();

    /** Additive picker: it leaves the base carousel and its camera actors unchanged. */
    UFUNCTION(BlueprintCallable, Category="GodFall|Packs")
    static UUserWidget* OpenExternalCharacterPicker(UUserWidget* SelectionWidget, UButton* SourceButton);

    /** Builds one conditional runtime entry in the selection screen's existing canvas. */
    UFUNCTION(BlueprintCallable, Category="GodFall|Packs", meta=(DefaultToSelf="SelectionWidget"))
    static bool InstallExternalCharacterEntry(UUserWidget* SelectionWidget);

    /** Additive entry in the existing pause menu, only in the Vertical Slice. */
    UFUNCTION(BlueprintCallable, Category="GodFall|Store", meta=(DefaultToSelf="Menu"))
    static bool InstallFreeStoreEntry(UUserWidget* Menu);

    UFUNCTION(BlueprintCallable, Category="GodFall|Store")
    static UUserWidget* OpenFreeStore(UUserWidget* Menu, UButton* SourceButton);

    /** Unreal armor catalog and local Sword/Dagger definitions; no startup grants. */
    UFUNCTION(BlueprintPure, Category="GodFall|Store")
    static TArray<UObject*> GetFreeStoreDefinitions();

    /** Once-rolled permanent item through the existing zero-price purchase/save authority. */
    UFUNCTION(BlueprintCallable, Category="GodFall|Store", meta=(WorldContext="WorldContextObject"))
    static bool ClaimFreeStoreItem(UObject* WorldContextObject, UObject* Definition, FName& InstanceId, FString& Error);

    /** Editor-only private save override; rejects external records, PIE and packaged games. */
    UFUNCTION(BlueprintCallable, Category="GodFall|Packs|Testing", meta=(DevelopmentOnly))
    static bool ReloadEditorFixture(const FString& OwnershipRecord, const FString& PrivateSaveSlot, FString& Error);
};
