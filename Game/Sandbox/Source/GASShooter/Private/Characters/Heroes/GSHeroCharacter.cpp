// Copyright 2020 Dan Kestranek.


#include "Characters/Heroes/GSHeroCharacter.h"
#include "Animation/AnimInstance.h"
#include "AI/GSHeroAIController.h"
#include "Camera/CameraComponent.h"
#include "Characters/Abilities/GSAbilitySystemComponent.h"
#include "Characters/Abilities/GSAbilitySystemGlobals.h"
#include "Characters/Abilities/AttributeSets/GSAmmoAttributeSet.h"
#include "Characters/Abilities/AttributeSets/GSAttributeSetBase.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GASShooter/GASShooterGameModeBase.h"
#include "GSBlueprintFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Player/GSPlayerController.h"
#include "Player/GSPlayerState.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"
#include "UI/GSFloatingStatusBarWidget.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Weapons/GSWeapon.h"

AGSHeroCharacter::AGSHeroCharacter(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bHasBeenInitialized = false;
	BaseTurnRate = 45.0f;
	BaseLookUpRate = 45.0f;
	bStartInFirstPersonPerspective = true;
	bIsFirstPersonPerspective = false;
	bWasInFirstPersonPerspectiveWhenKnockedDown = false;
	bASCInputBound = false;
	bChangedWeaponLocally = false;
	Default1PFOV = 90.0f;
	Default3PFOV = 80.0f;
	NoWeaponTag = FGameplayTag::RequestGameplayTag(FName("Weapon.Equipped.None"));
	WeaponChangingDelayReplicationTag = FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.IsChangingDelayReplication"));
	WeaponAmmoTypeNoneTag = FGameplayTag::RequestGameplayTag(FName("Weapon.Ammo.None"));
	WeaponAbilityTag = FGameplayTag::RequestGameplayTag(FName("Ability.Weapon"));
	CurrentWeaponTag = NoWeaponTag;
	Inventory = FGSHeroInventory();
	ReviveDuration = 4.0f;
	
	ThirdPersonCameraBoom = CreateDefaultSubobject<USpringArmComponent>(FName("CameraBoom"));
	ThirdPersonCameraBoom->SetupAttachment(RootComponent);
	ThirdPersonCameraBoom->bUsePawnControlRotation = true;
	ThirdPersonCameraBoom->SetRelativeLocation(FVector(0, 50, 68.492264));

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(FName("FollowCamera"));
	ThirdPersonCamera->SetupAttachment(ThirdPersonCameraBoom);
	ThirdPersonCamera->FieldOfView = Default3PFOV;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(FName("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->bUsePawnControlRotation = true;

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(FName("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(FirstPersonCamera);
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->bReceivesDecals = false;
	FirstPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
	FirstPersonMesh->CastShadow = false;
	FirstPersonMesh->SetVisibility(false, true);

	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionProfileName(FName("NoCollision"));
	GetMesh()->SetCollisionResponseToChannel(COLLISION_INTERACTABLE, ECollisionResponse::ECR_Overlap);
	GetMesh()->bCastHiddenShadow = true;
	GetMesh()->bReceivesDecals = false;

	UIFloatingStatusBarComponent = CreateDefaultSubobject<UWidgetComponent>(FName("UIFloatingStatusBarComponent"));
	UIFloatingStatusBarComponent->SetupAttachment(RootComponent);
	UIFloatingStatusBarComponent->SetRelativeLocation(FVector(0, 0, 120));
	UIFloatingStatusBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
	UIFloatingStatusBarComponent->SetDrawSize(FVector2D(500, 500));

	UIFloatingStatusBarClass = StaticLoadClass(UObject::StaticClass(), nullptr, TEXT("/Game/GASShooter/UI/UI_FloatingStatusBar_Hero.UI_FloatingStatusBar_Hero_C"));
	if (!UIFloatingStatusBarClass)
	{
		UE_LOG(LogTemp, Error, TEXT("%s() Failed to find UIFloatingStatusBarClass. If it was moved, please update the reference location in C++."), *FString(__FUNCTION__));
	}

	AutoPossessAI = EAutoPossessAI::PlacedInWorld;
	AIControllerClass = AGSHeroAIController::StaticClass();

	// Cache tags
	KnockedDownTag = FGameplayTag::RequestGameplayTag("State.KnockedDown");
	InteractingTag = FGameplayTag::RequestGameplayTag("State.Interacting");

	bShowLocation = false;
	LocationWidget = nullptr;
}

void AGSHeroCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGSHeroCharacter, Inventory);
	// Only replicate CurrentWeapon to simulated clients and manually sync CurrentWeeapon with Owner when we're ready.
	// This allows us to predict weapon changing.
	DOREPLIFETIME_CONDITION(AGSHeroCharacter, CurrentWeapon, COND_SimulatedOnly);
}

// Called to bind functionality to input
void AGSHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AGSHeroCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AGSHeroCharacter::MoveRight);

	PlayerInputComponent->BindAxis("LookUp", this, &AGSHeroCharacter::LookUp);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AGSHeroCharacter::LookUpRate);
	PlayerInputComponent->BindAxis("Turn", this, &AGSHeroCharacter::Turn);
	PlayerInputComponent->BindAxis("TurnRate", this, &AGSHeroCharacter::TurnRate);

	PlayerInputComponent->BindAction("TogglePerspective", IE_Pressed, this, &AGSHeroCharacter::TogglePerspective);
	PlayerInputComponent->BindAction("ToggleLocation", IE_Pressed, this, &AGSHeroCharacter::ToggleLocationDisplay);
	PlayerInputComponent->BindAction("ESCMenu", IE_Pressed, this, &AGSHeroCharacter::ToggleMenu);
	// Bind player input to the AbilitySystemComponent. Also called in OnRep_PlayerState because of a potential race condition.
	BindASCInput();
}

// Server only
void AGSHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	AGSPlayerState* PS = GetPlayerState<AGSPlayerState>();
	if (PS)
	{
		// Set the ASC on the Server. Clients do this in OnRep_PlayerState()
		AbilitySystemComponent = Cast<UGSAbilitySystemComponent>(PS->GetAbilitySystemComponent());

		// AI won't have PlayerControllers so we can init again here just to be sure. No harm in initing twice for heroes that have PlayerControllers.
		PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);

		WeaponChangingDelayReplicationTagChangedDelegateHandle = AbilitySystemComponent->RegisterGameplayTagEvent(WeaponChangingDelayReplicationTag)
			.AddUObject(this, &AGSHeroCharacter::WeaponChangingDelayReplicationTagChanged);

		// Set the AttributeSetBase for convenience attribute functions
		AttributeSetBase = PS->GetAttributeSetBase();

		AmmoAttributeSet = PS->GetAmmoAttributeSet();

		if (!bHasBeenInitialized)
		{
			// If we handle players disconnecting and rejoining in the future, we'll have to change this so that possession from rejoining doesn't reset attributes.
			// For now assume possession = spawn/respawn.
			InitializeAttributes();
			AddStartupEffects();
			AddCharacterAbilities(); // If this grants default items/ammo that should only happen once
			bHasBeenInitialized = true;
			UE_LOG(LogTemp, Warning, TEXT("AGSHeroCharacter::PossessedBy - First time initialization of attributes and abilities."));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AGSHeroCharacter::PossessedBy - Attributes and abilities already initialized, skipping full re-init."));
		}

		// Refresh HUD, ASC actor info etc. even on re-possession
		PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);

		AGSPlayerController* PC = Cast<AGSPlayerController>(GetController());
		if (PC)
		{
			PC->CreateHUD();
		}

		if (AbilitySystemComponent->GetTagCount(DeadTag) > 0)
		{
			// Set Health/Mana/Stamina to their max. This is only necessary for *Respawn*.
			SetHealth(GetMaxHealth());
			SetMana(GetMaxMana());
			SetStamina(GetMaxStamina());
			SetShield(GetMaxShield());
		}

		// Remove Dead tag
		AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(DeadTag));

		InitializeFloatingStatusBar();

		// If player is host on listen server, the floating status bar would have been created for them from BeginPlay before player possession, hide it
		if (IsLocallyControlled() && IsPlayerControlled() && UIFloatingStatusBarComponent && UIFloatingStatusBar)
		{
			UIFloatingStatusBarComponent->SetVisibility(false, true);
		}
	}

	SetupStartupPerspective();
}

UGSFloatingStatusBarWidget* AGSHeroCharacter::GetFloatingStatusBar()
{
	return UIFloatingStatusBar;
}

void AGSHeroCharacter::KnockDown()
{
	if (!HasAuthority())
	{
		return;
	}

	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->CancelAllAbilities();

		FGameplayTagContainer EffectTagsToRemove;
		EffectTagsToRemove.AddTag(EffectRemoveOnDeathTag);
		int32 NumEffectsRemoved = AbilitySystemComponent->RemoveActiveEffectsWithTags(EffectTagsToRemove);

		AbilitySystemComponent->ApplyGameplayEffectToSelf(Cast<UGameplayEffect>(KnockDownEffect->GetDefaultObject()), 1.0f, AbilitySystemComponent->MakeEffectContext());
	}

	SetHealth(GetMaxHealth());
	SetShield(0.0f);
}

void AGSHeroCharacter::PlayKnockDownEffects()
{
	// Store perspective to restore on Revive
	bWasInFirstPersonPerspectiveWhenKnockedDown = IsInFirstPersonPerspective();

	SetPerspective(false);

	// Play it here instead of in the ability to skip extra replication data
	if (DeathMontage)
	{
		PlayAnimMontage(DeathMontage);
	}

	if (AbilitySystemComponent)
	{
		FGameplayCueParameters GCParameters;
		GCParameters.Location = GetActorLocation();
		AbilitySystemComponent->ExecuteGameplayCueLocal(FGameplayTag::RequestGameplayTag("GameplayCue.Hero.KnockedDown"), GCParameters);
	}
}

void AGSHeroCharacter::PlayReviveEffects()
{
	// Restore perspective the player had when knocked down
	SetPerspective(bWasInFirstPersonPerspectiveWhenKnockedDown);

	// Play revive particles or sounds here (we don't have any)
	if (AbilitySystemComponent)
	{
		FGameplayCueParameters GCParameters;
		GCParameters.Location = GetActorLocation();
		AbilitySystemComponent->ExecuteGameplayCueLocal(FGameplayTag::RequestGameplayTag("GameplayCue.Hero.Revived"), GCParameters);
	}
}

void AGSHeroCharacter::FinishDying()
{
	// AGSHeroCharacter doesn't follow AGSCharacterBase's pattern of Die->Anim->FinishDying because AGSHeroCharacter can be knocked down
	// to either be revived, bleed out, or finished off by an enemy.

	if (!HasAuthority())
	{
		return;
	}

	RemoveAllWeaponsFromInventory();

	AbilitySystemComponent->RegisterGameplayTagEvent(WeaponChangingDelayReplicationTag).Remove(WeaponChangingDelayReplicationTagChangedDelegateHandle);

	AGASShooterGameModeBase* GM = Cast<AGASShooterGameModeBase>(GetWorld()->GetAuthGameMode());

	if (GM)
	{
		GM->HeroDied(GetController());
	}

	RemoveCharacterAbilities();

	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->CancelAllAbilities();

		FGameplayTagContainer EffectTagsToRemove;
		EffectTagsToRemove.AddTag(EffectRemoveOnDeathTag);
		int32 NumEffectsRemoved = AbilitySystemComponent->RemoveActiveEffectsWithTags(EffectTagsToRemove);

		AbilitySystemComponent->ApplyGameplayEffectToSelf(Cast<UGameplayEffect>(DeathEffect->GetDefaultObject()), 1.0f, AbilitySystemComponent->MakeEffectContext());
	}

	OnCharacterDied.Broadcast(this);

	Super::FinishDying();
}

bool AGSHeroCharacter::IsInFirstPersonPerspective() const
{
	return bIsFirstPersonPerspective;
}

USkeletalMeshComponent* AGSHeroCharacter::GetFirstPersonMesh() const
{
	return FirstPersonMesh;
}

USkeletalMeshComponent* AGSHeroCharacter::GetThirdPersonMesh() const
{
	return GetMesh();
}

AGSWeapon* AGSHeroCharacter::GetCurrentWeapon() const
{
	return CurrentWeapon;
}

bool AGSHeroCharacter::AddWeaponToInventory(AGSWeapon* NewWeapon, bool bEquipWeapon)
{
	if (DoesWeaponExistInInventory(NewWeapon))
	{
		USoundCue* PickupSound = NewWeapon->GetPickupSound();

		if (PickupSound && IsLocallyControlled())
		{
			UGameplayStatics::SpawnSoundAttached(PickupSound, GetRootComponent());
		}

		if (GetLocalRole() < ROLE_Authority)
		{
			return false;
		}

		// Create a dynamic instant Gameplay Effect to give the primary and secondary ammo
		UGameplayEffect* GEAmmo = NewObject<UGameplayEffect>(GetTransientPackage(), FName(TEXT("Ammo")));
		GEAmmo->DurationPolicy = EGameplayEffectDurationType::Instant;

		if (NewWeapon->PrimaryAmmoType != WeaponAmmoTypeNoneTag)
		{
			int32 Idx = GEAmmo->Modifiers.Num();
			GEAmmo->Modifiers.SetNum(Idx + 1);

			FGameplayModifierInfo& InfoPrimaryAmmo = GEAmmo->Modifiers[Idx];
			InfoPrimaryAmmo.ModifierMagnitude = FScalableFloat(NewWeapon->GetPrimaryClipAmmo());
			InfoPrimaryAmmo.ModifierOp = EGameplayModOp::Additive;
			InfoPrimaryAmmo.Attribute = UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(NewWeapon->PrimaryAmmoType);
		}

		if (NewWeapon->SecondaryAmmoType != WeaponAmmoTypeNoneTag)
		{
			int32 Idx = GEAmmo->Modifiers.Num();
			GEAmmo->Modifiers.SetNum(Idx + 1);

			FGameplayModifierInfo& InfoSecondaryAmmo = GEAmmo->Modifiers[Idx];
			InfoSecondaryAmmo.ModifierMagnitude = FScalableFloat(NewWeapon->GetSecondaryClipAmmo());
			InfoSecondaryAmmo.ModifierOp = EGameplayModOp::Additive;
			InfoSecondaryAmmo.Attribute = UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(NewWeapon->SecondaryAmmoType);
		}

		if (GEAmmo->Modifiers.Num() > 0)
		{
			AbilitySystemComponent->ApplyGameplayEffectToSelf(GEAmmo, 1.0f, AbilitySystemComponent->MakeEffectContext());
		}

		NewWeapon->Destroy();

		return false;
	}

	if (GetLocalRole() < ROLE_Authority)
	{
		return false;
	}

	Inventory.Weapons.Add(NewWeapon);
	NewWeapon->SetOwningCharacter(this);
	NewWeapon->AddAbilities();

	if (bEquipWeapon)
	{
		EquipWeapon(NewWeapon);
		ClientSyncCurrentWeapon(CurrentWeapon);
	}

	return true;
}

bool AGSHeroCharacter::RemoveWeaponFromInventory(AGSWeapon* WeaponToRemove)
{
	if (DoesWeaponExistInInventory(WeaponToRemove))
	{
		if (WeaponToRemove == CurrentWeapon)
		{
			UnEquipCurrentWeapon();
		}

		Inventory.Weapons.Remove(WeaponToRemove);
		WeaponToRemove->RemoveAbilities();
		WeaponToRemove->SetOwningCharacter(nullptr);
		WeaponToRemove->ResetWeapon();

		// Add parameter to drop weapon?

		return true;
	}

	return false;
}

void AGSHeroCharacter::RemoveAllWeaponsFromInventory()
{
	if (GetLocalRole() < ROLE_Authority)
	{
		return;
	}

	UnEquipCurrentWeapon();

	float radius = 50.0f;
	float NumWeapons = Inventory.Weapons.Num();

	for (int32 i = Inventory.Weapons.Num() - 1; i >= 0; i--)
	{
		AGSWeapon* Weapon = Inventory.Weapons[i];
		RemoveWeaponFromInventory(Weapon);

		// Set the weapon up as a pickup

		float OffsetX = radius * FMath::Cos((i / NumWeapons) * 2.0f * PI);
		float OffsetY = radius * FMath::Sin((i / NumWeapons) * 2.0f * PI);
		Weapon->OnDropped(GetActorLocation() + FVector(OffsetX, OffsetY, 0.0f));
	}
}

void AGSHeroCharacter::EquipWeapon(AGSWeapon* NewWeapon)
{
	if (GetLocalRole() < ROLE_Authority)
	{
		ServerEquipWeapon(NewWeapon);
		SetCurrentWeapon(NewWeapon, CurrentWeapon);
		bChangedWeaponLocally = true;
	}
	else
	{
		SetCurrentWeapon(NewWeapon, CurrentWeapon);
	}
}

void AGSHeroCharacter::ServerEquipWeapon_Implementation(AGSWeapon* NewWeapon)
{
	EquipWeapon(NewWeapon);
}

bool AGSHeroCharacter::ServerEquipWeapon_Validate(AGSWeapon* NewWeapon)
{
	return true;
}

void AGSHeroCharacter::NextWeapon()
{
	if (Inventory.Weapons.Num() < 2)
	{
		return;
	}

	int32 CurrentWeaponIndex = Inventory.Weapons.Find(CurrentWeapon);
	UnEquipCurrentWeapon();

	if (CurrentWeaponIndex == INDEX_NONE)
	{
		EquipWeapon(Inventory.Weapons[0]);
	}
	else
	{
		EquipWeapon(Inventory.Weapons[(CurrentWeaponIndex + 1) % Inventory.Weapons.Num()]);
	}
}

void AGSHeroCharacter::PreviousWeapon()
{
	if (Inventory.Weapons.Num() < 2)
	{
		return;
	}

	int32 CurrentWeaponIndex = Inventory.Weapons.Find(CurrentWeapon);

	UnEquipCurrentWeapon();

	if (CurrentWeaponIndex == INDEX_NONE)
	{
		EquipWeapon(Inventory.Weapons[0]);
	}
	else
	{
		int32 IndexOfPrevWeapon = FMath::Abs(CurrentWeaponIndex - 1 + Inventory.Weapons.Num()) % Inventory.Weapons.Num();
		EquipWeapon(Inventory.Weapons[IndexOfPrevWeapon]);
	}
}

FName AGSHeroCharacter::GetWeaponAttachPoint()
{
	return WeaponAttachPoint;
}

int32 AGSHeroCharacter::GetPrimaryClipAmmo() const
{
	if (CurrentWeapon)
	{
		return CurrentWeapon->GetPrimaryClipAmmo();
	}

	return 0;
}

int32 AGSHeroCharacter::GetMaxPrimaryClipAmmo() const
{
	if (CurrentWeapon)
	{
		return CurrentWeapon->GetMaxPrimaryClipAmmo();
	}

	return 0;
}

int32 AGSHeroCharacter::GetPrimaryReserveAmmo() const
{
	if (CurrentWeapon && AmmoAttributeSet)
	{
		FGameplayAttribute Attribute = AmmoAttributeSet->GetReserveAmmoAttributeFromTag(CurrentWeapon->PrimaryAmmoType);
		if (Attribute.IsValid())
		{
			return AbilitySystemComponent->GetNumericAttribute(Attribute);
		}
	}

	return 0;
}

int32 AGSHeroCharacter::GetSecondaryClipAmmo() const
{
	if (CurrentWeapon)
	{
		return CurrentWeapon->GetSecondaryClipAmmo();
	}

	return 0;
}

int32 AGSHeroCharacter::GetMaxSecondaryClipAmmo() const
{
	if (CurrentWeapon)
	{
		return CurrentWeapon->GetMaxSecondaryClipAmmo();
	}

	return 0;
}

int32 AGSHeroCharacter::GetSecondaryReserveAmmo() const
{
	if (CurrentWeapon)
	{
		FGameplayAttribute Attribute = AmmoAttributeSet->GetReserveAmmoAttributeFromTag(CurrentWeapon->SecondaryAmmoType);
		if (Attribute.IsValid())
		{
			return AbilitySystemComponent->GetNumericAttribute(Attribute);
		}
	}

	return 0;
}

int32 AGSHeroCharacter::GetNumWeapons() const
{
	return Inventory.Weapons.Num();
}

bool AGSHeroCharacter::IsAvailableForInteraction_Implementation(UPrimitiveComponent* InteractionComponent) const
{
	// Hero is available to be revived if knocked down and is not already being revived.
	// If you want multiple heroes reviving someone to speed it up, you would need to change GA_Interact
	// (outside the scope of this sample).
	if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(KnockedDownTag)
		&& !AbilitySystemComponent->HasMatchingGameplayTag(InteractingTag))
	{
		return true;
	}
	
	return IGSInteractable::IsAvailableForInteraction_Implementation(InteractionComponent);
}

float AGSHeroCharacter::GetInteractionDuration_Implementation(UPrimitiveComponent* InteractionComponent) const
{
	if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(KnockedDownTag))
	{
		return ReviveDuration;
	}

	return IGSInteractable::GetInteractionDuration_Implementation(InteractionComponent);
}

void AGSHeroCharacter::PreInteract_Implementation(AActor* InteractingActor, UPrimitiveComponent* InteractionComponent)
{
	if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(KnockedDownTag) && HasAuthority())
	{
		AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(FGameplayTag::RequestGameplayTag("Ability.Revive")));
	}
}

void AGSHeroCharacter::PostInteract_Implementation(AActor* InteractingActor, UPrimitiveComponent* InteractionComponent)
{
	if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(KnockedDownTag) && HasAuthority())
	{
		AbilitySystemComponent->ApplyGameplayEffectToSelf(Cast<UGameplayEffect>(ReviveEffect->GetDefaultObject()), 1.0f, AbilitySystemComponent->MakeEffectContext());
	}
}

void AGSHeroCharacter::GetPreInteractSyncType_Implementation(bool& bShouldSync, EAbilityTaskNetSyncType& Type, UPrimitiveComponent* InteractionComponent) const
{
	if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(KnockedDownTag))
	{
		bShouldSync = true;
		Type = EAbilityTaskNetSyncType::OnlyClientWait;
		return;
	}

	IGSInteractable::GetPreInteractSyncType_Implementation(bShouldSync, Type, InteractionComponent);
}

void AGSHeroCharacter::CancelInteraction_Implementation(UPrimitiveComponent* InteractionComponent)
{
	if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(KnockedDownTag) && HasAuthority())
	{
		FGameplayTagContainer CancelTags(FGameplayTag::RequestGameplayTag("Ability.Revive"));
		AbilitySystemComponent->CancelAbilities(&CancelTags);
	}
}

FSimpleMulticastDelegate* AGSHeroCharacter::GetTargetCancelInteractionDelegate(UPrimitiveComponent* InteractionComponent)
{
	return &InteractionCanceledDelegate;
}

/**
* On the Server, Possession happens before BeginPlay.
* On the Client, BeginPlay happens before Possession.
* So we can't use BeginPlay to do anything with the AbilitySystemComponent because we don't have it until the PlayerState replicates from possession.
*/
void AGSHeroCharacter::BeginPlay()
{
	Super::BeginPlay();


	//For coordinates
	if (LocationWidgetClass)
	{
		LocationWidget = CreateWidget<UUserWidget>(GetWorld(), LocationWidgetClass);
		if (LocationWidget)
		{
			LocationWidget->AddToViewport();
			LocationWidget->SetVisibility(ESlateVisibility::Hidden);

			// Get the text block reference by name
			LocationText = Cast<UTextBlock>(LocationWidget->GetWidgetFromName(TEXT("Text_Location")));
		}
	}

	StartingFirstPersonMeshLocation = FirstPersonMesh->GetRelativeLocation();

	// Only needed for Heroes placed in world and when the player is the Server.
	// On respawn, they are set up in PossessedBy.
	// When the player a client, the floating status bars are all set up in OnRep_PlayerState.
	InitializeFloatingStatusBar();

	// CurrentWeapon is replicated only to Simulated clients so sync the current weapon manually
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		ServerSyncCurrentWeapon();
	}
}

void AGSHeroCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Cancel being revived if killed
	//InteractionCanceledDelegate.Broadcast();
	Execute_InteractableCancelInteraction(this, GetThirdPersonMesh());

	// Clear CurrentWeaponTag on the ASC. This happens naturally in UnEquipCurrentWeapon() but
	// that is only called on the server from hero death (the OnRep_CurrentWeapon() would have
	// handled it on the client but that is never called due to the hero being marked pending
	// destroy). This makes sure the client has it cleared.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(CurrentWeaponTag);
		CurrentWeaponTag = NoWeaponTag;
		AbilitySystemComponent->AddLooseGameplayTag(CurrentWeaponTag);
	}

	Super::EndPlay(EndPlayReason);
}

void AGSHeroCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	StartingThirdPersonCameraBoomArmLength = ThirdPersonCameraBoom->TargetArmLength;
	StartingThirdPersonCameraBoomLocation = ThirdPersonCameraBoom->GetRelativeLocation();
	StartingThirdPersonMeshLocation = GetMesh()->GetRelativeLocation();

	GetWorldTimerManager().SetTimerForNextTick(this, &AGSHeroCharacter::SpawnDefaultInventory);
}

void AGSHeroCharacter::LookUp(float Value)
{
	if (IsAlive())
	{
		AddControllerPitchInput(Value);
	}
}

void AGSHeroCharacter::LookUpRate(float Value)
{
	if (IsAlive())
	{
		AddControllerPitchInput(Value * BaseLookUpRate * GetWorld()->DeltaTimeSeconds);
	}
}

void AGSHeroCharacter::Turn(float Value)
{
	if (IsAlive())
	{
		AddControllerYawInput(Value);
	}
}

void AGSHeroCharacter::TurnRate(float Value)
{
	if (IsAlive())
	{
		AddControllerYawInput(Value * BaseTurnRate * GetWorld()->DeltaTimeSeconds);
	}
}

void AGSHeroCharacter::MoveForward(float Value)
{
	if (IsAlive())
	{
		AddMovementInput(UKismetMathLibrary::GetForwardVector(FRotator(0, GetControlRotation().Yaw, 0)), Value);
	}
}

void AGSHeroCharacter::MoveRight(float Value)
{
	if (IsAlive())
	{
		AddMovementInput(UKismetMathLibrary::GetRightVector(FRotator(0, GetControlRotation().Yaw, 0)), Value);
	}
}

void AGSHeroCharacter::TogglePerspective()
{
	// If knocked down, always be in 3rd person
	if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(KnockedDownTag))
	{
		return;
	}

	bIsFirstPersonPerspective = !bIsFirstPersonPerspective;
	SetPerspective(bIsFirstPersonPerspective);
}

void AGSHeroCharacter::SetPerspective(bool InIsFirstPersonPerspective)
{
	// If knocked down, always be in 3rd person. This check should remain.
	if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(KnockedDownTag) && InIsFirstPersonPerspective)
	{
		// If we're trying to go to 1P but are knocked down, force 3P
		InIsFirstPersonPerspective = false; 
	}

	bIsFirstPersonPerspective = InIsFirstPersonPerspective; // Set the member variable

    FString Context = (GetNetMode() == NM_Client) ? TEXT("Client") : ((GetNetMode() == NM_ListenServer || GetNetMode() == NM_DedicatedServer) ? TEXT("Server") : TEXT("Standalone"));
    UE_LOG(LogTemp, Log, TEXT("AGSHeroCharacter::SetPerspective (%s) on %s. New Perspective: %s. CurrentWeapon: %s"),
        *GetName(),
        *Context,
        bIsFirstPersonPerspective ? TEXT("First Person") : TEXT("Third Person"), // Use the updated bIsFirstPersonPerspective
        GetCurrentWeapon() ? *GetCurrentWeapon()->GetName() : TEXT("NULL")
    );

	USkeletalMeshComponent* TPPMesh = GetMesh();

	if (!FirstPersonMesh || !TPPMesh || !FirstPersonCamera || !ThirdPersonCamera)
	{
		UE_LOG(LogTemp, Error, TEXT("AGSHeroCharacter::SetPerspective - Crucial component (Mesh or Camera) is missing!"));
		return;
	}

	// Handle mesh visibility based on perspective (OwnerNoSee and explicit Visibility)
	if (bIsFirstPersonPerspective)
	{
		TPPMesh->SetOwnerNoSee(true);
		FirstPersonMesh->SetOwnerNoSee(false);
		FirstPersonMesh->SetVisibility(true);
		// TPPMesh visibility for others is true by default, SetOwnerNoSee hides it locally.
        // Potentially adjust 3P mesh location for shadow as in original code:
        TPPMesh->SetRelativeLocation(StartingThirdPersonMeshLocation + FVector(-120.0f, 0.0f, 0.0f));
	}
	else // Third Person Perspective
	{
		TPPMesh->SetOwnerNoSee(false);
		FirstPersonMesh->SetOwnerNoSee(true);
		FirstPersonMesh->SetVisibility(false);
        TPPMesh->SetVisibility(true); // Ensure 3P mesh is visible
        // Reset the third person mesh location as in original code:
        TPPMesh->SetRelativeLocation(StartingThirdPersonMeshLocation);
	}

	// Camera Activation and View Target for locally controlled player
	APlayerController* PC = GetController<APlayerController>(); // Use APlayerController for wider compatibility
	if (PC && PC->IsLocalPlayerController()) // Check if it's the local player controller
	{
		if (bIsFirstPersonPerspective)
		{
			if (ThirdPersonCamera) ThirdPersonCamera->Deactivate();
			if (FirstPersonCamera) FirstPersonCamera->Activate();
		}
		else
		{
			if (FirstPersonCamera) FirstPersonCamera->Deactivate();
			if (ThirdPersonCamera) ThirdPersonCamera->Activate();
		}
		PC->SetViewTarget(this); // Ensure view target is set to self
	}
	
	// Refresh weapon if equipped - This was in the original, good to keep.
	// The Equip function in AGSWeapon should handle its own 1P/3P mesh visibility.
	AGSWeapon* CurrentWeaponPtr = GetCurrentWeapon();
	if (CurrentWeaponPtr)
	{
        UE_LOG(LogTemp, Log, TEXT("AGSHeroCharacter::SetPerspective (%s) on %s - Calling Equip() on %s"), *GetName(), *Context, *CurrentWeaponPtr->GetName());
		CurrentWeaponPtr->Equip(); 
	}
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("AGSHeroCharacter::SetPerspective (%s) on %s - CurrentWeapon is NULL, cannot call Equip."), *GetName(), *Context);
    }
}

void AGSHeroCharacter::InitializeFloatingStatusBar()
{
	// Only create once
	if (UIFloatingStatusBar || !IsValid(AbilitySystemComponent))
	{
		return;
	}

	// Don't create for locally controlled player. We could add a game setting to toggle this later.
	if (IsPlayerControlled() && IsLocallyControlled())
	{
		return;
	}

	// Need a valid PlayerState
	if (!GetPlayerState())
	{
		return;
	}

	// Setup UI for Locally Owned Players only, not AI or the server's copy of the PlayerControllers
	AGSPlayerController* PC = Cast<AGSPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (PC && PC->IsLocalPlayerController())
	{
		if (UIFloatingStatusBarClass)
		{
			UIFloatingStatusBar = CreateWidget<UGSFloatingStatusBarWidget>(PC, UIFloatingStatusBarClass);
			if (UIFloatingStatusBar && UIFloatingStatusBarComponent)
			{
				UIFloatingStatusBarComponent->SetWidget(UIFloatingStatusBar);

				// Setup the floating status bar
				UIFloatingStatusBar->SetHealthPercentage(GetHealth() / GetMaxHealth());
				UIFloatingStatusBar->SetManaPercentage(GetMana() / GetMaxMana());
				UIFloatingStatusBar->SetShieldPercentage(GetShield() / GetMaxShield());
				UIFloatingStatusBar->OwningCharacter = this;
				UIFloatingStatusBar->SetCharacterName(CharacterName);
			}
		}
	}
}

// Client only
void AGSHeroCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	AGSPlayerState* PS = GetPlayerState<AGSPlayerState>();
	if (PS)
	{
		// Set the ASC for clients. Server does this in PossessedBy.
		AbilitySystemComponent = Cast<UGSAbilitySystemComponent>(PS->GetAbilitySystemComponent());

		// Init ASC Actor Info for clients. Server will init its ASC when it possesses a new Actor.
		AbilitySystemComponent->InitAbilityActorInfo(PS, this);

		// Bind player input to the AbilitySystemComponent. Also called in SetupPlayerInputComponent because of a potential race condition.
		BindASCInput();

		AbilitySystemComponent->AbilityFailedCallbacks.AddUObject(this, &AGSHeroCharacter::OnAbilityActivationFailed);

		// Set the AttributeSetBase for convenience attribute functions
		AttributeSetBase = PS->GetAttributeSetBase();
		
		AmmoAttributeSet = PS->GetAmmoAttributeSet();

		if (!bHasBeenInitialized && GetLocalRole() == ROLE_AutonomousProxy) // Only run init once on autonomous proxy
		{
			// If we handle players disconnecting and rejoining in the future, we'll have to change this so that posession from rejoining doesn't reset attributes.
			// For now assume possession = spawn/respawn.
			InitializeAttributes();
			// AddStartupEffects(); // Typically server-only, but if client needs some...
			// AddCharacterAbilities(); // Typically server-only
			bHasBeenInitialized = true;
			UE_LOG(LogTemp, Warning, TEXT("AGSHeroCharacter::OnRep_PlayerState - First time initialization of attributes."));
		}
		else if (GetLocalRole() == ROLE_AutonomousProxy)
		{
			 UE_LOG(LogTemp, Warning, TEXT("AGSHeroCharacter::OnRep_PlayerState - Attributes already initialized, skipping full re-init."));
		}

		AGSPlayerController* PC = Cast<AGSPlayerController>(GetController());
		if (PC)
		{
			PC->CreateHUD();
		}
		
		if (CurrentWeapon)
		{
			// If current weapon repped before PlayerState, set tag on ASC
			AbilitySystemComponent->AddLooseGameplayTag(CurrentWeaponTag);
			// Update owning character and ASC just in case it repped before PlayerState
			CurrentWeapon->SetOwningCharacter(this);

			if (!PrimaryReserveAmmoChangedDelegateHandle.IsValid())
			{
				PrimaryReserveAmmoChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(CurrentWeapon->PrimaryAmmoType)).AddUObject(this, &AGSHeroCharacter::CurrentWeaponPrimaryReserveAmmoChanged);
			}
			if (!SecondaryReserveAmmoChangedDelegateHandle.IsValid())
			{
				SecondaryReserveAmmoChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(CurrentWeapon->SecondaryAmmoType)).AddUObject(this, &AGSHeroCharacter::CurrentWeaponSecondaryReserveAmmoChanged);
			}
		}

		if (AbilitySystemComponent->GetTagCount(DeadTag) > 0)
		{
			// Set Health/Mana/Stamina/Shield to their max. This is only for *Respawn*. It will be set (replicated) by the
			// Server, but we call it here just to be a little more responsive.
			SetHealth(GetMaxHealth());
			SetMana(GetMaxMana());
			SetStamina(GetMaxStamina());
			SetShield(GetMaxShield());
		}

		// Simulated on proxies don't have their PlayerStates yet when BeginPlay is called so we call it again here
		InitializeFloatingStatusBar();
	}
}

void AGSHeroCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	SetupStartupPerspective();
}

void AGSHeroCharacter::BindASCInput()
{
	if (!bASCInputBound && IsValid(AbilitySystemComponent) && IsValid(InputComponent))
	{
		AbilitySystemComponent->BindAbilityActivationToInputComponent(InputComponent, FGameplayAbilityInputBinds(FString("ConfirmTarget"),
			FString("CancelTarget"), FString("EGSAbilityInputID"), static_cast<int32>(EGSAbilityInputID::Confirm), static_cast<int32>(EGSAbilityInputID::Cancel)));

		bASCInputBound = true;
	}
}

void AGSHeroCharacter::SpawnDefaultInventory()
{
	if (GetLocalRole() < ROLE_Authority)
	{
		return;
	}

	int32 NumWeaponClasses = DefaultInventoryWeaponClasses.Num();
	for (int32 i = 0; i < NumWeaponClasses; i++)
	{
		if (!DefaultInventoryWeaponClasses[i])
		{
			// An empty item was added to the Array in blueprint
			continue;
		}

		AGSWeapon* NewWeapon = GetWorld()->SpawnActorDeferred<AGSWeapon>(DefaultInventoryWeaponClasses[i],
			FTransform::Identity, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		NewWeapon->bSpawnWithCollision = false;
		NewWeapon->FinishSpawning(FTransform::Identity);

		bool bEquipFirstWeapon = i == 0;
		AddWeaponToInventory(NewWeapon, bEquipFirstWeapon);
	}
}

void AGSHeroCharacter::SetupStartupPerspective()
{
	// If player-controlled (not AI)
	if (IsPlayerControlled())
	{
		// For locally controlled player, determine initial perspective and apply it.
		APlayerController* PC = GetController<APlayerController>();
		if (PC && PC->IsLocalController())
		{
			SetPerspective(bStartInFirstPersonPerspective);
		}
		// For simulated proxies of players, they generally should always see the 3P mesh.
		// SetPerspective with SetOwnerNoSee handles this for the local player's view.
		// No special handling needed here for simulated proxies beyond what SetPerspective does if called.
		// However, SetPerspective is primarily for the local player's camera and mesh setup.
		// Simulated proxies will rely on replication for mesh visibility state if needed beyond default.
	}
	else // AI controlled
	{
		// AI usually doesn't have a first-person perspective and doesn't need camera setup like players.
		SetPerspective(false); // Force AI to 3rd person setup for meshes
		if (FirstPersonMesh) FirstPersonMesh->SetVisibility(false, true); // Explicitly hide 1P
		if (GetMesh()) GetMesh()->SetVisibility(true, true); // Explicitly show 3P
	}
}

bool AGSHeroCharacter::DoesWeaponExistInInventory(AGSWeapon* InWeapon)
{
	//UE_LOG(LogTemp, Log, TEXT("%s InWeapon class %s"), *FString(__FUNCTION__), *InWeapon->GetClass()->GetName());

	for (AGSWeapon* Weapon : Inventory.Weapons)
	{
		if (Weapon && InWeapon && Weapon->GetClass() == InWeapon->GetClass())
		{
			return true;
		}
	}

	return false;
}

void AGSHeroCharacter::SetCurrentWeapon(AGSWeapon* NewWeapon, AGSWeapon* LastWeapon)
{
	if (NewWeapon == LastWeapon)
	{
		return;
	}

	// Cancel active weapon abilities
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer AbilityTagsToCancel = FGameplayTagContainer(WeaponAbilityTag);
		AbilitySystemComponent->CancelAbilities(&AbilityTagsToCancel);
	}

	UnEquipWeapon(LastWeapon);

	if (NewWeapon)
	{
		if (AbilitySystemComponent)
		{
			// Clear out potential NoWeaponTag
			AbilitySystemComponent->RemoveLooseGameplayTag(CurrentWeaponTag);
		}

		// Weapons coming from OnRep_CurrentWeapon won't have the owner set
		CurrentWeapon = NewWeapon;
		CurrentWeapon->SetOwningCharacter(this);
		CurrentWeapon->Equip();
		CurrentWeaponTag = CurrentWeapon->WeaponTag;

		if (AbilitySystemComponent)
		{
			AbilitySystemComponent->AddLooseGameplayTag(CurrentWeaponTag);
		}

		AGSPlayerController* PC = GetController<AGSPlayerController>();
		if (PC && PC->IsLocalController())
		{
			PC->SetEquippedWeaponPrimaryIconFromSprite(CurrentWeapon->PrimaryIcon);
			PC->SetEquippedWeaponStatusText(CurrentWeapon->StatusText);
			PC->SetPrimaryClipAmmo(CurrentWeapon->GetPrimaryClipAmmo());
			PC->SetPrimaryReserveAmmo(GetPrimaryReserveAmmo());
			PC->SetHUDReticle(CurrentWeapon->GetPrimaryHUDReticleClass());
		}

		NewWeapon->OnPrimaryClipAmmoChanged.AddDynamic(this, &AGSHeroCharacter::CurrentWeaponPrimaryClipAmmoChanged);
		NewWeapon->OnSecondaryClipAmmoChanged.AddDynamic(this, &AGSHeroCharacter::CurrentWeaponSecondaryClipAmmoChanged);
		
		if (AbilitySystemComponent)
		{
			PrimaryReserveAmmoChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(CurrentWeapon->PrimaryAmmoType)).AddUObject(this, &AGSHeroCharacter::CurrentWeaponPrimaryReserveAmmoChanged);
			SecondaryReserveAmmoChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(CurrentWeapon->SecondaryAmmoType)).AddUObject(this, &AGSHeroCharacter::CurrentWeaponSecondaryReserveAmmoChanged);
		}

		UAnimMontage* Equip1PMontage = CurrentWeapon->GetEquip1PMontage();
		if (Equip1PMontage && GetFirstPersonMesh())
		{
			GetFirstPersonMesh()->GetAnimInstance()->Montage_Play(Equip1PMontage);
		}

		UAnimMontage* Equip3PMontage = CurrentWeapon->GetEquip3PMontage();
		if (Equip3PMontage && GetThirdPersonMesh())
		{
			GetThirdPersonMesh()->GetAnimInstance()->Montage_Play(Equip3PMontage);
		}
	}
	else
	{
		// This will clear HUD, tags etc
		UnEquipCurrentWeapon();
	}
}

void AGSHeroCharacter::UnEquipWeapon(AGSWeapon* WeaponToUnEquip)
{
	//TODO this will run into issues when calling UnEquipWeapon explicitly and the WeaponToUnEquip == CurrentWeapon

	if (WeaponToUnEquip)
	{
		WeaponToUnEquip->OnPrimaryClipAmmoChanged.RemoveDynamic(this, &AGSHeroCharacter::CurrentWeaponPrimaryClipAmmoChanged);
		WeaponToUnEquip->OnSecondaryClipAmmoChanged.RemoveDynamic(this, &AGSHeroCharacter::CurrentWeaponSecondaryClipAmmoChanged);

		if (AbilitySystemComponent)
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(WeaponToUnEquip->PrimaryAmmoType)).Remove(PrimaryReserveAmmoChangedDelegateHandle);
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UGSAmmoAttributeSet::GetReserveAmmoAttributeFromTag(WeaponToUnEquip->SecondaryAmmoType)).Remove(SecondaryReserveAmmoChangedDelegateHandle);
		}
		
		WeaponToUnEquip->UnEquip();
	}
}

void AGSHeroCharacter::UnEquipCurrentWeapon()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(CurrentWeaponTag);
		CurrentWeaponTag = NoWeaponTag;
		AbilitySystemComponent->AddLooseGameplayTag(CurrentWeaponTag);
	}

	UnEquipWeapon(CurrentWeapon);
	CurrentWeapon = nullptr;

	AGSPlayerController* PC = GetController<AGSPlayerController>();
	if (PC && PC->IsLocalController())
	{
		PC->SetEquippedWeaponPrimaryIconFromSprite(nullptr);
		PC->SetEquippedWeaponStatusText(FText());
		PC->SetPrimaryClipAmmo(0);
		PC->SetPrimaryReserveAmmo(0);
		PC->SetHUDReticle(nullptr);
	}
}

void AGSHeroCharacter::CurrentWeaponPrimaryClipAmmoChanged(int32 OldPrimaryClipAmmo, int32 NewPrimaryClipAmmo)
{
	AGSPlayerController* PC = GetController<AGSPlayerController>();
	if (PC && PC->IsLocalController())
	{
		PC->SetPrimaryClipAmmo(NewPrimaryClipAmmo);
	}
}

void AGSHeroCharacter::CurrentWeaponSecondaryClipAmmoChanged(int32 OldSecondaryClipAmmo, int32 NewSecondaryClipAmmo)
{
	AGSPlayerController* PC = GetController<AGSPlayerController>();
	if (PC && PC->IsLocalController())
	{
		PC->SetSecondaryClipAmmo(NewSecondaryClipAmmo);
	}
}

void AGSHeroCharacter::CurrentWeaponPrimaryReserveAmmoChanged(const FOnAttributeChangeData& Data)
{
	AGSPlayerController* PC = GetController<AGSPlayerController>();
	if (PC && PC->IsLocalController())
	{
		PC->SetPrimaryReserveAmmo(Data.NewValue);
	}
}

void AGSHeroCharacter::CurrentWeaponSecondaryReserveAmmoChanged(const FOnAttributeChangeData& Data)
{
	AGSPlayerController* PC = GetController<AGSPlayerController>();
	if (PC && PC->IsLocalController())
	{
		PC->SetSecondaryReserveAmmo(Data.NewValue);
	}
}

void AGSHeroCharacter::WeaponChangingDelayReplicationTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag == WeaponChangingDelayReplicationTag)
	{
		if (NewCount < 1)
		{
			// We only replicate the current weapon to simulated proxies so manually sync it when the weapon changing delay replication
			// tag is removed. We keep the weapon changing tag on for ~1s after the equip montage to allow for activating changing weapon
			// again without the server trying to clobber the next locally predicted weapon.
			ClientSyncCurrentWeapon(CurrentWeapon);
		}
	}
}

void AGSHeroCharacter::OnRep_CurrentWeapon(AGSWeapon* LastWeapon)
{
	bChangedWeaponLocally = false;
	SetCurrentWeapon(CurrentWeapon, LastWeapon);
}

void AGSHeroCharacter::OnRep_Inventory()
{
	if (GetLocalRole() == ROLE_AutonomousProxy && Inventory.Weapons.Num() > 0 && !CurrentWeapon)
	{
		// Since we don't replicate the CurrentWeapon to the owning client, this is a way to ask the Server to sync
		// the CurrentWeapon after it's been spawned via replication from the Server.
		// The weapon spawning is replicated but the variable CurrentWeapon is not on the owning client.
		ServerSyncCurrentWeapon();
	}
}

void AGSHeroCharacter::OnAbilityActivationFailed(const UGameplayAbility* FailedAbility, const FGameplayTagContainer& FailTags)
{
	if (FailedAbility && FailedAbility->AbilityTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName("Ability.Weapon.IsChanging"))))
	{
		if (bChangedWeaponLocally)
		{
			// Ask the Server to resync the CurrentWeapon that we predictively changed
			UE_LOG(LogTemp, Warning, TEXT("%s Weapon Changing ability activation failed. Syncing CurrentWeapon. %s. %s"), *FString(__FUNCTION__),
				*UGSBlueprintFunctionLibrary::GetPlayerEditorWindowRole(GetWorld()), *FailTags.ToString());

			ServerSyncCurrentWeapon();
		}
	}
}

void AGSHeroCharacter::ServerSyncCurrentWeapon_Implementation()
{
	ClientSyncCurrentWeapon(CurrentWeapon);
}

bool AGSHeroCharacter::ServerSyncCurrentWeapon_Validate()
{
	return true;
}

void AGSHeroCharacter::ClientSyncCurrentWeapon_Implementation(AGSWeapon* InWeapon)
{
	AGSWeapon* LastWeapon = CurrentWeapon;
	CurrentWeapon = InWeapon;
	OnRep_CurrentWeapon(LastWeapon);
}

bool AGSHeroCharacter::ClientSyncCurrentWeapon_Validate(AGSWeapon* InWeapon)
{
	return true;
}

void AGSHeroCharacter::ToggleLocationDisplay()
{
	bShowLocation = !bShowLocation;

	if (LocationWidget)
	{
		if (bShowLocation)
		{
			// Show the widget and start updating coordinates
			LocationWidget->SetVisibility(ESlateVisibility::Visible);

			// Start the timer to update coordinates every 0.1 seconds
			GetWorld()->GetTimerManager().SetTimer(LocationUpdateTimer, this, &AGSHeroCharacter::UpdateLocationText, 0.1f, true);

			// Update immediately
			UpdateLocationText();
		}
		else
		{
			// Hide the widget and stop updating
			LocationWidget->SetVisibility(ESlateVisibility::Hidden);

			// Clear the timer
			GetWorld()->GetTimerManager().ClearTimer(LocationUpdateTimer);
		}
	}
	else if (bShowLocation && LocationWidgetClass)
	{
		// Create widget if it doesn't exist and we want to show it
		LocationWidget = CreateWidget<UUserWidget>(GetWorld(), LocationWidgetClass);
		if (LocationWidget)
		{
			LocationWidget->AddToViewport();
			LocationText = Cast<UTextBlock>(LocationWidget->GetWidgetFromName(TEXT("Text_Location")));

			// Start updating
			GetWorld()->GetTimerManager().SetTimer(LocationUpdateTimer, this, &AGSHeroCharacter::UpdateLocationText, 0.1f, true);
			UpdateLocationText();
		}
	}
}

void AGSHeroCharacter::UpdateLocationText()
{
	if (!LocationText)
	{
		return;
	}

	FVector Location = GetActorLocation();
	FString LocationString = FString::Printf(TEXT("X: %.1f\nY: %.1f\nZ: %.1f"), Location.X, Location.Y, Location.Z);
	LocationText->SetText(FText::FromString(LocationString));
}

void AGSHeroCharacter::ToggleMenu()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	if (!MenuInGameWidget && MenuInGameWidgetClass)
	{
		// Create the widget if it doesn't exist
		MenuInGameWidget = CreateWidget<UUserWidget>(GetWorld(), MenuInGameWidgetClass);
		if (MenuInGameWidget)
		{
			MenuInGameWidget->AddToViewport();
			MenuInGameWidget->SetVisibility(ESlateVisibility::Visible);
			
			// Show cursor and enable input
			PC->bShowMouseCursor = true;
			PC->SetInputMode(FInputModeGameAndUI());
		}
	}
	else if (MenuInGameWidget)
	{
		// Toggle visibility
		if (MenuInGameWidget->IsVisible())
		{
			MenuInGameWidget->SetVisibility(ESlateVisibility::Hidden);
			
			// Hide cursor and disable input
			PC->bShowMouseCursor = false;
			PC->SetInputMode(FInputModeGameOnly());
		}
		else
		{
			MenuInGameWidget->SetVisibility(ESlateVisibility::Visible);
			
			// Show cursor and enable input
			PC->bShowMouseCursor = true;
			PC->SetInputMode(FInputModeGameAndUI());
		}
	}
}