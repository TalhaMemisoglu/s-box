#pragma once

#include "CoreMinimal.h"
#include "Characters/Heroes/GSHeroCharacter.h"
#include "BPI_Interact.h"
#include "SandboxHeroCharacter.generated.h"

class APawn;
class APlayerController;

UCLASS(BlueprintType, Blueprintable)
class SANDBOX_API ASandboxHeroCharacter : public AGSHeroCharacter
{
	GENERATED_BODY()

public:
	ASandboxHeroCharacter(const class FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// Search radius for vehicles
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle Interaction")
	float VehicleSearchRadius;

	// Pointer to currently nearby vehicle pawn
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle Interaction")
	APawn* NearbyVehicle;

	// Blueprint path for Sedan class detection
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle Interaction", meta = (DisplayName = "Sedan Blueprint Path"))
	FString SedanBlueprintAssetPath;

	// Store the loaded UClass of the Sedan Blueprint
	UClass* LoadedSedanBlueprintClass;

	// Track if character is currently in a vehicle
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle Interaction")
	bool bIsInVehicle;

	// Store reference to the vehicle we're currently in
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle Interaction")
	APawn* CurrentVehicle;

	// Cooldown timer to prevent immediate re-entry after exiting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle Interaction")
	float VehicleInteractionCooldown;

	// Track when last interaction occurred
	float LastInteractionTime;

private:
	// Function to check for interactable vehicles in proximity
	void UpdateNearbyVehicle();

	// Handle F key press for vehicle interaction
	void OnVehicleInteract();

public:
	// Called by the vehicle when the player exits, to restore the character's state
	UFUNCTION(BlueprintCallable, Category = "Vehicle Interaction")
	void OnPlayerExitVehicle(const FTransform& ExitTransform);
}; 