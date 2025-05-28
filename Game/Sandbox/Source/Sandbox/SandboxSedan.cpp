#include "SandboxSedan.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "GASShooter/Public/Characters/Heroes/GSHeroCharacter.h"
#include "GASShooter/Public/Weapons/GSWeapon.h"
#include "Components/SkeletalMeshComponent.h"

ASandboxSedan::ASandboxSedan()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create exit point component and attach to root
	ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPoint"));
	ExitPoint->SetupAttachment(RootComponent);

	// Initialize variables
	StoredPlayerCharacter = nullptr;
	StoredPlayerController = nullptr;
	bIsInCar = false;
	ReEntryCooldownTime = 2.0f; // 2 second cooldown
	LastExitTime = -10.0f; // Initialize to allow immediate first entry

	// Enable input
	AutoReceiveInput = EAutoReceiveInput::Player0;
}

void ASandboxSedan::BeginPlay()
{
	Super::BeginPlay();
}

void ASandboxSedan::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Bind exit vehicle input - support both E and F keys
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ASandboxSedan::RequestExitVehicle);
	PlayerInputComponent->BindAction("VehicleInteract", IE_Pressed, this, &ASandboxSedan::RequestExitVehicle);
}

void ASandboxSedan::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// When possessed, store the player controller and character
	APlayerController* PC = Cast<APlayerController>(NewController);
	if (PC)
	{
		StoredPlayerController = PC;
		
		// Get the player pawn the controller was originally possessing
		APawn* PreviousPawn = PC->GetPawn();
		if (PreviousPawn && PreviousPawn != this)
		{
			StoredPlayerCharacter = PreviousPawn;
		}
		
		if (!StoredPlayerCharacter)
		{
			StoredPlayerCharacter = UGameplayStatics::GetPlayerPawn(this, 0);
		}
		
		if (StoredPlayerCharacter)
		{
			bIsInCar = true;
			UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::PossessedBy - Player entered vehicle (generic pawn)"));
		}
	}
}

void ASandboxSedan::OnPlayerInteraction_Implementation(APawn* InteractingPawn)
{
	if (bIsInCar)
	{
		// Player is trying to exit - this shouldn't happen as input is handled differently
		UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::OnPlayerInteraction - Player trying to exit via interaction"));
		return;
	}

	// Check cooldown to prevent immediate re-entry
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastExitTime < ReEntryCooldownTime)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::OnPlayerInteraction - Still in cooldown period, ignoring entry attempt"));
		return;
	}

	// Player is entering vehicle
	APawn* PlayerPawn = InteractingPawn;
	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::OnPlayerInteraction - InteractingPawn is null"));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(PlayerPawn->GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("ASandboxSedan::OnPlayerInteraction - No PlayerController found"));
		return;
	}

	// Store references
	StoredPlayerCharacter = PlayerPawn;
	StoredPlayerController = PC;

	// Call Multicast to update visuals on server and all clients BEFORE hiding the pawn locally on server.
	Multicast_UpdateCharacterVisualsOnEnter(PlayerPawn, true);

	// Hide and disable the character
	PlayerPawn->SetActorHiddenInGame(true);
	if (UCapsuleComponent* Cap = PlayerPawn->FindComponentByClass<UCapsuleComponent>())
	{
		Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	// Keep tick enabled so ongoing AbilitySystem tasks don't dereference null pointers
	if (UAbilitySystemComponent* ASC = PlayerPawn->FindComponentByClass<UAbilitySystemComponent>())
	{
		ASC->CancelAllAbilities();
	}
	PlayerPawn->DisableInput(PC);

	// Possess the vehicle
	PC->Possess(this);
	EnableInput(PC);

	bIsInCar = true;

	UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::OnPlayerInteraction - Player entered vehicle successfully"));
}

void ASandboxSedan::RequestExitVehicle()
{
	if (GetLocalRole() < ROLE_Authority) // If client
	{
		Server_OnExitVehicle();
	}
	else // If server or standalone
	{
		Server_OnExitVehicle_Implementation();
	}
}

bool ASandboxSedan::Server_OnExitVehicle_Validate()
{
	// Basic validation: Check if we are actually in car and have references.
	// More sophisticated checks could be added (e.g., is vehicle moving too fast?)
	return bIsInCar && StoredPlayerCharacter && StoredPlayerController;
}

void ASandboxSedan::Server_OnExitVehicle_Implementation()
{
	UE_LOG(LogTemp, Error, TEXT("ASandboxSedan::Server_OnExitVehicle_Implementation - EXIT KEY PRESSED (E or F)!"));
	UE_LOG(LogTemp, Warning, TEXT("Debug: bIsInCar=%s, StoredPlayerCharacter=%s, StoredPlayerController=%s"), 
		bIsInCar ? TEXT("true") : TEXT("false"),
		StoredPlayerCharacter ? TEXT("valid") : TEXT("null"),
		StoredPlayerController ? TEXT("valid") : TEXT("null"));
	
	if (!bIsInCar || !StoredPlayerCharacter || !StoredPlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::Server_OnExitVehicle_Implementation - Cannot exit: not in car or missing references"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::Server_OnExitVehicle_Implementation - Player exiting vehicle"));

	// Calculate exit transform - move it further away from the vehicle
	FTransform ExitTransform = ExitPoint->GetComponentTransform();
	FVector ExitLocation = ExitTransform.GetLocation();
	
	// Move the exit location further from the vehicle to avoid immediate re-entry
	FVector VehicleLocation = GetActorLocation();
	FVector AwayDirection = (ExitLocation - VehicleLocation).GetSafeNormal();
	if (AwayDirection.IsNearlyZero())
	{
		// If exit point is at vehicle center, use right vector
		AwayDirection = GetActorRightVector();
	}
	ExitLocation = VehicleLocation + (AwayDirection * 400.0f); // Move 400 units away
	ExitTransform.SetLocation(ExitLocation);

	// Store references before clearing them
	APawn* CharacterToRestore = StoredPlayerCharacter;
	APlayerController* ControllerToRestore = StoredPlayerController;

	// If the character being restored is a GSHeroCharacter (or our SandboxHeroCharacter),
	// reset its bASCInputBound flag. This will allow BindASCInput() to re-bind abilities
	// when the character is re-possessed.
	AGSHeroCharacter* GSHero = Cast<AGSHeroCharacter>(CharacterToRestore);
	if (GSHero)
	{
		GSHero->bASCInputBound = false;
		UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::Server_OnExitVehicle_Implementation - Reset bASCInputBound for GSHeroCharacter prior to re-possession."));
	}

	// Clear references first to prevent re-entry
	StoredPlayerCharacter = nullptr;
	StoredPlayerController = nullptr;
	bIsInCar = false;

	// Disable input on this vehicle first
	DisableInput(ControllerToRestore);

	// Possess the character back
	if (ControllerToRestore && CharacterToRestore)
	{
		ControllerToRestore->Possess(CharacterToRestore);

		// Restore basic state generically
		CharacterToRestore->SetActorHiddenInGame(false);
		if (UCapsuleComponent* Cap = CharacterToRestore->FindComponentByClass<UCapsuleComponent>())
		{
			Cap->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		CharacterToRestore->SetActorEnableCollision(true);
		CharacterToRestore->SetActorTransform(ExitTransform, false, nullptr, ETeleportType::TeleportPhysics);
		
		AGSHeroCharacter* RestoredHero = Cast<AGSHeroCharacter>(CharacterToRestore);
		if (RestoredHero)
		{
			// Restore shadow casting for character's main mesh
			if (USkeletalMeshComponent* CharacterMesh3P = RestoredHero->GetMesh())
			{
				CharacterMesh3P->SetCastShadow(true); // Assuming it should cast shadows normally
				UE_LOG(LogTemp, Log, TEXT("ASandboxSedan: Re-enabled 3P mesh shadow for %s"), *RestoredHero->GetName());
			}

			// The character's PossessedBy -> SetupStartupPerspective -> SetPerspective flow
            // should handle re-equipping the weapon and setting its visibility correctly.
            // If GetCurrentWeapon() exists, SetPerspective will call Equip() on it.
			UE_LOG(LogTemp, Log, TEXT("ASandboxSedan: Character's SetPerspective will handle weapon re-equip for %s"), *RestoredHero->GetName());
		}

		// Re-enable input on the character
		CharacterToRestore->EnableInput(ControllerToRestore);
		
		// The engine will call SetupPlayerInputComponent on the CharacterToRestore as part of the possession process.
		// For GSHeroCharacter, its overridden SetupPlayerInputComponent will call BindASCInput().
		UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::Server_OnExitVehicle_Implementation - Character possession will handle input component setup."));
		
		// Set the exit time for cooldown
		LastExitTime = GetWorld()->GetTimeSeconds();
		
		UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::Server_OnExitVehicle_Implementation - Successfully exited vehicle and restored character"));
	}
}

void ASandboxSedan::Multicast_UpdateCharacterVisualsOnEnter_Implementation(APawn* CharacterPawn, bool bEnteringVehicle)
{
	AGSHeroCharacter* HeroCharacter = Cast<AGSHeroCharacter>(CharacterPawn);
	if (!HeroCharacter) 
	{
		UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::Multicast_UpdateCharacterVisualsOnEnter - Passed CharacterPawn is not a GSHeroCharacter or is null."));
		return;
	}

	if (bEnteringVehicle) // Player is entering
	{
		// Unequip current weapon to hide its meshes and handle state.
		// This should ideally be done for the locally controlled player or on the server.
		// Simulated proxies might not need to run the full UnEquip logic if it's complex,
		// but weapon visibility should be handled.
		if (HeroCharacter->IsLocallyControlled() || GetLocalRole() == ROLE_Authority) // Check if we are server or the owning client
		{
			if (AGSWeapon* CurrentWeapon = HeroCharacter->GetCurrentWeapon())
			{
				CurrentWeapon->UnEquip(); // UnEquip should handle hiding meshes
				UE_LOG(LogTemp, Log, TEXT("ASandboxSedan (Multicast %s): Unequipped weapon for %s"), GetLocalRole() == ROLE_Authority ? TEXT("Server") : TEXT("Client"), *HeroCharacter->GetName());
			}
		}
		else if (HeroCharacter->GetLocalRole() == ENetRole::ROLE_SimulatedProxy)
		{
		    // For simulated proxies, we just want to ensure the weapon's meshes are hidden if it has one.
            // The full UnEquip() might have gameplay logic we don't want to run on a non-owning client.
            if (AGSWeapon* CurrentWeapon = HeroCharacter->GetCurrentWeapon())
            { 
                // Access weapon meshes directly and hide them
                if(USkeletalMeshComponent* WeaponMesh1P = CurrentWeapon->GetWeaponMesh1P()) { WeaponMesh1P->SetVisibility(false, true); }
                if(USkeletalMeshComponent* WeaponMesh3P = CurrentWeapon->GetWeaponMesh3P()) { WeaponMesh3P->SetVisibility(false, true); }
				UE_LOG(LogTemp, Log, TEXT("ASandboxSedan (Multicast SimulatedProxy): Hid weapon meshes for %s"), *HeroCharacter->GetName());
            }
		}

		// Prevent character's main mesh from casting shadows while hidden
		if (USkeletalMeshComponent* CharacterMesh3P = HeroCharacter->GetMesh())
		{
			CharacterMesh3P->SetCastShadow(false);
			UE_LOG(LogTemp, Log, TEXT("ASandboxSedan (Multicast %s): Disabled 3P mesh shadow for %s"), GetLocalRole() == ROLE_Authority ? TEXT("Server") : TEXT("Client"), *HeroCharacter->GetName());
		}
	}
	// Exiting visuals (shadows, weapon re-equip) are handled by the server-driven repossession and SetPerspective logic.
} 