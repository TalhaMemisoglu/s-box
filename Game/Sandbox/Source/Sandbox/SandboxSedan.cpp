#include "SandboxSedan.h"
#include "SandboxCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

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

	// Bind exit vehicle input
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ASandboxSedan::OnExitVehicle);
}

void ASandboxSedan::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// When possessed, store the player controller and character
	APlayerController* PC = Cast<APlayerController>(NewController);
	if (PC)
	{
		StoredPlayerController = PC;
		
		// Get the player character from the controller's previous pawn
		APawn* PreviousPawn = PC->GetPawn();
		if (PreviousPawn && PreviousPawn != this)
		{
			StoredPlayerCharacter = Cast<ASandboxCharacter>(PreviousPawn);
		}
		
		// If that didn't work, try getting from world
		if (!StoredPlayerCharacter)
		{
			StoredPlayerCharacter = Cast<ASandboxCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
		}
		
		if (StoredPlayerCharacter)
		{
			// Mark character as in vehicle
			StoredPlayerCharacter->bIsInVehicle = true;
			StoredPlayerCharacter->CurrentVehicle = this;
			bIsInCar = true; // Set this to true when possessed
			
			UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::PossessedBy - Player entered vehicle, bIsInCar set to true"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ASandboxSedan::PossessedBy - Could not find StoredPlayerCharacter!"));
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

	// Player is entering vehicle
	ASandboxCharacter* PlayerCharacter = Cast<ASandboxCharacter>(InteractingPawn);
	if (!PlayerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::OnPlayerInteraction - InteractingPawn is not a SandboxCharacter"));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(PlayerCharacter->GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("ASandboxSedan::OnPlayerInteraction - No PlayerController found"));
		return;
	}

	// Store references
	StoredPlayerCharacter = PlayerCharacter;
	StoredPlayerController = PC;

	// Hide and disable the character
	PlayerCharacter->SetActorHiddenInGame(true);
	PlayerCharacter->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayerCharacter->SetActorTickEnabled(false);
	PlayerCharacter->DisableInput(PC);

	// Mark as in vehicle
	PlayerCharacter->bIsInVehicle = true;
	PlayerCharacter->CurrentVehicle = this;
	bIsInCar = true;

	// Possess the vehicle
	PC->Possess(this);

	UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::OnPlayerInteraction - Player entered vehicle successfully"));
}

void ASandboxSedan::OnExitVehicle()
{
	UE_LOG(LogTemp, Error, TEXT("ASandboxSedan::OnExitVehicle - E KEY PRESSED!"));
	UE_LOG(LogTemp, Warning, TEXT("Debug: bIsInCar=%s, StoredPlayerCharacter=%s, StoredPlayerController=%s"), 
		bIsInCar ? TEXT("true") : TEXT("false"),
		StoredPlayerCharacter ? TEXT("valid") : TEXT("null"),
		StoredPlayerController ? TEXT("valid") : TEXT("null"));
	
	if (!bIsInCar || !StoredPlayerCharacter || !StoredPlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::OnExitVehicle - Cannot exit: not in car or missing references"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("ASandboxSedan::OnExitVehicle - Player exiting vehicle"));

	// Calculate exit transform
	FTransform ExitTransform = ExitPoint->GetComponentTransform();
	
	// Mark as not in vehicle
	StoredPlayerCharacter->bIsInVehicle = false;
	StoredPlayerCharacter->CurrentVehicle = nullptr;
	bIsInCar = false;

	// Possess the character back
	StoredPlayerController->Possess(StoredPlayerCharacter);

	// Call the character's exit function to restore state and teleport
	StoredPlayerCharacter->OnPlayerExitVehicle(ExitTransform);

	// Clear references
	StoredPlayerCharacter = nullptr;
	StoredPlayerController = nullptr;
} 