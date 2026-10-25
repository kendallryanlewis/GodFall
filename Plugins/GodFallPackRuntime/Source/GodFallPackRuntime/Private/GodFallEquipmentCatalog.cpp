#include "GodFallEquipmentCatalog.h"
#include "UObject/UnrealType.h"

UObject* FGodFallEquipmentEntry::Resolve() const
{
    if(!Weapon.IsNull()) { UClass* Type=Weapon.LoadSynchronous(); return Type?Type->GetDefaultObject():nullptr; }
    return Armor.LoadSynchronous();
}
FString FGodFallEquipmentEntry::Key() const { return Weapon.IsNull()?Armor.ToString():Weapon.ToString(); }
UGodFallEquipmentCatalog* UGodFallEquipmentCatalog::Get()
{
    return LoadObject<UGodFallEquipmentCatalog>(nullptr,TEXT("/Game/GF/Items/DA_GF_EquipmentManager.DA_GF_EquipmentManager"));
}
const FGodFallEquipmentEntry* UGodFallEquipmentCatalog::Find(const UObject* Definition) const
{
    if(!Definition)return nullptr;
    const FString Key=Definition->HasAnyFlags(RF_ClassDefaultObject)?Definition->GetClass()->GetPathName():Definition->GetPathName();
    return Items.FindByPredicate([&](const FGodFallEquipmentEntry& Entry){return Entry.Key()==Key;});
}
FName UGodFallEquipmentCatalog::DefinitionId(const UObject* Definition)
{
    const auto* Property=Definition?FindFProperty<FNameProperty>(Definition->GetClass(),TEXT("DefinitionId")):nullptr;
    return Property?Property->GetPropertyValue_InContainer(Definition):NAME_None;
}
bool UGodFallEquipmentCatalog::AllowsLoot(const UObject* Definition)
{
    if(!IsValid(Definition))return false;
    if(const auto* Catalog=Get())
    {
        const auto* Entry=Catalog->Find(Definition);
        return Entry&&Entry->bEnabled&&Entry->bLoot;
    }
    return true; // Existing project remains usable until initial editor setup is saved.
}
TArray<UClass*> UGodFallEquipmentCatalog::WeaponClasses(bool bLootOnly)
{
    TArray<UClass*> Result;
    if(const auto* Catalog=Get())for(const auto& Entry:Catalog->Items)
        if(!Entry.Weapon.IsNull()&&(!bLootOnly||(Entry.bEnabled&&Entry.bLoot)))
            if(UClass* Type=Entry.Weapon.LoadSynchronous())Result.AddUnique(Type);
    return Result;
}
