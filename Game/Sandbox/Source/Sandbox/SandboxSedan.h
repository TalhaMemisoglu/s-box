#pragma once

#include "CoreMinimal.h"
#include "WheeledVehicle.h"
#include "BPI_Interact.h"
#include "SandboxSedan.generated.h"

class ASandboxCharacter;
class USceneComponent;
class UCameraComponent;

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

	// Store the original player character
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	ASandboxCharacter* StoredPlayerCharacter;

	// Store the original player controller
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	APlayerController* StoredPlayerController;

	// Track if player is in the car
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle")
	bool bIsInCar;

	// Camera components for internal (FPS) and chase (TPS) views
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Camera")
	class UCameraComponent* InternalCameraComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Camera")
	class UCameraComponent* ChaseCameraComp;

	// Keeps track of which camera is currently active (true = internal/FPS)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
	bool bUseInternalCamera;

public:
	// Interface implementation
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnPlayerInteraction(APawn* InteractingPawn);
	virtual void OnPlayerInteraction_Implementation(APawn* InteractingPawn);

	// Handle exit input
	UFUNCTION()
	void OnExitVehicle();

	// Toggles between cameras when the SwitchCamera input action is pressed
	UFUNCTION()
	void ToggleCamera();
}; 