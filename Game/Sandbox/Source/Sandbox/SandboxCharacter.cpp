// Copyright Epic Games, Inc. All Rights Reserved.

#include "SandboxCharacter.h"
#include "SandboxProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h" // For debug sphere
#include "Jeep.h" 
#include "Engine/BlueprintGeneratedClass.h" // Required for UBlueprintGeneratedClass
#include "UObject/ConstructorHelpers.h" // Required for FClassFinder
// #include "BPI_Interact.h" // REMOVE - No longer using interface for Sedan
// ASedan_C is forward declared in the header, no direct include needed for the _C class usually

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// ASandboxCharacter

ASandboxCharacter::ASandboxCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Initialize interactable pointers
	NearbyJeep = nullptr;
	NearbyInteractablePawn = nullptr; // MODIFIED
	LoadedSedanBlueprintClass = nullptr; // MODIFIED

	// Allow character to tick
	PrimaryActorTick.bCanEverTick = true;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));

	// Create a gun mesh component
	FP_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Gun"));
	FP_Gun->SetOnlyOwnerSee(true);			// only the owning player will see this mesh
	FP_Gun->bCastDynamicShadow = false;
	FP_Gun->CastShadow = false;
	// FP_Gun->SetupAttachment(Mesh1P, TEXT("GripPoint"));
	FP_Gun->SetupAttachment(RootComponent);

	FP_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	FP_MuzzleLocation->SetupAttachment(FP_Gun);
	FP_MuzzleLocation->SetRelativeLocation(FVector(0.2f, 48.4f, -10.6f));

	// Default offset from the character location for projectiles to spawn
	GunOffset = FVector(100.0f, 0.0f, 10.0f);

	// Note: The ProjectileClass and FireSound variables are set in the editor Properties list -
	//       derived Blueprints from this class, in an Object Library Folder Assets in workflow

	// Create VR Controllers.
	R_MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("R_MotionController"));
	R_MotionController->MotionSource = FXRMotionControllerBase::RightHandSourceId;
	R_MotionController->SetupAttachment(RootComponent);
	L_MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("L_MotionController"));
	L_MotionController->SetupAttachment(RootComponent);

	// Create a gun mesh component for VR
	VR_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VR_Gun"));
	VR_Gun->SetOnlyOwnerSee(true);			// only the owning player will see this mesh
	VR_Gun->bCastDynamicShadow = false;
	VR_Gun->CastShadow = false;
	VR_Gun->SetupAttachment(R_MotionController);
	VR_Gun->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	VR_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("VR_MuzzleLocation"));
	VR_MuzzleLocation->SetupAttachment(VR_Gun);
	VR_MuzzleLocation->SetRelativeLocation(FVector(0.000000, 0.000000, -0.000000));
	VR_MuzzleLocation->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));		// Counteract the rotation of the VR gun model.

	// Uncomment the following line to turn motion controllers on by default:
	//bUsingMotionControllers = true;

	// Enable this actor to receive input from Player 0
	AutoReceiveInput = EAutoReceiveInput::Player0;

	// Default path to the Sedan Blueprint - THIS SHOULD BE SET IN THE BLUEPRINT EDITOR for any derived character BP
	SedanBlueprintAssetPath = TEXT("/Game/VehicleBP/Sedan/Sedan.Sedan_C"); // Example path, ADJUST IF YOURS IS DIFFERENT
}

void ASandboxCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	// Load the Sedan Blueprint Class
	if (!SedanBlueprintAssetPath.IsEmpty())
	{
		LoadedSedanBlueprintClass = StaticLoadClass(UObject::StaticClass(), nullptr, *SedanBlueprintAssetPath);
		if (LoadedSedanBlueprintClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("ASandboxCharacter::BeginPlay - Successfully loaded Sedan Blueprint Class: %s"), *LoadedSedanBlueprintClass->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ASandboxCharacter::BeginPlay - FAILED to load Sedan Blueprint Class from path: %s. Check path in Character Blueprint! Current path: %s"), *SedanBlueprintAssetPath, *SedanBlueprintAssetPath);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ASandboxCharacter::BeginPlay - SedanBlueprintAssetPath is EMPTY. Please set it in the Character Blueprint defaults."));
	}

	//Attach gun mesh component to Skeleton, doing it here because the skeleton is not yet created in the constructor
	FP_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("GripPoint"));

	// Show or hide the two versions of the gun based on whether or not we're using motion controllers.
	if (bUsingMotionControllers)
	{
		VR_Gun->SetHiddenInGame(false, true);
		Mesh1P->SetHiddenInGame(true, true);
	}
	else
	{
		VR_Gun->SetHiddenInGame(true, true);
		Mesh1P->SetHiddenInGame(false, true);
	}
}

void ASandboxCharacter::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);
	// UE_LOG(LogTemp, Warning, TEXT("ASandboxCharacter::Tick - Frame Update"));
	CheckForNearbySedan(); // MODIFIED: Call renamed function

    FVector Start = GetActorLocation();
    Start.Z = 20000.0f;
    FVector End = Start;
    End.Z = -20000.0f;

    FHitResult Result;

    if(GetWorld()->LineTraceSingleByChannel(Result, Start, End, ECC_PhysicsBody, FCollisionQueryParams())) {
        //DrawDebugLine(GetWorld(), Result.ImpactPoint, Result.ImpactPoint + Result.ImpactNormal * 50.0f, FColor::Green, false, 1, 0, 1);
        ATerrainMeshActor * TerrainMesh = Cast<ATerrainMeshActor>(Result.GetActor());
        if(TerrainMesh) {
            SetActorLocation(Result.Location);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// Input

void ASandboxCharacter::BeginSprint() {
	GetCharacterMovement()->MaxWalkSpeed = 1500.f;
}

void ASandboxCharacter::EndSprint() {
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
}

void ASandboxCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ASandboxCharacter::OnFire);

	// Enable touchscreen input
	EnableTouchscreenMovement(PlayerInputComponent);

	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &ASandboxCharacter::OnResetVR);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &ASandboxCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ASandboxCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ASandboxCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ASandboxCharacter::LookUpAtRate);

	// Bind sprint events
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &ASandboxCharacter::BeginSprint);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &ASandboxCharacter::EndSprint);

	// Bind interact event
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ASandboxCharacter::OnInteract);
}

void ASandboxCharacter::OnFire()
{
    if(GetLocalRole() == ROLE_Authority) {
        // try and fire a projectile
        if (ProjectileClass != nullptr)
        {
            UWorld* const World = GetWorld();
            if (World != nullptr)
            {
                if (bUsingMotionControllers)
                {
                    const FRotator SpawnRotation = VR_MuzzleLocation->GetComponentRotation();
                    const FVector SpawnLocation = VR_MuzzleLocation->GetComponentLocation();
                    World->SpawnActor<ASandboxProjectile>(ProjectileClass, SpawnLocation, SpawnRotation);
                }
                else
                {
                    const FRotator SpawnRotation = GetControlRotation();
                    // MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
                    const FVector SpawnLocation = ((FP_MuzzleLocation != nullptr) ? FP_MuzzleLocation->GetComponentLocation() : GetActorLocation()) + SpawnRotation.RotateVector(GunOffset);

                    //Set Spawn Collision Handling Override
                    FActorSpawnParameters ActorSpawnParams;
                    ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

                    // spawn the projectile at the muzzle
                    World->SpawnActor<ASandboxProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);
                }
            }
        }
    }
    else {
        const FRotator SpawnRotation = GetControlRotation();
        const FVector SpawnLocation = ((FP_MuzzleLocation != nullptr) ? FP_MuzzleLocation->GetComponentLocation() : GetActorLocation()) + SpawnRotation.RotateVector(GunOffset);
        FireFromClient(SpawnRotation, SpawnLocation);
    }

    // try and play the sound if specified
    if (FireSound != nullptr)
    {
        UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
    }

    // try and play a firing animation if specified
    if (FireAnimation != nullptr)
    {
        // Get the animation object for the arms mesh
        UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
        if (AnimInstance != nullptr)
        {
            AnimInstance->Montage_Play(FireAnimation, 1.f);
        }
    }
}

void ASandboxCharacter::FireFromClient_Implementation(FRotator Rotation, FVector Position)
{
    UWorld* const World = GetWorld();
    FActorSpawnParameters ActorSpawnParams;
    ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

    World->SpawnActor<ASandboxProjectile>(ProjectileClass, Position, Rotation, ActorSpawnParams);
}

void ASandboxCharacter::RequestMapUpdate_Implementation(ATerrainMeshActor * TerrainMesh)
{
    if(TerrainMesh) TerrainMesh->RequestMapUpdate();
}

void ASandboxCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void ASandboxCharacter::BeginTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (TouchItem.bIsPressed == true)
	{
		return;
	}
	if ((FingerIndex == TouchItem.FingerIndex) && (TouchItem.bMoved == false))
	{
		OnFire();
	}
	TouchItem.bIsPressed = true;
	TouchItem.FingerIndex = FingerIndex;
	TouchItem.Location = Location;
	TouchItem.bMoved = false;
}

void ASandboxCharacter::EndTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (TouchItem.bIsPressed == false)
	{
		return;
	}
	TouchItem.bIsPressed = false;
}

//Commenting this section out to be consistent with FPS BP template.
//This allows the user to turn without using the right virtual joystick

//void ASandboxCharacter::TouchUpdate(const ETouchIndex::Type FingerIndex, const FVector Location)
//{
//	if ((TouchItem.bIsPressed == true) && (TouchItem.FingerIndex == FingerIndex))
//	{
//		if (TouchItem.bIsPressed)
//		{
//			if (GetWorld() != nullptr)
//			{
//				UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
//				if (ViewportClient != nullptr)
//				{
//					FVector MoveDelta = Location - TouchItem.Location;
//					FVector2D ScreenSize;
//					ViewportClient->GetViewportSize(ScreenSize);
//					FVector2D ScaledDelta = FVector2D(MoveDelta.X, MoveDelta.Y) / ScreenSize;
//					if (FMath::Abs(ScaledDelta.X) >= 4.0 / ScreenSize.X)
//					{
//						TouchItem.bMoved = true;
//						float Value = ScaledDelta.X * BaseTurnRate;
//						AddControllerYawInput(Value);
//					}
//					if (FMath::Abs(ScaledDelta.Y) >= 4.0 / ScreenSize.Y)
//					{
//						TouchItem.bMoved = true;
//						float Value = ScaledDelta.Y * BaseLookUpRate;
//						AddControllerPitchInput(Value);
//					}
//					TouchItem.Location = Location;
//				}
//			}
//		}
//	}
//}

void ASandboxCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void ASandboxCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void ASandboxCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ASandboxCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

bool ASandboxCharacter::EnableTouchscreenMovement(class UInputComponent* PlayerInputComponent)
{
	if (FPlatformMisc::SupportsTouchInput() || GetDefault<UInputSettings>()->bUseMouseForTouch)
	{
		PlayerInputComponent->BindTouch(EInputEvent::IE_Pressed, this, &ASandboxCharacter::BeginTouch);
		PlayerInputComponent->BindTouch(EInputEvent::IE_Released, this, &ASandboxCharacter::EndTouch);

		//Commenting this out to be more consistent with FPS BP template.
		//PlayerInputComponent->BindTouch(EInputEvent::IE_Repeat, this, &ASandboxCharacter::TouchUpdate);
		return true;
	}
	
	return false;
}

// RENAMED and MODIFIED function
void ASandboxCharacter::CheckForNearbySedan() 
{
    if (!LoadedSedanBlueprintClass)
    {
        // UE_LOG(LogTemp, Verbose, TEXT("CheckForNearbySedan - LoadedSedanBlueprintClass is NULL."));
        NearbyInteractablePawn = nullptr;
        return; // Don't check if the class isn't loaded
    }

    FVector Start = GetActorLocation();
    //float InteractionRadius = 200.0f; // You can adjust this

    TArray<FOverlapResult> OverlapResults;
    FCollisionQueryParams CollisionParams;
    CollisionParams.AddIgnoredActor(this); // Ignore self
	FCollisionShape CollisionShape = FCollisionShape::MakeSphere(200.0f); // Radius of 200

    NearbyInteractablePawn = nullptr; // Reset before check
    // NearbyJeep = nullptr; // You might want to reset Jeep here too if not handled elsewhere

	// UE_LOG(LogTemp, Warning, TEXT("ASandboxCharacter::CheckForNearbySedan - Checking for overlaps."));

    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic); 
    ObjectQueryParams.AddObjectTypesToQuery(ECC_Vehicle); // Ensure your Sedan Blueprint is of Vehicle object type

    if (GetWorld()->OverlapMultiByObjectType(
        OverlapResults,
        Start,
        FQuat::Identity,
        ObjectQueryParams,
        CollisionShape,
        CollisionParams))
    {
        // UE_LOG(LogTemp, Warning, TEXT("ASandboxCharacter::CheckForNearbySedan - Found %d overlapping actors."), OverlapResults.Num());
        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* OverlappedActor = Result.GetActor();
            if (OverlappedActor && OverlappedActor->IsA(LoadedSedanBlueprintClass))
            {
                // UE_LOG(LogTemp, Warning, TEXT("ASandboxCharacter::CheckForNearbySedan - Overlapped with: %s"), *OverlappedActor->GetName());
                
                NearbyInteractablePawn = Cast<APawn>(OverlappedActor); // Cast to APawn for Possess
                if (NearbyInteractablePawn)
                {
                    // UE_LOG(LogTemp, Warning, TEXT("ASandboxCharacter::CheckForNearbySedan - Found and set NearbyInteractablePawn: %s"), *NearbyInteractablePawn->GetName());
                    return; // Found our sedan
                }

                // If not a sedan, check if it's a Jeep (if you still have Jeep logic)
                if (!NearbyInteractablePawn) { // Only check for Jeep if Sedan wasn't found yet in this loop iteration
                    AJeep* JeepCandidate = Cast<AJeep>(OverlappedActor);
                    if (JeepCandidate)
                    {
                        NearbyJeep = JeepCandidate;
                        // UE_LOG(LogTemp, Warning, TEXT("ASandboxCharacter::CheckForInteractables - Found and set NearbyJeep: %s"), *NearbyJeep->GetName());
                        // Potentially return here if Jeep is prioritized or you only want one interactable at a time
                    }
                }
            }
        }
    }
     // else
    // {
    //     // No overlaps, ensure pointers are null if nothing is found by the end of the function
    //      NearbySedan = nullptr; // Already reset at the beginning
    //      NearbyJeep = nullptr; // Reset if it wasn't found
    // }
}

void ASandboxCharacter::OnInteract()
{
    // UE_LOG(LogTemp, Warning, TEXT("ASandboxCharacter::OnInteract - 'E' key pressed."));

    if (NearbyInteractablePawn)
    {
        APlayerController* PC = Cast<APlayerController>(GetController());
        if (PC)
        {
            UE_LOG(LogTemp, Warning, TEXT("ASandboxCharacter::OnInteract - Attempting to possess NearbyInteractablePawn: %s of class %s"), 
                *NearbyInteractablePawn->GetName(), 
                *NearbyInteractablePawn->GetClass()->GetName());

            // Attempt to set the OriginalDriver variable on the Sedan Blueprint instance
            if (LoadedSedanBlueprintClass && NearbyInteractablePawn->IsA(LoadedSedanBlueprintClass))
            {
                // This requires a direct include of the Sedan's generated header if you want to access its specific UPROPERTIES directly.
                // For robust interaction without needing to recompile C++ if Sedan BP changes, 
                // it's often better to use a BlueprintCallable function on the Sedan to set this.
                // However, for a direct variable set like this IF the variable exists:
                
                // Try to find the UProperty by name and set it (more complex, reflection based)
                // Simpler if we know the exact C++ class of the Blueprint (ASedan_C if BP is Sedan)
                // For now, this part is tricky without direct access to ASedan_C specific members.
                // The Sedan Blueprint will need to get the player character reference itself via GetPlayerCharacter(0) after its OnPossessed event.
                UE_LOG(LogTemp, Warning, TEXT("ASandboxCharacter::OnInteract - Sedan recognized. Sedan BP should store OriginalDriver on its own OnPossessed."));
            }
            
            SetActorHiddenInGame(true);
            GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            SetActorTickEnabled(false);
            DisableInput(PC);

            PC->Possess(NearbyInteractablePawn);
            
            NearbyInteractablePawn = nullptr; // Clear after possessing
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ASandboxCharacter::OnInteract - PlayerController is NULL!"));
        }
    }
    else if (NearbyJeep) // Handle Jeep interaction if no Sedan is targeted
    {
        // UE_LOG(LogTemp, Warning, TEXT("ASandboxCharacter::OnInteract - Interacting with Jeep: %s"), *NearbyJeep->GetName());
        NearbyJeep->Interact(this); // Assuming Jeep has its own Interact() method
    }
    // else
    // {
    //     UE_LOG(LogTemp, Warning, TEXT("ASandboxCharacter::OnInteract - No NearbySedan or NearbyJeep to interact with."));
    // }
}

void ASandboxCharacter::OnPlayerExitVehicle(const FTransform& ExitTransform)
{
	UE_LOG(LogTemp, Warning, TEXT("ASandboxCharacter::OnPlayerExitVehicle - Player is exiting vehicle. Teleporting to exit location and re-enabling."));

	// Teleport character to the exit spot
	SetActorTransform(ExitTransform, false, nullptr, ETeleportType::TeleportPhysics);

	// Re-enable visibility, collision, and tick (if they were disabled)
	SetActorHiddenInGame(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetActorTickEnabled(true);

	// Make sure the controller is set correctly on the character if it isn't already handled by Possess
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && PC->GetPawn() != this)
	{
		// This might be redundant if the Possess call in the Sedan BP handles it all correctly,
		// but as a safeguard:
		// PC->Possess(this); // This can cause issues if called at the wrong time. The Sedan should handle possessing this character.
		UE_LOG(LogTemp, Warning, TEXT("ASandboxCharacter::OnPlayerExitVehicle - Controller is possessing: %s. Character is: %s"), 
			PC->GetPawn() ? *PC->GetPawn()->GetName() : TEXT("NULL"), 
			*GetName());
	}
	else if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("ASandboxCharacter::OnPlayerExitVehicle - PlayerController is NULL after attempting to exit vehicle!"));
	}

	// Re-enable input for this character if it was disabled
	EnableInput(UGameplayStatics::GetPlayerController(this, 0));
}
