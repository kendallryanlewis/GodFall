#include "GodFallShopkeeper.h"
#include "GodFallFreeStore.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AGodFallShopkeeper::AGodFallShopkeeper()
{
    PrimaryActorTick.bCanEverTick=true;
    Capsule=CreateDefaultSubobject<UCapsuleComponent>(TEXT("ShopkeeperCapsule"));SetRootComponent(Capsule);
    Capsule->InitCapsuleSize(34,90);Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Capsule->SetCollisionObjectType(ECC_WorldStatic);Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
    Capsule->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);Capsule->SetGenerateOverlapEvents(false);
    CharacterMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ShopkeeperCharacter"));CharacterMesh->SetupAttachment(Capsule);
    CharacterMesh->SetRelativeLocation(FVector(0,0,-90));CharacterMesh->SetRelativeRotation(FRotator(0,-90,0));
    CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Prompt=CreateDefaultSubobject<UTextRenderComponent>(TEXT("ShopkeeperPrompt"));Prompt->SetupAttachment(Capsule);
    Prompt->SetRelativeLocation(FVector(0,0,125));Prompt->SetWorldSize(20);
    Prompt->SetHorizontalAlignment(EHTA_Center);Prompt->SetVerticalAlignment(EVRTA_TextBottom);
    Prompt->SetTextRenderColor(FColor(245,216,146));Prompt->SetText(NSLOCTEXT("GodFallStore","Quartermaster","QUARTERMASTER\nFree Store"));
}
void AGodFallShopkeeper::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if(IdleAnimation)CharacterMesh->OverrideAnimationData(IdleAnimation,true,true);
}
void AGodFallShopkeeper::BeginPlay()
{
    Super::BeginPlay();
    if(!GetWorld()->GetMapName().EndsWith(TEXT("L_GF_VerticalSlice"))){SetActorTickEnabled(false);Prompt->SetVisibility(false);return;}
    if(IdleAnimation)CharacterMesh->PlayAnimation(IdleAnimation,true);
    BindLocalInput();
}
void AGodFallShopkeeper::BindLocalInput()
{
    if(InputSubsystem)return;
    APlayerController* PC=UGameplayStatics::GetPlayerController(this,0);
    ULocalPlayer* LP=PC?PC->GetLocalPlayer():nullptr;
    auto* Subsystem=LP?LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>():nullptr;
    if(!Subsystem)return;
    LocalController=PC;InputSubsystem=Subsystem;
    InteractAction=NewObject<UInputAction>(this,TEXT("ShopkeeperInteract"));InteractAction->ValueType=EInputActionValueType::Boolean;
    InteractAction->bConsumeInput=false;
    InteractionMapping=NewObject<UInputMappingContext>(this,TEXT("ShopkeeperInput"));
    InteractionMapping->MapKey(InteractAction,EKeys::E);
    InteractionMapping->MapKey(InteractAction,EKeys::Gamepad_FaceButton_Left);
    auto* Enhanced=NewObject<UEnhancedInputComponent>(this,TEXT("ShopkeeperInputComponent"));
    InputComponent=Enhanced;Enhanced->RegisterComponent();Enhanced->BindAction(InteractAction,ETriggerEvent::Started,this,&ThisClass::Interact);
    EnableInput(PC);Subsystem->AddMappingContext(InteractionMapping,0);
}
bool AGodFallShopkeeper::CanInteract() const
{
    if(!GetWorld()||!GetWorld()->IsGameWorld()||!GetWorld()->GetMapName().EndsWith(TEXT("L_GF_VerticalSlice")))return false;
    if(!IsValid(LocalController)||UGameplayStatics::IsGamePaused(this)||LocalController->IsMoveInputIgnored()||LocalController->IsLookInputIgnored())return false;
    if(IsValid(ActiveStore)&&ActiveStore->IsInViewport())return false;
    const APawn* Player=LocalController->GetPawn();if(!Player)return false;
    const FVector Delta=Player->GetActorLocation()-GetActorLocation();
    return Delta.SizeSquared2D()<=FMath::Square(240.f)&&FMath::Abs(Delta.Z)<150.f;
}
void AGodFallShopkeeper::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);BindLocalInput();
    if(!IsValid(LocalController))return;
    const APawn* Player=LocalController->GetPawn();
    Prompt->SetVisibility(Player&&FVector::DistSquared2D(Player->GetActorLocation(),GetActorLocation())<FMath::Square(1000.f));
    FVector CameraLocation;FRotator CameraRotation;LocalController->GetPlayerViewPoint(CameraLocation,CameraRotation);
    Prompt->SetWorldRotation(FRotator(0,(CameraLocation-Prompt->GetComponentLocation()).Rotation().Yaw,0));
    Prompt->SetText(CanInteract()?NSLOCTEXT("GodFallStore","InteractPrompt","QUARTERMASTER\n[E / X] Open Free Store"):NSLOCTEXT("GodFallStore","Quartermaster","QUARTERMASTER\nFree Store"));
}
void AGodFallShopkeeper::Interact()
{
    if(!CanInteract())return;
    auto* Store=CreateWidget<UGodFallFreeStore>(LocalController);
    if(!Store||!LocalController->SetPause(true))return;
    ActiveStore=Store;Store->ConfigureWorldInteraction();Store->AddToViewport(500);
    FInputModeUIOnly Mode;Mode.SetWidgetToFocus(Store->TakeWidget());LocalController->SetInputMode(Mode);LocalController->bShowMouseCursor=true;
}
void AGodFallShopkeeper::EndPlay(const EEndPlayReason::Type Reason)
{
    if(IsValid(ActiveStore)&&ActiveStore->IsInViewport())ActiveStore->CloseStore();
    if(IsValid(InputSubsystem)&&InteractionMapping)InputSubsystem->RemoveMappingContext(InteractionMapping);
    if(IsValid(LocalController))DisableInput(LocalController);
    Super::EndPlay(Reason);
}
