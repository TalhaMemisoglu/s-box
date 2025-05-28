#include "SandboxHeroCharacter.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"
#include "UObject/ConstructorHelpers.h"

ASandboxHeroCharacter::ASandboxHeroCharacter(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set default values
	VehicleSearchRadius = 300.0f;
	NearbyVehicle = nullptr;
	LoadedSedanBlueprintClass = nullptr;
	bIsInVehicle = false;
	CurrentVehicle = nullptr;
	VehicleInteractionCooldown = 2.0f; // 2 second cooldown
	LastInteractionTime = -10.0f; // Initialize to allow immediate first interaction

	// Set default sedan blueprint path - you may need to adjust this path
	SedanBlueprintAssetPath = TEXT("/Game/Blueprints/Vehicles/BP_Sedan.BP_Sedan_C");

	// Enable tick for vehicle detection
	PrimaryActorTick.bCanEverTick = true;
}

void ASandboxHeroCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Load the Sedan Blueprint class if path is provided
	if (!SedanBlueprintAssetPath.IsEmpty())
	{
		LoadedSedanBlueprintClass = StaticLoadClass(UObject::StaticClass(), nullptr, *SedanBlueprintAssetPath);
		if (!LoadedSedanBlueprintClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("ASandboxHeroCharacter::BeginPlay - Failed to load Sedan Blueprint from path: %s"), *SedanBlueprintAssetPath);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("ASandboxHeroCharacter::BeginPlay - Successfully loaded Sedan Blueprint: %s"), *LoadedSedanBlueprintClass->GetName());
		}
	}
}

void ASandboxHeroCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Only check for nearby vehicles if locally controlled and not in a vehicle
	if (IsLocallyControlled() && !bIsInVehicle)
	{
		UpdateNearbyVehicle();
	}
}

void ASandboxHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Bind the F key for vehicle interaction
	PlayerInputComponent->BindAction("VehicleInteract", IE_Pressed, this, &ASandboxHeroCharacter::OnVehicleInteract);
}

void ASandboxHeroCharacter::UpdateNearbyVehicle()
{
	if (!GetWorld())
	{
		NearbyVehicle = nullptr;
		return;
	}

	FVector Start = GetActorLocation();
	FCollisionShape Sphere = FCollisionShape::MakeSphere(VehicleSearchRadius);

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Vehicle);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic); // if vehicles are dynamic

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	bool bFoundAny = GetWorld()->OverlapMultiByObjectType(Overlaps, Start, FQuat::Identity, ObjParams, Sphere, QueryParams);

	APawn* ClosestVehicle = nullptr;
	float BestDistanceSquared = FLT_MAX;

	if (bFoundAny)
	{
		for (const FOverlapResult& Result : Overlaps)
		{
			APawn* CandidateVehicle = Cast<APawn>(Result.GetActor());
			if (CandidateVehicle && CandidateVehicle->GetClass()->ImplementsInterface(UBPI_Interact::StaticClass()))
			{
				// Check if it's a sedan if we have the loaded class
				bool bIsSedanOrGenericVehicle = true;
				if (LoadedSedanBlueprintClass)
				{
					bIsSedanOrGenericVehicle = CandidateVehicle->IsA(LoadedSedanBlueprintClass);
				}

				if (bIsSedanOrGenericVehicle)
				{
					float DistanceSquared = FVector::DistSquared(Start, CandidateVehicle->GetActorLocation());
					if (DistanceSquared < BestDistanceSquared)
					{
						BestDistanceSquared = DistanceSquared;
						ClosestVehicle = CandidateVehicle;
					}
				}
			}
		}
	}

	NearbyVehicle = ClosestVehicle;
}

void ASandboxHeroCharacter::OnVehicleInteract()
{
	UE_LOG(LogTemp, Log, TEXT("ASandboxHeroCharacter::OnVehicleInteract - F key pressed"));

	if (!NearbyVehicle)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASandboxHeroCharacter::OnVehicleInteract - No nearby vehicle to interact with"));
		return;
	}

	if (bIsInVehicle)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASandboxHeroCharacter::OnVehicleInteract - Already in a vehicle"));
		return;
	}

	// Check cooldown to prevent spam interactions
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastInteractionTime < VehicleInteractionCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASandboxHeroCharacter::OnVehicleInteract - Still in cooldown period, ignoring interaction"));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("ASandboxHeroCharacter::OnVehicleInteract - PlayerController is NULL"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ASandboxHeroCharacter::OnVehicleInteract - Attempting to interact with vehicle: %s"), *NearbyVehicle->GetName());

	// Check if it implements the interaction interface
	if (NearbyVehicle->GetClass()->ImplementsInterface(UBPI_Interact::StaticClass()))
	{
		// Call OnPlayerInteraction through the interface
		IBPI_Interact::Execute_OnPlayerInteraction(NearbyVehicle, this);
		
		// Mark as in vehicle and store reference
		bIsInVehicle = true;
		CurrentVehicle = NearbyVehicle;
		NearbyVehicle = nullptr; // Clear after interaction
		
		// Set the interaction time for cooldown
		LastInteractionTime = GetWorld()->GetTimeSeconds();

		UE_LOG(LogTemp, Log, TEXT("ASandboxHeroCharacter::OnVehicleInteract - Successfully interacted with vehicle"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ASandboxHeroCharacter::OnVehicleInteract - Vehicle does not implement BPI_Interact interface"));
	}
}

void ASandboxHeroCharacter::OnPlayerExitVehicle(const FTransform& ExitTransform)
{
	UE_LOG(LogTemp, Log, TEXT("ASandboxHeroCharacter::OnPlayerExitVehicle - Player is exiting vehicle"));

	// Teleport character to the exit spot
	SetActorTransform(ExitTransform, false, nullptr, ETeleportType::TeleportPhysics);

	// Re-enable visibility, collision, and tick
	SetActorHiddenInGame(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetActorEnableCollision(true);
	PrimaryActorTick.bCanEverTick = true;

	// Clear vehicle tracking
	bIsInVehicle = false;
	CurrentVehicle = nullptr;
	
	// Set the interaction time for cooldown
	LastInteractionTime = GetWorld()->GetTimeSeconds();

	// Re-enable input
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		EnableInput(PC);
		UE_LOG(LogTemp, Log, TEXT("ASandboxHeroCharacter::OnPlayerExitVehicle - Input re-enabled"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ASandboxHeroCharacter::OnPlayerExitVehicle - PlayerController is NULL"));
	}
} 