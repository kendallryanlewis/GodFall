#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GodFallEquipmentEditorLibrary.generated.h"

UCLASS()
class GODFALLEQUIPMENTEDITOR_API UGodFallEquipmentEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="GodFall|Editor") static void OpenManager();
    UFUNCTION(BlueprintCallable, Category="GodFall|Editor") static bool RefreshCatalog(FString& Error);
    UFUNCTION(BlueprintCallable, Category="GodFall|Editor") static bool SetAvailability(UObject* Definition,bool Enabled,bool Loot,bool FreeStore,bool GenericWeaponLoot,FString& Error);
    UFUNCTION(BlueprintCallable, Category="GodFall|Editor") static bool SelectDefinition(UObject* Definition);
    UFUNCTION(BlueprintCallable, Category="GodFall|Editor") static bool SaveDefinition(UObject* Definition,FString& Error);
    UFUNCTION(BlueprintCallable, Category="GodFall|Editor") static bool ValidateCatalog(FString& Error);
    UFUNCTION(BlueprintCallable, Category="GodFall|Editor") static bool CaptureEditorWindow(const FString& Filename);
    UFUNCTION(BlueprintCallable, Category="GodFall|Editor") static bool MaximizeManagerWindow();
};
