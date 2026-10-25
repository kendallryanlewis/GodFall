#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GodFallEquipmentDeletion.generated.h"

UCLASS()
class GODFALLPACKRUNTIME_API UGodFallEquipmentDeletion : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Called after save fields are loaded and before any player restores equipment.
     * Clears only retired identities, preserves inventory slot positions and all
     * unrelated payloads. Persist uses the existing GameInstance save authority. */
    UFUNCTION(BlueprintCallable, Category="GodFall|Equipment")
    static int32 PurgeDeletedEquipment(UObject* SaveOwner, bool Persist = true);
};
