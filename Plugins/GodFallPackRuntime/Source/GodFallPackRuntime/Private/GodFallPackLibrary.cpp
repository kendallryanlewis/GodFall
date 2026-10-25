#include "GodFallPackLibrary.h"
#include "GodFallPackCharacterPicker.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/CompositeDataTable.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogGodFallPacks, Log, All);

namespace GodFallPacks
{
static UGodFallPackRegistryState* State = nullptr;
#if WITH_EDITOR
static FString PrivateEditorSaveSlot;
#endif

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

static void Issue(UGodFallPackRegistryState* Target, const FString& Message)
{
    Target->Issues.Add(Message);
    UE_LOG(LogGodFallPacks, Warning, TEXT("%s"), *Message);
}

static void Reset()
{
    if (State) { State->RemoveFromRoot(); State = nullptr; }
}

static UGodFallPackRegistryState* Get()
{
    if (!State)
    {
        // Compatibility state remains empty: Unreal-authored assets own acquisition.
        // Never read old GitHub manifests or admit previously downloaded caches.
        State = NewObject<UGodFallPackRegistryState>();
        State->AddToRoot();
        State->Source = TEXT("Unreal-authored catalogs");
        State->bRemoteAcquisitionAvailable = false;
        UE_LOG(LogGodFallPacks, Display, TEXT("Registry ready: Catalogs=%d Characters=%d Clothing=%d Weapons=%d Issues=%d SavedDir=%s"),
            State->Catalogs.Num(), State->Characters.Num(), State->Clothing.Num(), State->Weapons.Num(), State->Issues.Num(), *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()));
    }
    return State;
}
}

bool UGodFallPackLibrary::IsEditorPackSyncAllowed()
{
    return false; // Retained for callers of the retired editor downloader.
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
    {
        for (const auto& Row : Source->GetRowMap())
        {
            if (Names.Contains(Row.Key)) { return AuthoredTable; }
            if (FindExternalWeaponForItemId(Row.Key))
            {
                const UStruct* ItemInfoStruct = Source->GetRowStruct();
                const void* ItemInfoData = Row.Value;
                for (TFieldIterator<FStructProperty> Property(Source->GetRowStruct()); Property; ++Property)
                {
                    if (Property->GetName().StartsWith(TEXT("BaseInfo")))
                    { ItemInfoStruct = Property->Struct; ItemInfoData = Property->ContainerPtrToValuePtr<void>(Row.Value); break; }
                }
                const FIntProperty* StackSize = nullptr;
                for (TFieldIterator<FIntProperty> Property(ItemInfoStruct); Property; ++Property)
                { if (Property->GetName().StartsWith(TEXT("MaxStackSize"))) { StackSize = *Property; break; } }
                if (!StackSize || StackSize->GetPropertyValue_InContainer(ItemInfoData) != 1)
                {
                    GodFallPacks::Issue(Registry, FString::Printf(TEXT("Weapon acquisition row %s must have MaxStackSize=1; table extension skipped."), *Row.Key.ToString()));
                    Registry->RuntimeCopies.Add(AuthoredTable, AuthoredTable);
                    return AuthoredTable;
                }
            }
            Names.Add(Row.Key);
        }
    }
    UDataTable* Copy = NewObject<UDataTable>(Registry, NAME_None, RF_Transient);
    Copy->RowStruct = AuthoredTable->RowStruct;
    for (const auto& Row : AuthoredTable->GetRowMap()) { Copy->AddRow(Row.Key, Row.Value, Copy->GetRowStruct()); }
    for (const UDataTable* Source : Sources)
    { for (const auto& Row : Source->GetRowMap()) { Copy->AddRow(Row.Key, Row.Value, Copy->GetRowStruct()); } }
    Registry->RuntimeCopies.Add(AuthoredTable, Copy);
    return Copy;
}

bool UGodFallPackLibrary::ShouldPreferGitHubItems()
{
    return false; // GitHub is version control, not a runtime item source.
}

bool UGodFallPackLibrary::IsPreferredLootDefinition(UObject* Definition)
{
    return IsValid(Definition);
}

bool UGodFallPackLibrary::ExtendGenericWeaponPickupPool(UObject* Pickup)
{
    if (!Pickup || Pickup->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) { return false; }
    const FArrayProperty* Array = FindFProperty<FArrayProperty>(Pickup->GetClass(), TEXT("ApprovedDefinitionClasses"));
    const FClassProperty* Element = Array ? CastField<FClassProperty>(Array->Inner) : nullptr;
    if (!Element) { return false; }
    FScriptArrayHelper Values(Array, Array->ContainerPtrToValuePtr<void>(Pickup));
    // Both generic and specialized pickups keep their authored arrays unchanged.
    return Values.Num() > 0;
}

TArray<UClass*> UGodFallPackLibrary::GetExternalWeaponDefinitions()
{
    TArray<UClass*> Result;
    for (UClass* Class : GodFallPacks::Get()->Weapons) { Result.Add(Class); }
    return Result;
}

UObject* UGodFallPackLibrary::FindExternalWeaponForItemId(FName ItemId, TSubclassOf<UObject> DefinitionClass)
{
    for (UClass* Class : GodFallPacks::Get()->Weapons)
    {
        UObject* Definition = Class->GetDefaultObject();
        if (GodFallPacks::ReadName(Definition, TEXT("DefinitionId")) == ItemId && (!DefinitionClass || Definition->IsA(DefinitionClass)))
        { return Definition; }
    }
    return nullptr;
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
    const UCanvasPanelSlot* SourceSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(Source);
    Slot->SetZOrder(SourceSlot ? SourceSlot->GetZOrder() : 6);
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
    if (!OwnershipRecord.IsEmpty())
    { Error = TEXT("External pack admission has been removed; use Unreal-authored items."); return false; }
    GodFallPacks::Reset();
    GodFallPacks::PrivateEditorSaveSlot = PrivateSaveSlot;
    return true;
#else
    Error = TEXT("Fixture overrides are unavailable in a packaged game."); return false;
#endif
}

class FGodFallPackRuntimeModule : public IModuleInterface
{
public:
    virtual void ShutdownModule() override
    {
        // Engine shutdown destroys UObjects before unloading runtime modules.
        if (!IsEngineExitRequested()) { GodFallPacks::Reset(); }
        else { GodFallPacks::State = nullptr; }
    }
};
IMPLEMENT_MODULE(FGodFallPackRuntimeModule, GodFallPackRuntime)
