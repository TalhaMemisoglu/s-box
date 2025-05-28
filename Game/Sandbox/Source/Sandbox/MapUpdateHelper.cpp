// Copyright 2020 Dan Kestranek.


#include "MapUpdateHelper.h"


// Sets default values for this component's properties
UMapUpdateHelper::UMapUpdateHelper()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UMapUpdateHelper::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UMapUpdateHelper::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UMapUpdateHelper::RequestMapUpdate_Implementation(ATerrainMeshActor * TerrainMesh)
{
    if(TerrainMesh) TerrainMesh->RequestMapUpdate();
}
