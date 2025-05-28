// Copyright 2020 Dan Kestranek.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FallProtectionComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SANDBOX_API UFallProtectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UFallProtectionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bClipBelow;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
};
