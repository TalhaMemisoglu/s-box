// Copyright 2020 Dan Kestranek.


#include "FallProtectionComponent.h"
#include "TerrainMeshActor.h"
#include "DrawDebugHelpers.h"


// Sets default values for this component's properties
UFallProtectionComponent::UFallProtectionComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UFallProtectionComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UFallProtectionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AActor * OwnerActor = GetOwner();
    if(OwnerActor) {
        FVector Pos = OwnerActor->GetActorLocation();
        FVector Start = Pos;
        Start.Z = 20000.0f;
        FVector End = Start;
        End.Z = -20000.0f;

        FHitResult Result;

        if(GetWorld()->LineTraceSingleByChannel(Result, Start, End, ECC_PhysicsBody, FCollisionQueryParams())) {
            //DrawDebugLine(GetWorld(), Result.ImpactPoint, Result.ImpactPoint + Result.ImpactNormal * 500.0f, FColor::Green, false, 1, 0, 1);
            ATerrainMeshActor * TerrainMesh = Cast<ATerrainMeshActor>(Result.GetActor());
            if(TerrainMesh && Result.ImpactPoint.Z > Pos.Z) {
                UMeshComponent * MeshComponent = OwnerActor->FindComponentByClass<UMeshComponent>();
                if(MeshComponent && MeshComponent->IsSimulatingPhysics()) {
                    //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::White, FString::Printf(TEXT("%f"), Pos.Z));
                    MeshComponent->SetAllPhysicsPosition(Result.ImpactPoint);
                }
                else {
                    OwnerActor->SetActorLocation(Result.ImpactPoint);
                }
            }
        }
    }

	// ...
}

