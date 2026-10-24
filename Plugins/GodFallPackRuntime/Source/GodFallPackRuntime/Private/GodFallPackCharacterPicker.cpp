#include "GodFallPackCharacterPicker.h"
#include "GodFallPackLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

namespace
{
FName ProfileId(const UObject* Profile)
{
    const FNameProperty* Property = Profile ? FindFProperty<FNameProperty>(Profile->GetClass(), TEXT("GodId")) : nullptr;
    return Property ? Property->GetPropertyValue_InContainer(Profile) : NAME_None;
}

FText ProfileLabel(const UObject* Profile)
{
    if (const FTextProperty* Text = FindFProperty<FTextProperty>(Profile->GetClass(), TEXT("DisplayName")))
    { return Text->GetPropertyValue_InContainer(Profile); }
    if (const FNameProperty* Name = FindFProperty<FNameProperty>(Profile->GetClass(), TEXT("DisplayName")))
    { return FText::FromName(Name->GetPropertyValue_InContainer(Profile)); }
    if (const FStrProperty* String = FindFProperty<FStrProperty>(Profile->GetClass(), TEXT("DisplayName")))
    { return FText::FromString(String->GetPropertyValue_InContainer(Profile)); }
    return FText::FromName(ProfileId(Profile));
}
}

void UGodFallPackEntryButton::Configure(UUserWidget* InSelection)
{
    Selection = InSelection;
    OnClicked.AddUniqueDynamic(this, &ThisClass::OpenPicker);
}
void UGodFallPackEntryButton::OpenPicker()
{ UGodFallPackLibrary::OpenExternalCharacterPicker(Selection, this); }

void UGodFallPackCharacterChoice::ConfigureChoice(UObject* InProfile, UGodFallPackCharacterPicker* InPicker)
{
    Profile = InProfile; Picker = InPicker;
    OnClicked.AddUniqueDynamic(this, &ThisClass::HandleChoice);
}
void UGodFallPackCharacterChoice::HandleChoice() { if (Picker && Profile) { Picker->SelectProfile(Profile); } }

void UGodFallPackCharacterPicker::Configure(UUserWidget* InSelection, UButton* InSourceButton)
{
    Selection = InSelection; SourceButton = InSourceButton;
    PreviousVisibility = Selection->GetVisibility();
    SetIsFocusable(true);
}

TSharedRef<SWidget> UGodFallPackCharacterPicker::RebuildWidget()
{
    if (!WidgetTree) { WidgetTree = NewObject<UWidgetTree>(this, TEXT("PackPickerTree")); }
    if (!WidgetTree->RootWidget)
    {
        UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PackPickerRoot"));
        Root->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.035f, 0.98f));
        Root->SetHorizontalAlignment(HAlign_Center); Root->SetVerticalAlignment(VAlign_Center);
        Root->SetPadding(FMargin(24)); WidgetTree->RootWidget = Root;
        USizeBox* Frame = WidgetTree->ConstructWidget<USizeBox>();
        Frame->SetWidthOverride(620); Frame->SetHeightOverride(650); Root->SetContent(Frame);
        UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(); Frame->SetContent(Column);
        auto Text = [&](FName Name, const FText& Content, int32 Size) {
            UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
            Label->SetText(Content); Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
            FSlateFontInfo Font = Label->GetFont(); Font.Size = Size; Label->SetFont(Font);
            Label->SetAutoWrapText(true); return Label;
        };
        Column->AddChildToVerticalBox(Text(TEXT("PackPickerHeading"), NSLOCTEXT("GodFallPacks", "Heading", "Additional Characters"), 30))->SetPadding(FMargin(0,0,0,24));
        UScrollBox* List = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("PackCharacterList"));
        Column->AddChildToVerticalBox(List)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        int32 Index = 0;
        for (UObject* Profile : UGodFallPackLibrary::GetExternalCharacterProfiles())
        {
            UGodFallPackCharacterChoice* Choice = WidgetTree->ConstructWidget<UGodFallPackCharacterChoice>(
                UGodFallPackCharacterChoice::StaticClass(), FName(*FString::Printf(TEXT("PackCharacter_%d"), Index++)));
            if (SourceButton) { Choice->SetStyle(SourceButton->GetStyle()); }
            Choice->ConfigureChoice(Profile, this);
            UTextBlock* Label = Text(NAME_None, ProfileLabel(Profile), 23);
            Choice->AddChild(Label); List->AddChild(Choice);
        }
        Status = Text(TEXT("PackSelectionStatus"), NSLOCTEXT("GodFallPacks", "Pick", "Choose a character to begin a new playthrough."), 19);
        Column->AddChildToVerticalBox(Status)->SetPadding(FMargin(0,20,0,20));
        auto Action = [&](FName Name, const FText& Label) {
            UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
            if (SourceButton) { Button->SetStyle(SourceButton->GetStyle()); }
            Button->AddChild(Text(NAME_None, Label, 21));
            Column->AddChildToVerticalBox(Button)->SetPadding(FMargin(0,6)); return Button;
        };
        Confirm = Action(TEXT("PackBeginPlaythrough"), NSLOCTEXT("GodFallPacks", "Begin", "BEGIN PLAYTHROUGH"));
        Confirm->SetIsEnabled(false); Confirm->OnClicked.AddUniqueDynamic(this, &ThisClass::ConfirmSelection);
        Back = Action(TEXT("PackBack"), NSLOCTEXT("GodFallPacks", "Back", "BACK"));
        Back->OnClicked.AddUniqueDynamic(this, &ThisClass::ClosePicker);
    }
    return Super::RebuildWidget();
}

void UGodFallPackCharacterPicker::NativeConstruct()
{
    Super::NativeConstruct();
    if (Back) { Back->SetKeyboardFocus(); }
}

void UGodFallPackCharacterPicker::SelectProfile(UObject* Profile)
{
    if (!Profile || UGodFallPackLibrary::FindExternalCharacterProfile(ProfileId(Profile)) != Profile) { return; }
    SelectedProfile = Profile;
    if (Status) { Status->SetText(FText::Format(NSLOCTEXT("GodFallPacks", "Selected", "Selected: {0}"), ProfileLabel(Profile))); }
    if (Confirm) { Confirm->SetIsEnabled(true); Confirm->SetKeyboardFocus(); }
}

void UGodFallPackCharacterPicker::ConfirmSelection()
{
    UGameInstance* Instance = GetGameInstance();
    if (!Instance || !SelectedProfile || UGodFallPackLibrary::FindExternalCharacterProfile(ProfileId(SelectedProfile)) != SelectedProfile) { return; }
    FBoolProperty* Locked = FindFProperty<FBoolProperty>(Instance->GetClass(), TEXT("IsGodLocked"));
    FNameProperty* Pending = FindFProperty<FNameProperty>(Instance->GetClass(), TEXT("PendingGodId"));
    FNameProperty* Selected = FindFProperty<FNameProperty>(Instance->GetClass(), TEXT("SelectedGodId"));
    UFunction* Commit = Instance->FindFunction(TEXT("CommitGodSelection"));
    if (!Locked || !Pending || !Selected || !Commit || Commit->ParmsSize != 0) { return; }
    if (Locked->GetPropertyValue_InContainer(Instance))
    { Status->SetText(NSLOCTEXT("GodFallPacks", "Locked", "Start a new game to choose a different character.")); return; }
    Pending->SetPropertyValue_InContainer(Instance, ProfileId(SelectedProfile));
    Instance->ProcessEvent(Commit, nullptr);
    if (Locked->GetPropertyValue_InContainer(Instance) && Selected->GetPropertyValue_InContainer(Instance) == ProfileId(SelectedProfile))
    { UGameplayStatics::OpenLevel(this, TEXT("/Game/GF/Worlds/Maps/L_GF_VerticalSlice")); }
}

void UGodFallPackCharacterPicker::ClosePicker()
{
    if (Selection) { Selection->SetVisibility(PreviousVisibility); }
    RemoveFromParent();
    if (SourceButton) { SourceButton->SetKeyboardFocus(); }
    if (APlayerController* Controller = GetOwningPlayer())
    {
        FInputModeUIOnly Input;
        if (Selection) { Input.SetWidgetToFocus(Selection->TakeWidget()); }
        Controller->SetInputMode(Input); Controller->SetShowMouseCursor(true);
    }
}

FReply UGodFallPackCharacterPicker::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)
    { ClosePicker(); return FReply::Handled(); }
    return Super::NativeOnKeyDown(Geometry, Event);
}
