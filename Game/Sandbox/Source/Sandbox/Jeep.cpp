// Copyright Epic Games, Inc. All Rights Reserved.

#include "Jeep.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "SandboxCharacter.h" // Assuming your character class is ASandboxCharacter
#include "GameFramework/PlayerController.h" // Required for APlayerController
#include "Components/InputComponent.h" // Required for UInputComponent

AJeep::AJeep()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
	// Ensure the mesh can simulate physics if you want physics-based movement later
	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetCollisionProfileName(TEXT("Vehicle")); // Use Vehicle collision profile

	ProximityTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ProximityTrigger"));
	ProximityTrigger->SetupAttachment(RootComponent);
	ProximityTrigger->SetCollisionProfileName(TEXT("Trigger"));

	bIsPlayerInJeep = false;
	PlayerInJeep = nullptr;
	JeepPlayerController = nullptr;
	CurrentThrottle = 0.0f;
	CurrentSteering = 0.0f;
}

void AJeep::BeginPlay()
{
	Super::BeginPlay();
	
}

void AJeep::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsPlayerInJeep && PlayerInJeep)
	{
		// Basic driving logic
		FVector ForwardForce = MeshComponent->GetForwardVector() * CurrentThrottle * 100000.0f * MeshComponent->GetMass(); // Adjust force multiplier as needed
		// Apply force at the center of mass, or a specific point if needed
		MeshComponent->AddForce(ForwardForce);

		FVector TorqueVector = FVector(0.0f, 0.0f, CurrentSteering * 50000.0f * MeshComponent->GetMass()); // Torque around Z axis for steering. Adjusted multiplier slightly.
		MeshComponent->AddTorqueInDegrees(TorqueVector);
	}
}

void AJeep::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	// Bind driving inputs
	PlayerInputComponent->BindAxis("MoveForward", this, &AJeep::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AJeep::TurnRight); // Assuming "MoveRight" is your steering axis
}

void AJeep::MoveForward(float Value)
{
	CurrentThrottle = Value;
}

void AJeep::TurnRight(float Value)
{
	CurrentSteering = Value;
}

void AJeep::Interact(APawn* PlayerPawn)
{
	if (bIsPlayerInJeep)
	{
		ExitJeep();
	}
	else
	{
		EnterJeep(PlayerPawn);
	}
}

void AJeep::EnterJeep(APawn* PlayerPawn)
{
	if (!PlayerPawn) return;

	ASandboxCharacter* PlayerCharacter = Cast<ASandboxCharacter>(PlayerPawn);
	if (!PlayerCharacter) return;

	JeepPlayerController = Cast<APlayerController>(PlayerCharacter->GetController());
	if (!JeepPlayerController) return;

	// Disable player's movement and collision
	PlayerCharacter->DisableInput(JeepPlayerController);
	PlayerCharacter->SetActorEnableCollision(false);

	// Attach player to jeep (you might want a specific socket on the jeep mesh)
	// For now, just hide the player and let them control the jeep.
	PlayerCharacter->SetActorHiddenInGame(true);
	// PlayerPawn->AttachToComponent(MeshComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, "DriverSeatSocket"); // Example socket

	bIsPlayerInJeep = true;
	PlayerInJeep = PlayerPawn;

	// Enable input for the Jeep actor itself
	EnableInput(JeepPlayerController);
	if (InputComponent) // Check if InputComponent is already created
	{
		SetupPlayerInputComponent(InputComponent);
	}
	else // If not, create it and then set it up.
	{
		InputComponent = NewObject<UInputComponent>(this);
		InputComponent->RegisterComponent();
		SetupPlayerInputComponent(InputComponent);
	}

	UE_LOG(LogTemp, Warning, TEXT("Player entered jeep. Jeep input enabled."));
}

void AJeep::ExitJeep()
{
	if (!PlayerInJeep || !JeepPlayerController) return;

	ASandboxCharacter* PlayerCharacter = Cast<ASandboxCharacter>(PlayerInJeep);
	if (!PlayerCharacter) return;

	// Disable input for the Jeep actor
	DisableInput(JeepPlayerController);

	// Detach player from jeep and make visible
	PlayerCharacter->SetActorHiddenInGame(false);
	// PlayerInJeep->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// Enable player's movement and collision
	PlayerCharacter->EnableInput(JeepPlayerController);
	PlayerCharacter->SetActorEnableCollision(true);

	// Teleport player to a safe exit location next to the jeep
	FVector ExitLocation = GetActorLocation() + GetActorRightVector() * 200.0f; // Example exit offset
	PlayerCharacter->SetActorLocation(ExitLocation);

	UE_LOG(LogTemp, Warning, TEXT("Player exited jeep. Player input restored."));
	bIsPlayerInJeep = false;
	PlayerInJeep = nullptr;
	JeepPlayerController = nullptr;
	CurrentThrottle = 0.0f;
	CurrentSteering = 0.0f;
} 