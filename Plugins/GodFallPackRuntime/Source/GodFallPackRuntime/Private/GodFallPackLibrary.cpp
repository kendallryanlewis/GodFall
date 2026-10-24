#include "GodFallPackLibrary.h"
#include "GodFallPackCharacterPicker.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/CompositeDataTable.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogGodFallPacks, Log, All);

namespace GodFallPacks
{
static UGodFallPackRegistryState* State = nullptr;
#if WITH_EDITOR
static FString PrivateEditorSaveSlot;
#endif
static const FString Prefix = TEXT("/Game/GFExternal/");

static FName ReadName(const UObject* Object, const TCHAR* Name)
{
    const FNameProperty* Property = Object ? FindFProperty<FNameProperty>(Object->GetClass(), Name) : nullptr;
    return Property ? Property->GetPropertyValue_InContainer(Object) : NAME_None;
}

static UObject* ReadObject(const UObject* Object, const TCHAR* Name)
{
    const FObjectPropertyBase* Property = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
    return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
}

static bool ExternalId(FName Id)
{
    return !Id.IsNone() && Id.ToString().StartsWith(TEXT("GFEXT_"), ESearchCase::CaseSensitive);
}

static bool ExternalAsset(const UObject* Object)
{
    return Object && Object->GetPathName().StartsWith(Prefix, ESearchCase::CaseSensitive);
}

static void Issue(UGodFallPackRegistryState* Target, const FString& Message)
{
    Target->Issues.Add(Message);
    UE_LOG(LogGodFallPacks, Warning, TEXT("%s"), *Message);
}

static void Reset()
{
    if (State) { State->RemoveFromRoot(); State = nullptr; }
}

static UGodFallPackRegistryState* Load(const FString& RecordPath)
{
    UGodFallPackRegistryState* Result = NewObject<UGodFallPackRegistryState>();
    Result->AddToRoot();
    Result->Source = RecordPath;
    if (!FPaths::FileExists(RecordPath)) { return Result; }
    FString Text;
    TSharedPtr<FJsonObject> Record;
    if (!FFileHelper::LoadFileToString(Text, *RecordPath) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Record) || !Record.IsValid())
    { Issue(Result, TEXT("Pack ownership record is unreadable; base content remains available.")); return Result; }
    FString Owner;
    const TSharedPtr<FJsonObject>* ManifestPtr = nullptr;
    if (!Record->TryGetStringField(TEXT("owner"), Owner) || Owner != TEXT("GodFallGitHubPacks-v1") ||
        !Record->TryGetObjectField(TEXT("manifest"), ManifestPtr))
    { Issue(Result, TEXT("Pack ownership or manifest is invalid.")); return Result; }
    const TSharedPtr<FJsonObject>& Manifest = *ManifestPtr;
    FString Project, Engine, Platform;
    double Schema = 0;
    const TArray<TSharedPtr<FJsonValue>>* Packs = nullptr;
    if (!Manifest->TryGetNumberField(TEXT("schema_version"), Schema) || Schema != 1 ||
        !Manifest->TryGetStringField(TEXT("project"), Project) || Project != TEXT("GodFall_Clean") ||
        !Manifest->TryGetStringField(TEXT("engine"), Engine) || Engine != TEXT("5.8") ||
        !Manifest->TryGetStringField(TEXT("platform"), Platform) || Platform != TEXT("Win64") ||
        !Manifest->TryGetArrayField(TEXT("packs"), Packs) || Packs->Num() > 256)
    { Issue(Result, TEXT("Unsupported pack manifest contract.")); return Result; }
    if (Packs->IsEmpty()) { return Result; }
    UClass* CharacterSchema = LoadClass<UObject>(nullptr, TEXT("/Game/GF/Characters/Profiles/BP_GF_GodCharacterProfile.BP_GF_GodCharacterProfile_C"));
    UClass* ClothingSchema = LoadClass<UObject>(nullptr, TEXT("/Game/GF/Items/Armor/Data/BP_GF_ArmorDefinition.BP_GF_ArmorDefinition_C"));
    UClass* WeaponSchema = LoadClass<UObject>(nullptr, TEXT("/Game/GF/Weapons/Data/BP_GF_WeaponDefinition.BP_GF_WeaponDefinition_C"));
    if (!CharacterSchema || !ClothingSchema || !WeaponSchema)
    { Issue(Result, TEXT("The base GodFall definition schemas are missing.")); return Result; }
    TSet<FName> PackIds, CharacterIds, ClothingIds, WeaponIds, ItemRowIds;
    for (const TSharedPtr<FJsonValue>& Value : *Packs)
    {
        const TSharedPtr<FJsonObject>* Pack = nullptr;
        FString CatalogPath, Id, Hash;
        double Size = 0;
        if (!Value->TryGetObject(Pack) || !(*Pack)->TryGetStringField(TEXT("catalog"), CatalogPath) ||
            !CatalogPath.StartsWith(Prefix, ESearchCase::CaseSensitive) || CatalogPath.Contains(TEXT("..")) ||
            !(*Pack)->TryGetStringField(TEXT("id"), Id) || Id.Len() != 24 ||
            !(*Pack)->TryGetStringField(TEXT("sha256"), Hash) || Hash.Len() != 64 ||
            !(*Pack)->TryGetNumberField(TEXT("size"), Size) || Size <= 0)
        { Issue(Result, TEXT("Skipping a pack with invalid entry-point metadata.")); continue; }
        const FString Filename = Id + TEXT("_") + Hash.Left(16) + TEXT("_P.pak");
        const FString PakPath = FPaths::GetPath(RecordPath) / Filename;
        if (IFileManager::Get().FileSize(*PakPath) != static_cast<int64>(Size))
        { Issue(Result, FString::Printf(TEXT("Pack %s is not installed completely."), *Id)); continue; }
        UGodFallPackDefinition* Catalog = LoadObject<UGodFallPackDefinition>(nullptr, *CatalogPath);
        if (!Catalog || Catalog->PackId.IsNone() || PackIds.Contains(Catalog->PackId))
        { Issue(Result, FString::Printf(TEXT("Pack %s has a missing or duplicate catalog."), *Id)); continue; }
        TArray<UObject*> Characters, Clothing;
        TArray<UClass*> Weapons;
        TArray<FGodFallPackItemTable> ItemTables;
        TSet<FName> LocalRows;
        TSet<FName> LocalCharacters, LocalClothing, LocalWeapons;
        bool Valid = true;
        for (const TSoftObjectPtr<UObject>& Ref : Catalog->CharacterProfiles)
        {
            UObject* Profile = Ref.LoadSynchronous();
            const FName CharacterId = ReadName(Profile, TEXT("GodId"));
            if (!ExternalAsset(Profile) || !Profile->IsA(CharacterSchema) || !ExternalId(CharacterId) ||
                CharacterIds.Contains(CharacterId) || LocalCharacters.Contains(CharacterId) ||
                (!ReadObject(Profile, TEXT("PresentationActorClass")) && !ReadObject(Profile, TEXT("BodyMesh"))))
            { Valid = false; break; }
            LocalCharacters.Add(CharacterId); Characters.Add(Profile);
        }
        for (const TSoftObjectPtr<UObject>& Ref : Catalog->ClothingDefinitions)
        {
            UObject* Definition = Ref.LoadSynchronous();
            const FName DefinitionId = ReadName(Definition, TEXT("DefinitionId"));
            if (!ExternalAsset(Definition) || !Definition->IsA(ClothingSchema) || !ExternalId(DefinitionId) ||
                ClothingIds.Contains(DefinitionId) || LocalClothing.Contains(DefinitionId) ||
                (!ReadObject(Definition, TEXT("PresentationMesh")) && !ReadObject(Definition, TEXT("PresentationStaticMesh"))))
            { Valid = false; break; }
            LocalClothing.Add(DefinitionId); Clothing.Add(Definition);
        }
        for (const TSoftClassPtr<UObject>& Ref : Catalog->WeaponDefinitions)
        {
            UClass* Class = Ref.LoadSynchronous();
            UObject* Definition = Class ? Class->GetDefaultObject() : nullptr;
            const FName ClassId = ReadName(Definition, TEXT("WeaponClassId"));
            if (!ExternalAsset(Class) || !Class->IsChildOf(WeaponSchema) || !ExternalId(ClassId) ||
                !ExternalId(ReadName(Definition, TEXT("DefinitionId"))) ||
                WeaponIds.Contains(ClassId) || LocalWeapons.Contains(ClassId) ||
                !ReadObject(Definition, TEXT("CombatStyleProfile")) || !ReadObject(Definition, TEXT("PresentationStaticMesh")))
            { Valid = false; break; }
            LocalWeapons.Add(ClassId); Weapons.Add(Class);
        }
        for (const FGodFallPackItemTable& Binding : Catalog->ItemTables)
        {
            UDataTable* Target = Binding.TargetTable.LoadSynchronous();
            UDataTable* Rows = Binding.AdditionalRows.LoadSynchronous();
            if (!Target || !Target->GetPathName().StartsWith(TEXT("/Game/SYN/Content/InventorySystem/DataTables/")) ||
                !ExternalAsset(Rows) || !Target->GetRowStruct() || Target->GetRowStruct() != Rows->GetRowStruct())
            { Valid = false; break; }
            for (const auto& Row : Rows->GetRowMap())
            {
                if (!ExternalId(Row.Key) || Target->GetRowMap().Contains(Row.Key) || ItemRowIds.Contains(Row.Key) || LocalRows.Contains(Row.Key))
                { Valid = false; break; }
                LocalRows.Add(Row.Key);
            }
            if (!Valid) { break; }
            ItemTables.Add(Binding);
        }
        if (!Valid)
        { Issue(Result, FString::Printf(TEXT("Pack %s contains invalid or conflicting definitions; none were registered."), *Id)); continue; }
        PackIds.Add(Catalog->PackId);
        for (UObject* Profile : Characters) { Result->Characters.Add(Profile); CharacterIds.Add(ReadName(Profile, TEXT("GodId"))); }
        for (UObject* Definition : Clothing) { Result->Clothing.Add(Definition); ClothingIds.Add(ReadName(Definition, TEXT("DefinitionId"))); }
        for (UClass* Class : Weapons) { Result->Weapons.Add(Class); WeaponIds.Add(ReadName(Class->GetDefaultObject(), TEXT("WeaponClassId"))); }
        Result->Catalogs.Add(Catalog);
        Result->ItemTables.Append(ItemTables);
        for (const FGodFallPackItemTable& Binding : ItemTables)
        {
            Result->AcquisitionTables.AddUnique(Binding.TargetTable.Get());
            Result->AcquisitionTables.AddUnique(Binding.AdditionalRows.Get());
        }
        ItemRowIds.Append(LocalRows);
    }
    return Result;
}

static UGodFallPackRegistryState* Get()
{
    if (!State) { State = Load(FPaths::ProjectContentDir() / TEXT("Paks/GodFallGitHub/.godfall-github.json")); }
    return State;
}
}

TArray<UObject*> UGodFallPackLibrary::GetExternalCharacterProfiles()
{
    TArray<UObject*> Result;
    for (UObject* Profile : GodFallPacks::Get()->Characters) { Result.Add(Profile); }
    return Result;
}

UObject* UGodFallPackLibrary::FindExternalCharacterProfile(FName CharacterId, TSubclassOf<UObject> ProfileClass)
{
    for (UObject* Profile : GodFallPacks::Get()->Characters)
    { if (GodFallPacks::ReadName(Profile, TEXT("GodId")) == CharacterId && (!ProfileClass || Profile->IsA(ProfileClass))) { return Profile; } }
    return nullptr;
}

bool UGodFallPackLibrary::IsSavedCharacterAvailable(FName CharacterId)
{
    return !GodFallPacks::ExternalId(CharacterId) || FindExternalCharacterProfile(CharacterId) != nullptr;
}

int32 UGodFallPackLibrary::GetExternalClothingCount() { return GodFallPacks::Get()->Clothing.Num(); }
UObject* UGodFallPackLibrary::GetExternalClothingDefinition(int32 Index, TSubclassOf<UObject> DefinitionClass)
{
    const auto* Registry = GodFallPacks::Get();
    UObject* Result = Registry->Clothing.IsValidIndex(Index) ? Registry->Clothing[Index].Get() : nullptr;
    return Result && (!DefinitionClass || Result->IsA(DefinitionClass)) ? Result : nullptr;
}

UObject* UGodFallPackLibrary::GetRuntimeClothingCatalog(UObject* AuthoredCatalog, TSubclassOf<UObject> CatalogClass)
{
    auto* Registry = GodFallPacks::Get();
    if (!AuthoredCatalog || !CatalogClass || !AuthoredCatalog->IsA(CatalogClass) || Registry->Clothing.IsEmpty()) { return AuthoredCatalog; }
    if (const TObjectPtr<UObject>* Existing = Registry->RuntimeCopies.Find(AuthoredCatalog)) { return Existing->Get(); }
    const FArrayProperty* Array = FindFProperty<FArrayProperty>(AuthoredCatalog->GetClass(), TEXT("Definitions"));
    const FObjectPropertyBase* Element = Array ? CastField<FObjectPropertyBase>(Array->Inner) : nullptr;
    if (!Element) { return AuthoredCatalog; }
    for (UObject* Definition : Registry->Clothing) { if (!Definition->IsA(Element->PropertyClass)) { return AuthoredCatalog; } }
    UObject* Copy = DuplicateObject<UObject>(AuthoredCatalog, Registry);
    Copy->SetFlags(RF_Transient);
    FScriptArrayHelper Values(Array, Array->ContainerPtrToValuePtr<void>(Copy));
    for (UObject* Definition : Registry->Clothing)
    { Element->SetObjectPropertyValue(Values.GetRawPtr(Values.AddValue()), Definition); }
    Registry->RuntimeCopies.Add(AuthoredCatalog, Copy);
    return Copy;
}

namespace GodFallPacks
{
static bool ContainsTable(const UDataTable* Root, const UDataTable* Target, TSet<const UDataTable*>& Visited)
{
    if (Root == Target) { return true; }
    if (!Root || Visited.Contains(Root)) { return false; }
    Visited.Add(Root);
    if (const UCompositeDataTable* Composite = Cast<UCompositeDataTable>(Root))
    { for (const UDataTable* Parent : Composite->GetParentTables()) { if (ContainsTable(Parent, Target, Visited)) { return true; } } }
    return false;
}
}

UDataTable* UGodFallPackLibrary::GetRuntimeItemTable(UDataTable* AuthoredTable)
{
    auto* Registry = GodFallPacks::Get();
    if (!AuthoredTable || Registry->ItemTables.IsEmpty()) { return AuthoredTable; }
    if (const TObjectPtr<UObject>* Existing = Registry->RuntimeCopies.Find(AuthoredTable)) { return Cast<UDataTable>(Existing->Get()); }
    TArray<UDataTable*> Sources;
    for (const FGodFallPackItemTable& Binding : Registry->ItemTables)
    {
        TSet<const UDataTable*> Visited;
        UDataTable* Rows = Binding.AdditionalRows.Get();
        if (GodFallPacks::ContainsTable(AuthoredTable, Binding.TargetTable.Get(), Visited) && Rows && Rows->GetRowStruct() == AuthoredTable->GetRowStruct())
        { Sources.Add(Rows); }
    }
    if (Sources.IsEmpty()) { return AuthoredTable; }
    // Preflight the entire merge; never shadow base rows or partially extend a table.
    TSet<FName> Names;
    for (const auto& Row : AuthoredTable->GetRowMap()) { Names.Add(Row.Key); }
    for (const UDataTable* Source : Sources)
    { for (const auto& Row : Source->GetRowMap()) { if (Names.Contains(Row.Key)) { return AuthoredTable; } Names.Add(Row.Key); } }
    UDataTable* Copy = NewObject<UDataTable>(Registry, NAME_None, RF_Transient);
    Copy->RowStruct = AuthoredTable->RowStruct;
    for (const auto& Row : AuthoredTable->GetRowMap()) { Copy->AddRow(Row.Key, Row.Value, Copy->GetRowStruct()); }
    for (const UDataTable* Source : Sources)
    { for (const auto& Row : Source->GetRowMap()) { Copy->AddRow(Row.Key, Row.Value, Copy->GetRowStruct()); } }
    Registry->RuntimeCopies.Add(AuthoredTable, Copy);
    return Copy;
}

bool UGodFallPackLibrary::ExtendGenericWeaponPickupPool(UObject* Pickup)
{
    if (!Pickup || Pickup->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ||
        Pickup->GetClass()->GetPathName() != TEXT("/Game/GF/Weapons/Pickups/BP_GF_WeaponPickup.BP_GF_WeaponPickup_C")) { return false; }
    const FArrayProperty* Array = FindFProperty<FArrayProperty>(Pickup->GetClass(), TEXT("ApprovedDefinitionClasses"));
    const FClassProperty* Element = Array ? CastField<FClassProperty>(Array->Inner) : nullptr;
    if (!Element) { return false; }
    for (UClass* Class : GodFallPacks::Get()->Weapons) { if (!Class->IsChildOf(Element->MetaClass)) { return false; } }
    FScriptArrayHelper Values(Array, Array->ContainerPtrToValuePtr<void>(Pickup));
    for (UClass* Class : GodFallPacks::Get()->Weapons)
    {
        bool Found = false;
        for (int32 Index = 0; Index < Values.Num(); ++Index) { Found |= Element->GetObjectPropertyValue(Values.GetRawPtr(Index)) == Class; }
        if (!Found) { Element->SetObjectPropertyValue(Values.GetRawPtr(Values.AddValue()), Class); }
    }
    return true;
}

TArray<UClass*> UGodFallPackLibrary::GetExternalWeaponDefinitions()
{
    TArray<UClass*> Result;
    for (UClass* Class : GodFallPacks::Get()->Weapons) { Result.Add(Class); }
    return Result;
}

bool UGodFallPackLibrary::RegisterExternalWeaponMetadata(UObject* RuntimeRegistry, FString& Error)
{
    Error.Reset();
    if (!RuntimeRegistry || RuntimeRegistry->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
    { Error = TEXT("Only a constructed runtime weapon registry may be changed."); return false; }
#if WITH_EDITOR
    // Test sessions change only this runtime instance, never the authored GameInstance CDO.
    if (GIsEditor && !GodFallPacks::PrivateEditorSaveSlot.IsEmpty())
    {
        UGameInstance* Instance = RuntimeRegistry->GetTypedOuter<UGameInstance>();
        FStrProperty* Slot = Instance ? FindFProperty<FStrProperty>(Instance->GetClass(), TEXT("SaveSlotName")) : nullptr;
        if (!Slot) { Error = TEXT("Private fixture save boundary is unavailable."); return false; }
        Slot->SetPropertyValue_InContainer(Instance, GodFallPacks::PrivateEditorSaveSlot);
    }
#endif
    struct FBinding { const TCHAR* Map; const TCHAR* Field; };
    static const FBinding Bindings[] = {
        {TEXT("WeaponCombatProfilesByClass"), TEXT("CombatStyleProfile")},
        {TEXT("WeaponMeshesByClass"), TEXT("PresentationStaticMesh")},
        {TEXT("WeaponSocketsByClass"), TEXT("PresentationSocket")},
        {TEXT("WeaponLocationsByClass"), TEXT("PresentationLocation")},
        {TEXT("WeaponRotationsByClass"), TEXT("PresentationRotation")},
        {TEXT("WeaponScalesByClass"), TEXT("PresentationScale")}
    };
    // Preflight every map/definition before applying any new key.
    for (UClass* Class : GodFallPacks::Get()->Weapons)
    {
        UObject* Definition = Class->GetDefaultObject();
        const FName Key = GodFallPacks::ReadName(Definition, TEXT("WeaponClassId"));
        for (const FBinding& Binding : Bindings)
        {
            FMapProperty* Map = FindFProperty<FMapProperty>(RuntimeRegistry->GetClass(), Binding.Map);
            FProperty* Field = FindFProperty<FProperty>(Class, Binding.Field);
            if (!Map || !CastField<FNameProperty>(Map->KeyProp) || !Field || !Map->ValueProp->SameType(Field))
            { Error = FString::Printf(TEXT("Weapon registry schema mismatch at %s."), Binding.Map); return false; }
            FScriptMapHelper Helper(Map, Map->ContainerPtrToValuePtr<void>(RuntimeRegistry));
            const int32 Existing = Helper.FindMapIndexWithKey(&Key);
            if (Existing != INDEX_NONE && !Map->ValueProp->Identical(Helper.GetValuePtr(Existing), Field->ContainerPtrToValuePtr<void>(Definition)))
            { Error = FString::Printf(TEXT("Conflicting existing weapon class %s."), *Key.ToString()); return false; }
        }
    }
    for (UClass* Class : GodFallPacks::Get()->Weapons)
    {
        UObject* Definition = Class->GetDefaultObject();
        const FName Key = GodFallPacks::ReadName(Definition, TEXT("WeaponClassId"));
        for (const FBinding& Binding : Bindings)
        {
            FMapProperty* Map = FindFProperty<FMapProperty>(RuntimeRegistry->GetClass(), Binding.Map);
            FProperty* Field = FindFProperty<FProperty>(Class, Binding.Field);
            FScriptMapHelper Helper(Map, Map->ContainerPtrToValuePtr<void>(RuntimeRegistry));
            Helper.AddPair(&Key, Field->ContainerPtrToValuePtr<void>(Definition));
        }
    }
    return true;
}

FString UGodFallPackLibrary::GetPackRegistryDiagnostics()
{
    const auto* Registry = GodFallPacks::Get();
    return FString::Printf(TEXT("Catalogs=%d Characters=%d Clothing=%d Weapons=%d Issues=%d\n%s"),
        Registry->Catalogs.Num(), Registry->Characters.Num(), Registry->Clothing.Num(), Registry->Weapons.Num(),
        Registry->Issues.Num(), *FString::Join(Registry->Issues, TEXT("\n")));
}

UUserWidget* UGodFallPackLibrary::OpenExternalCharacterPicker(UUserWidget* SelectionWidget, UButton* SourceButton)
{
    if (!SelectionWidget || !SourceButton || GetExternalCharacterProfiles().IsEmpty()) { return nullptr; }
    UObject* Stage = GodFallPacks::ReadObject(SelectionWidget, TEXT("GF_PreviewStage"));
    const FBoolProperty* Busy = Stage ? FindFProperty<FBoolProperty>(Stage->GetClass(), TEXT("GF_Busy")) : nullptr;
    if (Busy && Busy->GetPropertyValue_InContainer(Stage)) { return nullptr; }
    APlayerController* Controller = SelectionWidget->GetOwningPlayer();
    if (!Controller) { return nullptr; }
    UGodFallPackCharacterPicker* Picker = CreateWidget<UGodFallPackCharacterPicker>(Controller);
    if (!Picker) { return nullptr; }
    Picker->Configure(SelectionWidget, SourceButton);
    Picker->AddToViewport(500);
    SelectionWidget->SetVisibility(ESlateVisibility::Hidden);
    FInputModeUIOnly Input;
    Input.SetWidgetToFocus(Picker->TakeWidget());
    Controller->SetInputMode(Input);
    Controller->SetShowMouseCursor(true);
    return Picker;
}

bool UGodFallPackLibrary::InstallExternalCharacterEntry(UUserWidget* SelectionWidget)
{
    if (!SelectionWidget || !SelectionWidget->WidgetTree || GetExternalCharacterProfiles().IsEmpty()) { return false; }
    UWidgetTree* Tree = SelectionWidget->WidgetTree;
    if (Tree->FindWidget(TEXT("GF_PackCharacters"))) { return true; }
    UCanvasPanel* Panel = Cast<UCanvasPanel>(Tree->FindWidget(TEXT("GF_RealmDesign")));
    UButton* Source = Cast<UButton>(Tree->FindWidget(TEXT("GF_Back")));
    if (!Panel || !Source) { return false; }
    UGodFallPackEntryButton* Button = Tree->ConstructWidget<UGodFallPackEntryButton>(UGodFallPackEntryButton::StaticClass(), TEXT("GF_PackCharacters"));
    Button->SetStyle(Source->GetStyle()); Button->Configure(SelectionWidget);
    UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GF_PackCharactersLabel"));
    Label->SetText(NSLOCTEXT("GodFallPacks", "Entry", "ADDITIONAL CHARACTERS"));
    FSlateFontInfo Font = Label->GetFont(); Font.Size = 16;
    if (const UTextBlock* SourceLabel = Cast<UTextBlock>(Source->GetChildAt(0)))
    { Font = SourceLabel->GetFont(); Font.Size = 16; Label->SetColorAndOpacity(SourceLabel->GetColorAndOpacity()); }
    else { Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.75f, 0.4f))); }
    Label->SetFont(Font); Label->SetJustification(ETextJustify::Center); Button->AddChild(Label);
    UCanvasPanelSlot* Slot = Panel->AddChildToCanvas(Button);
    Slot->SetPosition(FVector2D(1228, 900)); Slot->SetSize(FVector2D(372, 36));
    return true;
}

bool UGodFallPackLibrary::ReloadEditorFixture(const FString& OwnershipRecord, const FString& PrivateSaveSlot, FString& Error)
{
    Error.Reset();
#if WITH_EDITOR
    if (!GIsEditor || IsRunningCommandlet()) { Error = TEXT("Interactive editor only."); return false; }
    if (!PrivateSaveSlot.IsEmpty() && !PrivateSaveSlot.StartsWith(TEXT("GF_GitHubPack_")))
    { Error = TEXT("Fixture saves must use the dedicated GF_GitHubPack_ namespace."); return false; }
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        { if (Context.WorldType == EWorldType::PIE) { Error = TEXT("Stop PIE before replacing the fixture registry."); return false; } }
    }
    if (!OwnershipRecord.IsEmpty() && !FPaths::FileExists(OwnershipRecord))
    { Error = TEXT("Fixture ownership record does not exist."); return false; }
    GodFallPacks::Reset();
    GodFallPacks::PrivateEditorSaveSlot = PrivateSaveSlot;
    if (!OwnershipRecord.IsEmpty()) { GodFallPacks::State = GodFallPacks::Load(OwnershipRecord); }
    return true;
#else
    Error = TEXT("Fixture overrides are unavailable in a packaged game."); return false;
#endif
}

class FGodFallPackRuntimeModule : public IModuleInterface
{
public:
    virtual void ShutdownModule() override { GodFallPacks::Reset(); }
};
IMPLEMENT_MODULE(FGodFallPackRuntimeModule, GodFallPackRuntime)
