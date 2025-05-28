#pragma once

#include "CoreMinimal.h"
#include "WheeledVehicle.h"
#include "BPI_Interact.h"
#include "SandboxSedan.generated.h"

class ASandboxCharacter;
class USceneComponent;

UCLASS(BlueprintType, Blueprintable)
class SANDBOX_API ASandboxSedan : public AWheeledVehicle, public IBPI_Interact
{
	GENERATED_BODY()

public:
	ASandboxSedan();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;

	// Exit point for the player
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	USceneComponent* ExitPoint;

	// Store the original player pawn (supports any character BP)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	APawn* StoredPlayerCharacter;

	// Store the original player controller
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	APlayerController* StoredPlayerController;

	// Track if player is in the car
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	bool bIsInCar;

	// Cooldown timer to prevent immediate re-entry
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float ReEntryCooldownTime;

	// Track when last exit occurred
	float LastExitTime;

public:
	// Interface implementation
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnPlayerInteraction(APawn* InteractingPawn);
	virtual void OnPlayerInteraction_Implementation(APawn* InteractingPawn);

	// Handle exit input
	UFUNCTION()
	void RequestExitVehicle(); // Client calls this

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_OnExitVehicle(); // Server executes this
	void Server_OnExitVehicle_Implementation();
	bool Server_OnExitVehicle_Validate();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_UpdateCharacterVisualsOnEnter(APawn* CharacterPawn, bool bEnteringVehicle);
	void Multicast_UpdateCharacterVisualsOnEnter_Implementation(APawn* CharacterPawn, bool bEnteringVehicle);
}; 