// Copyright 2020 Dan Kestranek.


#include "RandomSpawner.h"


// Sets default values
ARandomSpawner::ARandomSpawner()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ARandomSpawner::BeginPlay()
{
	Super::BeginPlay();
	
    for(int32 i = 0; i < NumberOfSpawns; ++i) {
        float SpawnRadius = FMath::FRandRange(0.0f, Radius);
        float SpawnAngle = FMath::FRandRange(0.0f, 6.2830f);
        float X, Y;
        FMath::SinCos(&X, &Y, SpawnAngle);
        GetWorld()->SpawnActor<AActor>(SpawnClass, FVector(X, Y, 0.0f) * SpawnRadius, FRotator());
    }

    Destroy();
}

// Called every frame
void ARandomSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

