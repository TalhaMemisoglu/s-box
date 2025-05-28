#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleInteractComponent.generated.h"

class APawn;
class APlayerController;
class IBPI_Interact;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SANDBOX_API UVehicleInteractComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleInteractComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// Search radius for vehicles
	UPROPERTY(EditAnywhere, Category="Interaction")
	float SearchRadius;

	// Pointer to currently nearby vehicle pawn
	UPROPERTY()
	APawn* NearbyVehicle;

	// Checks sphere around owner to find nearest vehicle pawn implementing interact interface
	void UpdateNearbyVehicle();

	// Returns true if local player pressed interact key this frame
	bool WasInteractKeyJustPressed() const;

	// Client-side key press handler -> calls server RPC
	void HandleLocalInteract();

	UFUNCTION(Server, Reliable)
	void ServerInteract(APawn* TargetVehicle);
}; 