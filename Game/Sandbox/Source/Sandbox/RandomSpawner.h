// Copyright 2020 Dan Kestranek.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RandomSpawner.generated.h"

UCLASS()
class SANDBOX_API ARandomSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ARandomSpawner();

	UPROPERTY(EditInstanceOnly)
	TSubclassOf<class AActor> SpawnClass;

	UPROPERTY(EditInstanceOnly)
	float Radius;

	UPROPERTY(EditInstanceOnly)
	int32 NumberOfSpawns;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	
	
};
