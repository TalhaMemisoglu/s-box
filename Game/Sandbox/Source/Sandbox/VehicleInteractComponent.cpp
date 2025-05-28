#include "VehicleInteractComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "BPI_Interact.h"
#include "GameFramework/PlayerController.h"
#include "Engine/EngineTypes.h"

UVehicleInteractComponent::UVehicleInteractComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SearchRadius = 300.f;
	NearbyVehicle = nullptr;
	SetIsReplicatedByDefault(false); // purely client-side detection; RPC used for interaction
}

void UVehicleInteractComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UVehicleInteractComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return; // run only on owning client for input detection
	}

	UpdateNearbyVehicle();

	if (WasInteractKeyJustPressed())
	{
		HandleLocalInteract();
	}
}

void UVehicleInteractComponent::UpdateNearbyVehicle()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		NearbyVehicle = nullptr;
		return;
	}

	FVector Start = OwnerPawn->GetActorLocation();
	FCollisionShape Sphere = FCollisionShape::MakeSphere(SearchRadius);

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Vehicle);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic); // if vehicles are dynamic

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerPawn);

	bool bAny = GetWorld()->OverlapMultiByObjectType(Overlaps, Start, FQuat::Identity, ObjParams, Sphere, QueryParams);

	APawn* Closest = nullptr;
	float BestDistSq = FLT_MAX;

	if (bAny)
	{
		for (const FOverlapResult& Res : Overlaps)
		{
			APawn* Candidate = Cast<APawn>(Res.GetActor());
			if (Candidate && Candidate->GetClass()->ImplementsInterface(UBPI_Interact::StaticClass()))
			{
				float DistSq = FVector::DistSquared(Start, Candidate->GetActorLocation());
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					Closest = Candidate;
				}
			}
		}
	}

	NearbyVehicle = Closest;
}

bool UVehicleInteractComponent::WasInteractKeyJustPressed() const
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return false;

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC) return false;

	return PC->WasInputKeyJustPressed(EKeys::E);
}

void UVehicleInteractComponent::HandleLocalInteract()
{
	if (!NearbyVehicle) return;

	ServerInteract(NearbyVehicle);
}

void UVehicleInteractComponent::ServerInteract_Implementation(APawn* TargetVehicle)
{
	if (!TargetVehicle) return;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;

	if (TargetVehicle->GetClass()->ImplementsInterface(UBPI_Interact::StaticClass()))
	{
		IBPI_Interact::Execute_OnPlayerInteraction(TargetVehicle, OwnerPawn);
	}
} 