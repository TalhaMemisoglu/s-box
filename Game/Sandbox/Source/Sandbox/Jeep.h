// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Jeep.generated.h"

class UBoxComponent;
class USkeletalMeshComponent;
class UInputComponent;
class APlayerController;

UCLASS()
class SANDBOX_API AJeep : public AActor
{
	GENERATED_BODY()
	
public:	
	AJeep();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* ProximityTrigger;

	// Function to be called when the player interacts with the jeep
	void Interact(APawn* PlayerPawn);

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent);

	// Input handlers for driving
	void MoveForward(float Value);
	void TurnRight(float Value);

private:
	bool bIsPlayerInJeep;
	APawn* PlayerInJeep;
	APlayerController* JeepPlayerController;

	// Current driving input
	float CurrentThrottle;
	float CurrentSteering;

	// Functions to handle player entering and exiting the jeep
	void EnterJeep(APawn* PlayerPawn);
	void ExitJeep();

}; 