#include "GodFallFreeStore.h"
#include "GodFallPackLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/ActorComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
FProperty* Field(UStruct* Type,const TCHAR* Name)
{
    if(!Type)return nullptr;
    if(FProperty* Exact=FindFProperty<FProperty>(Type,Name))return Exact;
    const FString Prefix=FString(Name)+TEXT("_");
    for(TFieldIterator<FProperty> It(Type);It;++It)
        if(It->GetName().StartsWith(Prefix,ESearchCase::IgnoreCase))return *It;
    return nullptr;
}
UObject* Object(const UObject* Owner,const TCHAR* Name)
{
    auto* P=Owner?CastField<FObjectPropertyBase>(Field(Owner->GetClass(),Name)):nullptr;
    return P?P->GetObjectPropertyValue_InContainer(Owner):nullptr;
}
FName Name(const UObject* Owner,const TCHAR* Key)
{
    auto* P=Owner?CastField<FNameProperty>(Field(Owner->GetClass(),Key)):nullptr;
    return P?P->GetPropertyValue_InContainer(Owner):NAME_None;
}
FText Label(const UObject* Definition)
{
    FProperty* P=Definition?Field(Definition->GetClass(),TEXT("DisplayName")):nullptr;
    if(auto* Text=CastField<FTextProperty>(P))return Text->GetPropertyValue_InContainer(Definition);
    if(auto* N=CastField<FNameProperty>(P))return FText::FromName(N->GetPropertyValue_InContainer(Definition));
    if(auto* S=CastField<FStrProperty>(P))return FText::FromString(S->GetPropertyValue_InContainer(Definition));
    return FText::FromName(Name(Definition,TEXT("DefinitionId")));
}
bool InSlice(const UObject* Context)
{
    UWorld* World=Context?Context->GetWorld():nullptr;
    return World&&World->IsGameWorld()&&World->GetMapName().EndsWith(TEXT("L_GF_VerticalSlice"));
}
bool Copy(FProperty* Dest,void* To,FProperty* Source,const void* From)
{
    if(!Dest||!Source||!Dest->SameType(Source))return false;
    Dest->CopyCompleteValue(Dest->ContainerPtrToValuePtr<void>(To),Source->ContainerPtrToValuePtr<void>(From));return true;
}
bool SetObject(UFunction* Function,void* Memory,const TCHAR* Key,UObject* Value)
{
    auto* P=CastField<FObjectPropertyBase>(Field(Function,Key));if(!P)return false;
    P->SetObjectPropertyValue_InContainer(Memory,Value);return true;
}
bool SetNumber(UFunction* Function,void* Memory,const TCHAR* Key,int64 Value)
{
    auto* P=CastField<FNumericProperty>(Field(Function,Key));if(!P||!P->IsInteger())return false;
    P->SetIntPropertyValue(P->ContainerPtrToValuePtr<void>(Memory),Value);return true;
}
bool Bool(UFunction* Function,void* Memory,const TCHAR* Key)
{
    auto* P=CastField<FBoolProperty>(Field(Function,Key));return P&&P->GetPropertyValue_InContainer(Memory);
}
UObject* OutputObject(UFunction* Function,void* Memory,const TCHAR* Key)
{
    auto* P=CastField<FObjectPropertyBase>(Field(Function,Key));return P?P->GetObjectPropertyValue_InContainer(Memory):nullptr;
}
UObject* Library(const TCHAR* ClassPath)
{
    UClass* Class=LoadClass<UObject>(nullptr,ClassPath);return Class?Class->GetDefaultObject():nullptr;
}
void RemoveUncommittedWeapon(UObject* Registry,UObject* Instance,FName Id)
{
    auto* Map=Registry?CastField<FMapProperty>(Field(Registry->GetClass(),TEXT("WeaponInstancesById"))):nullptr;
    auto* Value=Map?CastField<FObjectPropertyBase>(Map->ValueProp):nullptr;
    if(!Map||!CastField<FNameProperty>(Map->KeyProp)||!Value)return;
    FScriptMapHelper Helper(Map,Map->ContainerPtrToValuePtr<void>(Registry));const int32 Index=Helper.FindMapIndexWithKey(&Id);
    if(Index!=INDEX_NONE&&Value->GetObjectPropertyValue(Helper.GetValuePtr(Index))==Instance)Helper.RemoveAt(Index);
}
}

TArray<UObject*> UGodFallPackLibrary::GetFreeStoreDefinitions()
{
    TArray<UObject*> Result;TSet<FName> Ids;
    auto Add=[&](UObject* Definition){const FName Id=Name(Definition,TEXT("DefinitionId"));if(!Id.IsNone()&&!Ids.Contains(Id)){Ids.Add(Id);Result.Add(Definition);}};
    for(const TCHAR* Path:{TEXT("/Game/GF/Weapons/Definitions/BP_GF_WeaponDefinition_TestSword.BP_GF_WeaponDefinition_TestSword_C"),TEXT("/Game/GF/Weapons/Definitions/BP_GF_WeaponDefinition_TestDagger.BP_GF_WeaponDefinition_TestDagger_C")})
        if(UClass* Class=LoadClass<UObject>(nullptr,Path))Add(Class->GetDefaultObject());
    for(UClass* Class:GetExternalWeaponDefinitions())if(Class)Add(Class->GetDefaultObject());
    UObject* Authored=LoadObject<UObject>(nullptr,TEXT("/Game/GF/Items/Armor/Data/DA_GF_ArmorCatalog.DA_GF_ArmorCatalog"));
    UObject* Catalog=Authored?GetRuntimeClothingCatalog(Authored,Authored->GetClass()):nullptr;
    auto* Array=Catalog?CastField<FArrayProperty>(Field(Catalog->GetClass(),TEXT("Definitions"))):nullptr;
    auto* Value=Array?CastField<FObjectPropertyBase>(Array->Inner):nullptr;
    if(Array&&Value){FScriptArrayHelper Helper(Array,Array->ContainerPtrToValuePtr<void>(Catalog));for(int32 I=0;I<Helper.Num();++I)Add(Value->GetObjectPropertyValue(Helper.GetRawPtr(I)));}
    return Result;
}

bool UGodFallPackLibrary::ClaimFreeStoreItem(UObject* Context,UObject* Definition,FName& InstanceId,FString& Error)
{
    InstanceId=NAME_None;Error.Reset();
    if(!InSlice(Context)||!GetFreeStoreDefinitions().Contains(Definition)){Error=TEXT("This item is not available in this store.");return false;}
    APawn* Player=UGameplayStatics::GetPlayerPawn(Context,0);UGameInstance* GI=UGameplayStatics::GetGameInstance(Context);
    UObject* Inventory=Player?Object(Player,TEXT("PlayerInventory")):nullptr;
    if(!Inventory&&Player){UClass* Class=LoadClass<UActorComponent>(nullptr,TEXT("/Game/SYN/Content/InventorySystem/C_Inventory.C_Inventory_C"));if(Class)Inventory=Player->GetComponentByClass(Class);}
    UObject* Registry=GI?Object(GI,TEXT("GF_ItemRegistry")):nullptr;
    auto* Items=Inventory?CastField<FArrayProperty>(Field(Inventory->GetClass(),TEXT("Inventory"))):nullptr;
    auto* Gold=GI?CastField<FNumericProperty>(Field(GI->GetClass(),TEXT("GoldBalance"))):nullptr;
    UFunction* Buy=Player?Player->FindFunction(TEXT("TryBuyStockItem")):nullptr;
    UObject* Rules=Library(TEXT("/Game/GF/Economy/Inventory/BPFL_GF_EconomyInventory.BPFL_GF_EconomyInventory_C"));
    UFunction* Snapshot=Rules?Rules->FindFunction(TEXT("BuildPurchaseSnapshot")):nullptr;
    UDataTable* Authored=LoadObject<UDataTable>(nullptr,TEXT("/Game/SYN/Content/InventorySystem/DataTables/DB_Items_Composition.DB_Items_Composition"));
    UDataTable* Table=GetRuntimeItemTable(Authored);
    const bool IsWeapon=!Name(Definition,TEXT("WeaponClassId")).IsNone();
    FName Row=IsWeapon?Name(Definition,TEXT("DefinitionId")):Name(Definition,TEXT("TemplateRowId"));
    if(IsWeapon&&Table&&!Table->FindRowUnchecked(Row))Row=TEXT("GF_WeaponForge_TestSword");
    const uint8* Template=Table?Table->FindRowUnchecked(Row):nullptr;
    UObject* Factory=IsWeapon?Registry:Library(TEXT("/Game/GF/Items/Armor/Factories/BPFL_GF_ArmorFactory.BPFL_GF_ArmorFactory_C"));
    UFunction* Create=Factory?Factory->FindFunction(IsWeapon?TEXT("CreateWeaponInstance"):TEXT("CreateArmorInstance")):nullptr;
    if(!Items||!Gold||!Buy||!Snapshot||!Template||!Create){Error=TEXT("The item service is unavailable.");return false;}
    auto* BuyItem=CastField<FStructProperty>(Field(Buy,TEXT("StockItem")));
    auto* FactoryItem=CastField<FStructProperty>(Field(Create,TEXT("TemplateItem")));
    auto* SnapshotItem=CastField<FStructProperty>(Field(Snapshot,TEXT("StockItem")));
    if(!BuyItem||!FactoryItem||!SnapshotItem||BuyItem->Struct!=Table->GetRowStruct()||FactoryItem->Struct!=BuyItem->Struct||SnapshotItem->Struct!=BuyItem->Struct)
    {Error=TEXT("The item schema is incompatible.");return false;}
    // Pure existing snapshot preflight rejects a full bag before any item is rolled.
    FStructOnScope Preflight(Snapshot);void* Pre=Preflight.GetStructMemory();
    if(!Copy(Field(Snapshot,TEXT("Inventory")),Pre,Items,Inventory)){Error=TEXT("Inventory schema is incompatible.");return false;}
    void* Probe=SnapshotItem->ContainerPtrToValuePtr<void>(Pre);SnapshotItem->Struct->CopyScriptStruct(Probe,Template);
    auto* Id=CastField<FNameProperty>(Field(SnapshotItem->Struct,TEXT("ID")));
    if(!Id){Error=TEXT("Item identity is unavailable.");return false;}
    Id->SetPropertyValue_InContainer(Probe,FName(*(TEXT("FreeStoreProbe@")+FGuid::NewGuid().ToString(EGuidFormats::Digits))));
    if(!SetNumber(Snapshot,Pre,TEXT("CurrentGold"),Gold->GetSignedIntPropertyValue(Gold->ContainerPtrToValuePtr<void>(GI)))||!SetNumber(Snapshot,Pre,TEXT("ListingPrice"),0))
    {Error=TEXT("Purchase schema is incompatible.");return false;}
    SetObject(Snapshot,Pre,TEXT("__WorldContext"),Player);Rules->ProcessEvent(Snapshot,Pre);
    if(!Bool(Snapshot,Pre,TEXT("Valid"))){Error=TEXT("Your inventory is full, or this claim is unavailable.");return false;}
    FStructOnScope Creation(Create);void* Args=Creation.GetStructMemory();
    if(!SetObject(Create,Args,TEXT("Definition"),Definition)){Error=TEXT("Definition schema is incompatible.");return false;}
    FactoryItem->Struct->CopyScriptStruct(FactoryItem->ContainerPtrToValuePtr<void>(Args),Template);
    SetObject(Create,Args,TEXT("WorldContextObject"),Player);SetObject(Create,Args,TEXT("__WorldContext"),Player);
    Factory->ProcessEvent(Create,Args);
    UObject* Instance=OutputObject(Create,Args,IsWeapon?TEXT("WeaponInstance"):TEXT("ArmorInstance"));
    auto* Payload=Instance?CastField<FStructProperty>(Field(Instance->GetClass(),TEXT("SYNItem"))):nullptr;
    if(!Payload||!Payload->SameType(BuyItem)){Error=TEXT("The permanent item could not be created.");return false;}
    const FName NewId=Id->GetPropertyValue_InContainer(Payload->ContainerPtrToValuePtr<void>(Instance));
    FStructOnScope Purchase(Buy);void* PurchaseArgs=Purchase.GetStructMemory();
    Copy(BuyItem,PurchaseArgs,Payload,Instance);SetNumber(Buy,PurchaseArgs,TEXT("ListingPrice"),0);Player->ProcessEvent(Buy,PurchaseArgs);
    if(!Bool(Buy,PurchaseArgs,TEXT("Purchased"))){if(IsWeapon)RemoveUncommittedWeapon(Registry,Instance,NewId);Error=TEXT("Claim could not be saved. Your inventory and currency were preserved.");return false;}
    InstanceId=NewId;return true;
}

bool UGodFallPackLibrary::InstallFreeStoreEntry(UUserWidget* Menu)
{
    if(!InSlice(Menu)||!Menu->WidgetTree)return false;
    if(Menu->WidgetTree->FindWidget(TEXT("GF_FreeStoreEntry")))return true;
    auto* Canvas=Cast<UCanvasPanel>(Menu->WidgetTree->RootWidget);if(!Canvas)return false;
    auto* Button=Menu->WidgetTree->ConstructWidget<UGodFallFreeStoreEntry>(UGodFallFreeStoreEntry::StaticClass(),TEXT("GF_FreeStoreEntry"));
    Button->Configure(Menu);auto* Text=Menu->WidgetTree->ConstructWidget<UTextBlock>();Text->SetText(NSLOCTEXT("GodFallStore","Entry","FREE STORE"));
    Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f,0.85f,0.62f)));FSlateFontInfo Font=Text->GetFont();Font.Size=17;Text->SetFont(Font);Button->SetContent(Text);
    auto* Slot=Canvas->AddChildToCanvas(Button);Slot->SetAnchors(FAnchors(0,1));Slot->SetAlignment(FVector2D(0,1));Slot->SetPosition(FVector2D(180,-30));Slot->SetSize(FVector2D(220,40));Slot->SetZOrder(20);
    return true;
}
UUserWidget* UGodFallPackLibrary::OpenFreeStore(UUserWidget* Menu,UButton* SourceButton)
{
    if(!InSlice(Menu)||!SourceButton)return nullptr;
    auto* PC=Menu->GetOwningPlayer();if(!PC)return nullptr;
    auto* Store=CreateWidget<UGodFallFreeStore>(PC);if(!Store)return nullptr;
    Store->Configure(Menu,SourceButton);Store->AddToViewport(500);Menu->SetVisibility(ESlateVisibility::Hidden);
    FInputModeUIOnly Mode;Mode.SetWidgetToFocus(Store->TakeWidget());PC->SetInputMode(Mode);PC->bShowMouseCursor=true;return Store;
}
void UGodFallFreeStoreEntry::Configure(UUserWidget* InMenu){Menu=InMenu;OnClicked.AddUniqueDynamic(this,&ThisClass::OpenStore);}
void UGodFallFreeStoreEntry::OpenStore(){UGodFallPackLibrary::OpenFreeStore(Menu,this);}
void UGodFallFreeStoreChoice::Configure(UObject* InDefinition,UGodFallFreeStore* InStore){Definition=InDefinition;Store=InStore;OnClicked.AddUniqueDynamic(this,&ThisClass::Claim);}
void UGodFallFreeStoreChoice::Claim(){if(Store&&Definition)Store->ClaimDefinition(Definition);}
void UGodFallFreeStore::Configure(UUserWidget* InMenu,UButton* InSource){Menu=InMenu;Source=InSource;PreviousVisibility=Menu->GetVisibility();SetIsFocusable(true);}
void UGodFallFreeStore::ConfigureWorldInteraction(){bWorldInteraction=true;SetIsFocusable(true);}
TSharedRef<SWidget> UGodFallFreeStore::RebuildWidget()
{
    if(!WidgetTree)WidgetTree=NewObject<UWidgetTree>(this,TEXT("FreeStoreTree"));
    if(!WidgetTree->RootWidget)
    {
        auto* Root=WidgetTree->ConstructWidget<UBorder>();Root->SetBrushColor(FLinearColor(0.01f,0.015f,0.025f,0.97f));Root->SetPadding(FMargin(24));Root->SetHorizontalAlignment(HAlign_Center);Root->SetVerticalAlignment(VAlign_Center);WidgetTree->RootWidget=Root;
        auto* Frame=WidgetTree->ConstructWidget<USizeBox>();Frame->SetWidthOverride(860);Frame->SetHeightOverride(780);Root->SetContent(Frame);
        auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();Frame->SetContent(Column);
        auto Text=[&](const FText& Content,int32 Size){auto* T=WidgetTree->ConstructWidget<UTextBlock>();T->SetText(Content);T->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f,0.88f,0.73f)));FSlateFontInfo Font=T->GetFont();Font.Size=Size;T->SetFont(Font);T->SetAutoWrapText(true);return T;};
        Column->AddChildToVerticalBox(Text(NSLOCTEXT("GodFallStore","Title","FREE STORE"),28))->SetPadding(FMargin(0,0,0,8));
        Column->AddChildToVerticalBox(Text(NSLOCTEXT("GodFallStore","Subtitle","Clothing and weapons for your inventory. Every claim is free."),17))->SetPadding(FMargin(0,0,0,12));
        auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Column->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        int32 Index=0;
        for(UObject* Definition:UGodFallPackLibrary::GetFreeStoreDefinitions())
        {
            auto* Button=WidgetTree->ConstructWidget<UGodFallFreeStoreChoice>(UGodFallFreeStoreChoice::StaticClass(),FName(*FString::Printf(TEXT("FreeClaim_%d"),Index++)));Button->Configure(Definition,this);if(!FirstChoice)FirstChoice=Button;
            auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Button->SetContent(Row);
            Row->AddChildToHorizontalBox(Text(Label(Definition),19))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            Row->AddChildToHorizontalBox(Text(NSLOCTEXT("GodFallStore","Claim","FREE  |  CLAIM"),16))->SetPadding(FMargin(18,10));
            Scroll->AddChild(Button);
        }
        Status=Text(NSLOCTEXT("GodFallStore","Ready","Choose an item. Characters are free in character selection."),16);Column->AddChildToVerticalBox(Status)->SetPadding(FMargin(0,14,0,10));
        Close=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("FreeStoreClose"));Close->SetContent(Text(NSLOCTEXT("GodFallStore","Close","BACK"),18));Close->OnClicked.AddUniqueDynamic(this,&ThisClass::CloseStore);Column->AddChildToVerticalBox(Close)->SetPadding(FMargin(0,5));
    }
    return Super::RebuildWidget();
}
void UGodFallFreeStore::NativeConstruct(){Super::NativeConstruct();if(FirstChoice)FirstChoice->SetKeyboardFocus();}
void UGodFallFreeStore::ClaimDefinition(UObject* Definition)
{
    FName Id;FString Error;const bool Claimed=UGodFallPackLibrary::ClaimFreeStoreItem(this,Definition,Id,Error);
    if(Status)Status->SetText(Claimed?FText::Format(NSLOCTEXT("GodFallStore","Added","{0} added to inventory. Cost: 0."),Label(Definition)):FText::FromString(Error));
}
void UGodFallFreeStore::CloseStore()
{
    if(bWorldInteraction){RestoreWorldInput();RemoveFromParent();return;}
    RemoveFromParent();if(Menu)Menu->SetVisibility(PreviousVisibility);
    if(auto* PC=GetOwningPlayer()){FInputModeGameAndUI Mode;if(Source)Mode.SetWidgetToFocus(Source->TakeWidget());PC->SetInputMode(Mode);PC->bShowMouseCursor=true;}
    if(Source)Source->SetKeyboardFocus();
}
void UGodFallFreeStore::RestoreWorldInput()
{
    if(!bWorldInteraction)return;
    bWorldInteraction=false;
    if(auto* PC=GetOwningPlayer())
    {
        PC->SetPause(false);PC->FlushPressedKeys();
        PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=false;
        UWidgetBlueprintLibrary::SetFocusToGameViewport();
    }
}
void UGodFallFreeStore::NativeDestruct(){RestoreWorldInput();Super::NativeDestruct();}
FReply UGodFallFreeStore::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(Event.GetKey()==EKeys::Escape||Event.GetKey()==EKeys::Gamepad_FaceButton_Right){CloseStore();return FReply::Handled();}
    return Super::NativeOnKeyDown(Geometry,Event);
}
