#include "GodFallEquipmentEditorLibrary.h"
#include "GodFallEquipmentCatalog.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "Editor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AdvancedPreviewScene.h"
#include "SEditorViewport.h"
#include "EditorViewportClient.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "IPythonScriptPlugin.h"
#include "Misc/PackageName.h"
#include "Misc/MessageDialog.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"

namespace GFEquipment
{
static const FName TabId(TEXT("GodFallEquipmentManager"));
static const TCHAR* PolicyPackage=TEXT("/Game/GF/Items/DA_GF_EquipmentManager");
static bool CanEdit(){return GEditor&&!GEditor->PlayWorld&&!GEditor->bIsSimulatingInEditor;}
static UObject* Object(const UObject* Owner,const TCHAR* Name)
{
    const auto* P=Owner?FindFProperty<FObjectPropertyBase>(Owner->GetClass(),Name):nullptr;
    return P?P->GetObjectPropertyValue_InContainer(Owner):nullptr;
}
static FName Name(const UObject* Owner,const TCHAR* Key)
{
    const auto* P=Owner?FindFProperty<FNameProperty>(Owner->GetClass(),Key):nullptr;
    return P?P->GetPropertyValue_InContainer(Owner):NAME_None;
}
static bool Save(UObject* Asset)
{
    if(!Asset||!CanEdit())return false;
    UPackage* Package=Asset->GetOutermost();
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(Package,nullptr,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}
static bool Python(const FString& Expression)
{
    return CanEdit()&&IPythonScriptPlugin::Get()->ExecPythonCommand(*(TEXT("import godfall_equipment_manager as gfem; ")+Expression));
}
static FString AssetPath(UObject* Definition)
{
    if(!Definition)return FString();
    if(Definition->HasAnyFlags(RF_ClassDefaultObject))return Definition->GetClass()->ClassGeneratedBy?Definition->GetClass()->ClassGeneratedBy->GetPathName():FString();
    return Definition->GetPathName();
}
static FString Label(UObject* Definition)
{
    if(!Definition)return TEXT("Missing asset");
    if(const auto* P=FindFProperty<FTextProperty>(Definition->GetClass(),TEXT("DisplayName")))
    {const FString Text=P->GetPropertyValue_InContainer(Definition).ToString();if(!Text.IsEmpty())return Text;}
    return UGodFallEquipmentCatalog::DefinitionId(Definition).ToString();
}

class SEquipmentPreview : public SEditorViewport
{
public:
    SLATE_BEGIN_ARGS(SEquipmentPreview){} SLATE_END_ARGS()
    void Construct(const FArguments&)
    {
        Scene=MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());
        Scene->SetFloorVisibility(false);
        SEditorViewport::Construct(SEditorViewport::FArguments());
    }
    void Show(UObject* Definition)
    {
        if(Component){Scene->RemoveComponent(Component);Component=nullptr;}
        if(auto* Mesh=Cast<UStaticMesh>(Object(Definition,TEXT("PresentationStaticMesh"))))
        {
            auto* Static=NewObject<UStaticMeshComponent>();Static->SetStaticMesh(Mesh);Component=Static;
        }
        else if(auto* SkeletalMesh=Cast<USkeletalMesh>(Object(Definition,TEXT("PresentationMesh"))))
        {
            auto* Skeletal=NewObject<USkeletalMeshComponent>();Skeletal->SetSkeletalMeshAsset(SkeletalMesh);Component=Skeletal;
        }
        if(Component)
        {
            Scene->AddComponent(Component,FTransform::Identity);Component->UpdateBounds();
            bNeedsFrame=true;
        }
        Invalidate();
    }
    virtual void Tick(const FGeometry& Geometry,const double CurrentTime,const float DeltaTime) override
    {
        SEditorViewport::Tick(Geometry,CurrentTime,DeltaTime);
        // First selection can precede viewport layout. Frame against the actual
        // dimensions, and refit when docking/resizing changes the aspect ratio.
        if(Component&&Client->Viewport)
        {
            const FIntPoint Size=Client->Viewport->GetSizeXY();
            if(Size.X>0&&Size.Y>0&&(bNeedsFrame||Size!=FramedSize))
            {
                Component->UpdateBounds();
                Client->FocusViewportOnBox(Component->Bounds.GetBox(),true);
                FramedSize=Size;bNeedsFrame=false;Invalidate();
            }
        }
    }
protected:
    virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override
    {
        Client=MakeShared<FEditorViewportClient>(nullptr,Scene.Get(),SharedThis(this));
        Client->SetViewMode(VMI_Lit);Client->SetViewRotation(FRotator(-15,-135,0));Client->SetRealtime(false);
        Client->EngineShowFlags.SetGrid(false);return Client.ToSharedRef();
    }
private:
    TSharedPtr<FAdvancedPreviewScene> Scene;
    TSharedPtr<FEditorViewportClient> Client;
    UMeshComponent* Component=nullptr; // Preview scene owns component references.
    FIntPoint FramedSize=FIntPoint::ZeroValue;
    bool bNeedsFrame=false;
};

struct FRow { FString Key; FString Label; TWeakObjectPtr<UObject> Definition; };
class SEquipmentManager;
static TWeakPtr<SEquipmentManager> Panel;

class SEquipmentManager : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SEquipmentManager){} SLATE_END_ARGS()
    void Construct(const FArguments&)
    {
        FDetailsViewArgs Args;Args.bUpdatesFromSelection=false;Args.bLockable=false;Args.bAllowSearch=true;Args.NameAreaSettings=FDetailsViewArgs::HideNameArea;
        Details=FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(Args);
        Details->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateLambda([](const FPropertyAndParent& Property)
        {const FName N=Property.Property.GetFName();return N==TEXT("DefinitionId")||N==TEXT("WeaponClassId");}));
        Details->OnFinishedChangingProperties().AddSP(this,&SEquipmentManager::PropertyChanged);
        ChildSlot
        [SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(12,8)[SNew(STextBlock).Text(FText::FromString("EQUIPMENT MANAGER")).Font(FCoreStyle::GetDefaultFontStyle("Bold",18))]
            +SVerticalBox::Slot().AutoHeight().Padding(12,0,12,8)[SNew(STextBlock).Text(FText::FromString("Import or select an item, edit its definition, then Save changes. Availability is used the next time you press Play."))]
            +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().Padding(3)[Button("Import armor...",[this]{Python(TEXT("gfem.import_armor()"));Reload();UGodFallEquipmentEditorLibrary::OpenManager();})]
                +SHorizontalBox::Slot().AutoWidth().Padding(3)[Button("Import weapon...",[this]{Python(TEXT("gfem.import_weapon()"));Reload();UGodFallEquipmentEditorLibrary::OpenManager();})]
                +SHorizontalBox::Slot().AutoWidth().Padding(3)[Button("Add selected asset",[this]{Python(TEXT("gfem.add_selected()"));Reload();UGodFallEquipmentEditorLibrary::OpenManager();})]
                +SHorizontalBox::Slot().AutoWidth().Padding(3)[Button("Refresh list",[this]{FString Error;UGodFallEquipmentEditorLibrary::RefreshCatalog(Error);Status=Error;Reload();})]
                +SHorizontalBox::Slot().AutoWidth().Padding(3)[Button("Save changes",[this]{FString Error;const bool OK=UGodFallEquipmentEditorLibrary::SaveDefinition(Selected.Get(),Error);Status=OK?TEXT("Saved. Ready for Play."):Error;Reload();})]
            ]
            +SVerticalBox::Slot().FillHeight(1).Padding(12,0)[SNew(SSplitter)
                +SSplitter::Slot().Value(.30f)[SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,6,8)[SNew(SSearchBox).HintText(FText::FromString("Search armor and weapons")).OnTextChanged_Lambda([this](const FText& Text){Search=Text.ToString();Reload();})]
                    +SVerticalBox::Slot().FillHeight(1)[SAssignNew(List,SListView<TSharedPtr<FRow>>).ListItemsSource(&Rows).OnGenerateRow(this,&SEquipmentManager::Row).OnSelectionChanged(this,&SEquipmentManager::Selection)]
                ]
                +SSplitter::Slot().Value(.70f)[SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().Padding(10,0)[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(Selected.IsValid()?Label(Selected.Get()):TEXT("Select equipment to edit and preview"));}).Font(FCoreStyle::GetDefaultFontStyle("Bold",14))]
                    +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth().Padding(5)[Toggle("Available",0)]
                        +SHorizontalBox::Slot().AutoWidth().Padding(5)[Toggle("Loot",1)]
                        +SHorizontalBox::Slot().AutoWidth().Padding(5)[Toggle("Free store",2)]
                        +SHorizontalBox::Slot().AutoWidth().Padding(5)[Toggle("Random weapon pool",3)]
                    ]
                    +SVerticalBox::Slot().AutoHeight().Padding(10,0,10,6)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString("Loot also respects the definition's Allow Random Loot and character compatibility. Remove disables future acquisition; existing inventory and source assets are retained."))]
                    +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth().Padding(3)[Button("Remove / restore",[this]{if(const auto* E=Entry())Set(!E->bEnabled,E->bLoot,E->bFreeStore,E->bGenericWeaponLoot);})]
                        +SHorizontalBox::Slot().AutoWidth().Padding(3)[Button("Locate asset",[this]{Python(TEXT("gfem.locate('")+AssetPath(Selected.Get())+TEXT("')"));})]
                        +SHorizontalBox::Slot().AutoWidth().Padding(3)[Button("Fit selected armor draft",[this]{Python(TEXT("gfem.preview_fit()"));})]
                        +SHorizontalBox::Slot().AutoWidth().Padding(3)[Button("Approve fit",[this]{Python(TEXT("gfem.approve_fit()"));Reload();})]
                    ]
                    +SVerticalBox::Slot().FillHeight(.45f)[SAssignNew(Preview,SEquipmentPreview)]
                    +SVerticalBox::Slot().FillHeight(.55f)[SNew(SBox).IsEnabled_Lambda([]{return CanEdit();})[Details.ToSharedRef()]]
                ]
            ]
            +SVerticalBox::Slot().AutoHeight().Padding(12,8)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]{return FText::FromString(CanEdit()?Status:TEXT("Stop Play to edit equipment."));})]
        ];
        FString Error;UGodFallEquipmentEditorLibrary::RefreshCatalog(Error);Status=Error;Reload();
    }
    void Reload()
    {
        const FString Old=Selected.IsValid()?Selected->GetPathName():FString();Rows.Reset();
        if(const auto* Catalog=UGodFallEquipmentCatalog::Get())for(const auto& Entry:Catalog->Items)
        {
            UObject* Definition=Entry.Resolve();
            FString Text=(Entry.Weapon.IsNull()?TEXT("Armor  |  "):TEXT("Weapon  |  "))+Label(Definition);
            Text+=Entry.bEnabled?TEXT(""):TEXT("  [removed / draft]");
            if(!Search.IsEmpty()&&!Text.Contains(Search)&&!Entry.Key().Contains(Search))continue;
            auto Item=MakeShared<FRow>();Item->Key=Entry.Key();Item->Label=Text;Item->Definition=Definition;Rows.Add(Item);
        }
        Rows.Sort([](const auto& A,const auto& B){return A->Label<B->Label;});
        if(List){List->RequestListRefresh();for(const auto& Item:Rows)if(Item->Definition.IsValid()&&Item->Definition->GetPathName()==Old){List->SetSelection(Item);break;}}
    }
    bool Select(UObject* Definition)
    {
        Search.Empty();Reload();for(const auto& Item:Rows)if(Item->Definition.Get()==Definition){List->SetSelection(Item);List->RequestScrollIntoView(Item);return true;}return false;
    }
private:
    TSharedRef<SWidget> Button(const ANSICHAR* Text,TFunction<void()> Action)
    {return SNew(SButton).Text(FText::FromString(UTF8_TO_TCHAR(Text))).IsEnabled_Lambda([]{return CanEdit();}).OnClicked_Lambda([Action]{Action();return FReply::Handled();});}
    const FGodFallEquipmentEntry* Entry()const {auto* Catalog=UGodFallEquipmentCatalog::Get();return Catalog?Catalog->Find(Selected.Get()):nullptr;}
    void Set(bool Enabled,bool Loot,bool Store,bool Generic)
    {FString Error;bool OK=UGodFallEquipmentEditorLibrary::SetAvailability(Selected.Get(),Enabled,Loot,Store,Generic,Error);Status=OK?TEXT("Availability saved. Changes apply on the next Play session."):Error;Reload();}
    TSharedRef<SWidget> Toggle(const ANSICHAR* Text,int32 Field)
    {
        return SNew(SCheckBox).IsEnabled_Lambda([this,Field]{const auto* E=Entry();return CanEdit()&&E&&(Field!=3||!E->Weapon.IsNull());})
        .IsChecked_Lambda([this,Field]{const auto* E=Entry();return E&&(Field==0?E->bEnabled:Field==1?E->bLoot:Field==2?E->bFreeStore:E->bGenericWeaponLoot)?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
        .OnCheckStateChanged_Lambda([this,Field](ECheckBoxState State){if(const auto* E=Entry()){auto V=*E;bool On=State==ECheckBoxState::Checked;if(Field==0)V.bEnabled=On;else if(Field==1)V.bLoot=On;else if(Field==2)V.bFreeStore=On;else V.bGenericWeaponLoot=On;Set(V.bEnabled,V.bLoot,V.bFreeStore,V.bGenericWeaponLoot);}})
        [SNew(STextBlock).Text(FText::FromString(UTF8_TO_TCHAR(Text)))];
    }
    TSharedRef<ITableRow> Row(TSharedPtr<FRow> Item,const TSharedRef<STableViewBase>& Owner)
    {return SNew(STableRow<TSharedPtr<FRow>>,Owner).Padding(7)[SNew(STextBlock).Text(FText::FromString(Item->Label)).AutoWrapText(true)];}
    void Selection(TSharedPtr<FRow> Item,ESelectInfo::Type)
    {
        Selected=Item?Item->Definition:TWeakObjectPtr<UObject>();
        if(Selected.IsValid()&&CanEdit())Python(TEXT("gfem.prepare_edit('")+AssetPath(Selected.Get())+TEXT("')"));
        Details->SetObject(Selected.Get());Preview->Show(Selected.Get());
    }
    void PropertyChanged(const FPropertyChangedEvent&){if(Selected.IsValid())Selected->MarkPackageDirty();Preview->Show(Selected.Get());Status=TEXT("Unsaved definition changes. Press Save changes before Play.");}
    TArray<TSharedPtr<FRow>> Rows;
    TSharedPtr<SListView<TSharedPtr<FRow>>> List;
    TSharedPtr<IDetailsView> Details;
    TSharedPtr<SEquipmentPreview> Preview;
    TWeakObjectPtr<UObject> Selected;
    FString Search,Status;
};
}

void UGodFallEquipmentEditorLibrary::OpenManager()
{
    if(auto Tab=FGlobalTabmanager::Get()->TryInvokeTab(GFEquipment::TabId))
    {
        Tab->ActivateInParent(ETabActivationCause::SetDirectly);
        if(auto Window=FSlateApplication::Get().FindWidgetWindow(Tab.ToSharedRef()))Window->BringToFront(true);
    }
}
bool UGodFallEquipmentEditorLibrary::CaptureEditorWindow(const FString& Filename)
{
    if(!GFEquipment::CanEdit()||!FPaths::IsUnderDirectory(Filename,TEXT("C:/Users/June/Documents/ChatGPT/GodFall/artifacts"))||FPaths::GetExtension(Filename)!=TEXT("png"))return false;
    auto Panel=GFEquipment::Panel.Pin();
    auto Window=Panel?FSlateApplication::Get().FindWidgetWindow(Panel.ToSharedRef()):FSlateApplication::Get().GetActiveTopLevelWindow();if(!Window)return false;
    TArray<FColor> Pixels;FIntVector Size;
    if(!FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),Pixels,Size))return false;
    TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
    return FFileHelper::SaveArrayToFile(PNG,*Filename);
}
bool UGodFallEquipmentEditorLibrary::SelectDefinition(UObject* Definition)
{OpenManager();auto Panel=GFEquipment::Panel.Pin();return Panel&&Panel->Select(Definition);}

bool UGodFallEquipmentEditorLibrary::ValidateCatalog(FString& Error)
{
    Error.Reset();auto* Catalog=UGodFallEquipmentCatalog::Get();if(!Catalog){Error=TEXT("Equipment settings are missing.");return false;}
    TSet<FName> Ids,Classes;TSet<FString> Keys;
    for(const auto& Entry:Catalog->Items)
    {
        UObject* Definition=Entry.Resolve();const FName Id=UGodFallEquipmentCatalog::DefinitionId(Definition);
        if(!Definition){if(!Entry.bEnabled)continue;Error=TEXT("An available item has a missing definition.");return false;}
        if(Id.IsNone()||Ids.Contains(Id)||Keys.Contains(Entry.Key())){Error=TEXT("Definitions require unique, nonempty IDs and paths.");return false;}
        Ids.Add(Id);Keys.Add(Entry.Key());
        if(!Entry.Weapon.IsNull())
        {
            const FName Key=GFEquipment::Name(Definition,TEXT("WeaponClassId"));
            if(Key.IsNone()||Classes.Contains(Key)||!GFEquipment::Object(Definition,TEXT("CombatStyleProfile"))||!GFEquipment::Object(Definition,TEXT("PresentationStaticMesh")))
            {Error=TEXT("Each weapon needs a unique weapon class, combat style and static mesh. Keep incomplete items outside the managed catalog.");return false;}
            Classes.Add(Key);
        }
    }
    return true;
}

bool UGodFallEquipmentEditorLibrary::RefreshCatalog(FString& Error)
{
    Error.Reset();if(!GFEquipment::CanEdit()){Error=TEXT("Stop Play before refreshing equipment.");return false;}
    auto* Catalog=UGodFallEquipmentCatalog::Get();const bool Initial=!Catalog;
    UObject* ArmorCatalog=LoadObject<UObject>(nullptr,TEXT("/Game/GF/Items/Armor/Data/DA_GF_ArmorCatalog.DA_GF_ArmorCatalog"));
    auto* Array=ArmorCatalog?FindFProperty<FArrayProperty>(ArmorCatalog->GetClass(),TEXT("Definitions")):nullptr;
    auto* Inner=Array?CastField<FObjectPropertyBase>(Array->Inner):nullptr;
    UClass* Base=LoadClass<UObject>(nullptr,TEXT("/Game/GF/Weapons/Data/BP_GF_WeaponDefinition.BP_GF_WeaponDefinition_C"));
    if(!Inner||!Base){Error=TEXT("The armor catalog or weapon schema is unavailable.");return false;}
    if(!Catalog)
    {
        UPackage* Package=CreatePackage(GFEquipment::PolicyPackage);
        Catalog=NewObject<UGodFallEquipmentCatalog>(Package,TEXT("DA_GF_EquipmentManager"),RF_Public|RF_Standalone|RF_Transactional);
        FAssetRegistryModule::AssetCreated(Catalog);
    }
    const auto Before=Catalog->Items;
    auto Add=[&](UObject* Definition,bool Weapon)
    {
        if(!Definition||Catalog->Find(Definition))return;
        FGodFallEquipmentEntry Entry;
        if(Weapon)Entry.Weapon=Definition->GetClass();else Entry.Armor=Definition;
        Entry.bEnabled=Initial;Entry.bFreeStore=Initial;Entry.bLoot=Initial;
        if(!Weapon)if(const auto* P=FindFProperty<FBoolProperty>(Definition->GetClass(),TEXT("AllowRandomLoot")))Entry.bLoot=Initial&&P->GetPropertyValue_InContainer(Definition);
        // Preserve the existing generic Sword / specialized Dagger split at migration.
        Entry.bGenericWeaponLoot=!Initial||GFEquipment::Name(Definition,TEXT("DefinitionId"))==TEXT("GF_TestSword");
        Catalog->Items.Add(Entry);
    };
    FScriptArrayHelper Values(Array,Array->ContainerPtrToValuePtr<void>(ArmorCatalog));
    for(int32 I=0;I<Values.Num();++I)Add(Inner->GetObjectPropertyValue(Values.GetRawPtr(I)),false);
    FARFilter Filter;Filter.PackagePaths.Add(TEXT("/Game/GF/Weapons/Definitions"));Filter.bRecursivePaths=true;Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    TArray<FAssetData> Assets;FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssets(Filter,Assets);
    for(const auto& Asset:Assets)if(auto* BP=Cast<UBlueprint>(Asset.GetAsset()))if(BP->GeneratedClass&&BP->GeneratedClass->IsChildOf(Base))Add(BP->GeneratedClass->GetDefaultObject(),true);
    if(!ValidateCatalog(Error)){Catalog->Items=Before;return false;}
    if(Catalog->Items.Num()!=Before.Num()||Initial)
    {Catalog->MarkPackageDirty();if(!GFEquipment::Save(Catalog)){Catalog->Items=Before;Error=TEXT("Could not save equipment settings.");return false;}}
    return true;
}

bool UGodFallEquipmentEditorLibrary::SetAvailability(UObject* Definition,bool Enabled,bool Loot,bool FreeStore,bool GenericWeaponLoot,FString& Error)
{
    Error.Reset();if(!GFEquipment::CanEdit()){Error=TEXT("Stop Play before editing availability.");return false;}
    auto* Catalog=UGodFallEquipmentCatalog::Get();auto* Entry=Catalog?const_cast<FGodFallEquipmentEntry*>(Catalog->Find(Definition)):nullptr;
    if(!Entry){Error=TEXT("Select a registered equipment definition.");return false;}
    const auto Before=*Entry;Catalog->Modify();Entry->bEnabled=Enabled;Entry->bLoot=Loot;Entry->bFreeStore=FreeStore;Entry->bGenericWeaponLoot=GenericWeaponLoot;
    if(!ValidateCatalog(Error)||!GFEquipment::Save(Catalog)){*Entry=Before;if(Error.IsEmpty())Error=TEXT("Could not save availability.");return false;}
    return true;
}
bool UGodFallEquipmentEditorLibrary::SaveDefinition(UObject* Definition,FString& Error)
{
    Error.Reset();if(!GFEquipment::CanEdit()||!Definition){Error=TEXT("Stop Play and select an item first.");return false;}
    if(!ValidateCatalog(Error))return false;
    const FString Path=GFEquipment::AssetPath(Definition);
    if(!GFEquipment::Python(TEXT("gfem.save_definition('")+Path+TEXT("')"))){Error=TEXT("Save failed. Review the Output Log for the exact validation error.");return false;}
    // Compilation may replace a weapon CDO. Never leave the details view on it.
    if(UObject* Asset=LoadObject<UObject>(nullptr,*Path))
    {
        UBlueprint* BP=Cast<UBlueprint>(Asset);
        SelectDefinition(BP&&BP->GeneratedClass?BP->GeneratedClass->GetDefaultObject():Asset);
    }
    return true;
}

class FGodFallEquipmentEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GFEquipment::TabId,FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
        {auto Widget=SNew(GFEquipment::SEquipmentManager);GFEquipment::Panel=Widget;return SNew(SDockTab).TabRole(ETabRole::NomadTab)[Widget];}))
        .SetDisplayName(FText::FromString("Equipment Manager")).SetMenuType(ETabSpawnerMenuType::Hidden);
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this,&FGodFallEquipmentEditorModule::Menus));
    }
    void Menus()
    {
        FToolMenuOwnerScoped Owner(this);
        auto* Menu=UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
        auto& Section=Menu->FindOrAddSection("GodFallWearables");
        Section.AddMenuEntry("GodFallEquipmentManager",FText::FromString("GodFall Equipment Manager"),FText::FromString("Manage armor, weapons, loot and free-store availability."),FSlateIcon(),FUIAction(FExecuteAction::CreateStatic(&UGodFallEquipmentEditorLibrary::OpenManager)));
    }
    virtual void ShutdownModule() override
    {
        UToolMenus::UnRegisterStartupCallback(this);UToolMenus::UnregisterOwner(this);
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GFEquipment::TabId);
    }
};
IMPLEMENT_MODULE(FGodFallEquipmentEditorModule,GodFallEquipmentEditor)
