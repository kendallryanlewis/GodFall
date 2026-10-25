#include "GodFallEquipmentDeletion.h"
#include "GodFallEquipmentCatalog.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

int32 UGodFallEquipmentDeletion::PurgeDeletedEquipment(UObject* Owner,bool Persist)
{
    const auto* Catalog=UGodFallEquipmentCatalog::Get();
    if(!Catalog||Catalog->RetiredItemPrefixes.IsEmpty())return 0;
    if(!Owner||Owner->HasAnyFlags(RF_ClassDefaultObject))return -1;
    const FString Type=Owner->GetClass()->GetPathName();
    if(Type!=TEXT("/Game/GF/Core/GameInstance/BP_GF_GameInstance.BP_GF_GameInstance_C")&&
       Type!=TEXT("/Game/GF/Core/Save/BP_GF_SaveGame.BP_GF_SaveGame_C"))return -1;
    auto Retired=[&](FName Id)
    {
        FString Prefix,Suffix;
        return Id.ToString().Split(TEXT("@"),&Prefix,&Suffix)&&!Suffix.IsEmpty()&&Catalog->RetiredItemPrefixes.Contains(FName(*Prefix));
    };
    auto* Inventory=FindFProperty<FArrayProperty>(Owner->GetClass(),TEXT("PersistedInventory"));
    auto* Item=Inventory?CastField<FStructProperty>(Inventory->Inner):nullptr;
    FNameProperty* Id=nullptr;
    if(Item)for(TFieldIterator<FNameProperty> It(Item->Struct);It;++It)
        if(It->GetName()==TEXT("ID")||It->GetName().StartsWith(TEXT("ID_"))){if(Id)return -1;Id=*It;}
    auto* Hidden=FindFProperty<FArrayProperty>(Owner->GetClass(),TEXT("ConcealedArmorItemIds"));
    auto* HiddenName=Hidden?CastField<FNameProperty>(Hidden->Inner):nullptr;
    auto* Powers=FindFProperty<FMapProperty>(Owner->GetClass(),TEXT("ItemPowerByInstanceId"));
    auto* PowerKey=Powers?CastField<FNameProperty>(Powers->KeyProp):nullptr;
    TArray<FNameProperty*> Selections;
    for(const TCHAR* Name:{TEXT("BodysuitInstanceId"),TEXT("HaloInstanceId"),TEXT("BackAttachmentInstanceId")})
    {auto* P=FindFProperty<FNameProperty>(Owner->GetClass(),Name);if(!P)return -1;Selections.Add(P);}
    auto* Style=FindFProperty<FIntProperty>(Owner->GetClass(),TEXT("SavedCombatStyleIndex"));
    auto* LastStyle=FindFProperty<FIntProperty>(Owner->GetClass(),TEXT("SavedLastWeaponStyleIndex"));
    UFunction* Save=Persist?Owner->FindFunction(TEXT("SavePlaythroughToDisk")):nullptr;
    auto* SaveSucceeded=Save?FindFProperty<FBoolProperty>(Save,TEXT("SaveSucceeded")):nullptr;
    auto* Slot=FindFProperty<FStrProperty>(Owner->GetClass(),TEXT("SaveSlotName"));
    if(!Id||!HiddenName||!PowerKey||!Style||!LastStyle||(Persist&&(!Save||!SaveSucceeded||!Slot)))return -1;
    FScriptArrayHelper Values(Inventory,Inventory->ContainerPtrToValuePtr<void>(Owner));
    FScriptArrayHelper Concealed(Hidden,Hidden->ContainerPtrToValuePtr<void>(Owner));
    FScriptMapHelper PowerValues(Powers,Powers->ContainerPtrToValuePtr<void>(Owner));
    TArray<int32> Remove;
    for(int32 I=0;I<Values.Num();++I)if(Retired(Id->GetPropertyValue_InContainer(Values.GetRawPtr(I))))Remove.Add(I);
    bool Changed=!Remove.IsEmpty();
    for(const auto* P:Selections)Changed|=Retired(P->GetPropertyValue_InContainer(Owner));
    for(int32 I=0;I<Concealed.Num();++I)Changed|=Retired(HiddenName->GetPropertyValue(Concealed.GetRawPtr(I)));
    for(int32 I=0;I<PowerValues.GetMaxIndex();++I)if(PowerValues.IsValidIndex(I))Changed|=Retired(PowerKey->GetPropertyValue(PowerValues.GetKeyPtr(I)));
    if(!Changed)return 0;
    // Back up the affected on-disk save before changing its in-memory authority.
    if(Persist)
    {
        const FString Name=Slot->GetPropertyValue_InContainer(Owner);
        if(Name.IsEmpty()||Name!=FPaths::GetCleanFilename(Name)||Name.Contains(TEXT("..")))return -1;
        const FString Source=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames"),Name+TEXT(".sav"));
        const FString Backup=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("EquipmentDeletionBackups"),Name+TEXT("_")+FGuid::NewGuid().ToString()+TEXT(".sav"));
        if(!IFileManager::Get().FileExists(*Source)||!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true)||
           IFileManager::Get().Copy(*Backup,*Source,false,true)!=COPY_OK)return -1;
    }
    for(int32 I:Remove)
    {
        Item->DestroyValue(Values.GetRawPtr(I));Item->InitializeValue(Values.GetRawPtr(I));
        // Weapon slots occupy the first two positions; other equipment and bags
        // must not alter the player's remembered surviving weapon style.
        if(I<2 && Style->GetPropertyValue_InContainer(Owner)==I+1)Style->SetPropertyValue_InContainer(Owner,0);
        if(I<2 && LastStyle->GetPropertyValue_InContainer(Owner)==I+1)
            LastStyle->SetPropertyValue_InContainer(Owner,I==0&&!Remove.Contains(1)?2:1);
    }
    for(auto* P:Selections)if(Retired(P->GetPropertyValue_InContainer(Owner)))P->SetPropertyValue_InContainer(Owner,NAME_None);
    for(int32 I=Concealed.Num()-1;I>=0;--I)if(Retired(HiddenName->GetPropertyValue(Concealed.GetRawPtr(I))))Concealed.RemoveValues(I);
    for(int32 I=PowerValues.GetMaxIndex()-1;I>=0;--I)if(PowerValues.IsValidIndex(I)&&Retired(PowerKey->GetPropertyValue(PowerValues.GetKeyPtr(I))))PowerValues.RemoveAt(I);
    if(Persist)
    {
        FStructOnScope Params(Save);Owner->ProcessEvent(Save,Params.GetStructMemory());
        if(!SaveSucceeded->GetPropertyValue_InContainer(Params.GetStructMemory()))
        {
            UE_LOG(LogTemp,Error,TEXT("GodFall equipment deletion: cleaned this session, but could not persist the save. Original backup retained."));
            return -1;
        }
    }
    return Remove.Num();
}
