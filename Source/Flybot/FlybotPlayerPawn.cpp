// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.

#include "FlybotPlayerPawn.h"
#include "Flybot.h"
#include "FlybotPlayerController.h"
#include "FlybotPlayerHUD.h"
#include "FlybotShot.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"

AFlybotPlayerPawn::AFlybotPlayerPawn()
{
	Collision = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->SetVisibleFlag(false);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Body);

	// Springarm and Camera
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Collision);
	SpringArm->SetRelativeLocation(FVector(120.f, 0.f, 50.f));
	SpringArm->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
	SpringArm->TargetArmLength = 600.f;

	SpringArmLengthScale = 2000.f;
	SpringArmLengthMin = 0.f;
	SpringArmLengthMax = 1000.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->SetRelativeRotation(FRotator(15.f, 0.f, 0.f));
	Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
	Camera->PostProcessSettings.MotionBlurAmount = 0.1f;

	// Movement
	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->MaxSpeed = 5000.f;
	Movement->Acceleration = 5000.f;
	Movement->Deceleration = 10000.f;

	MoveScale = 1.f;
	RotateScale = 50.f;
	bFreeFly = false;
	SpeedCheckInterval = 0.5f;
	SpeedCheckTranslationSum = FVector::ZeroVector;
	SpeedCheckTranslationCount = 0;
	SpeedCheckLastTranslation = FVector::ZeroVector;
	SpeedCheckLastTime = 0.f;
	MaxMovesWithHits = 30;
	MovesWithHits = 0;

	// Pawn Animation
	ZMovementFrequency = 2.f;
	ZMovementAmplitude = 5.f;
	ZMovementOffset = 0.f;

	TiltInput = 0.f;
	TiltMax = 15.f;
	TiltMoveScale = 0.6f;
	TiltRotateScale = 0.4f;
	TiltResetScale = 0.3f;

	// Shooting
	bShooting = false;
	ShootingInterval = 0.2f;
	ShootingOffset = FVector(300.f, 0.f, 0.f);
	ShotClass = AFlybotShot::StaticClass();
	ShootingLastTime = 0.f;

	// HUD
	PlayerHUDClass = nullptr;
	PlayerHUD = nullptr;

	// Health
	MaxHealth = 25.f;
	Health = MaxHealth;

	// Power
	MaxPower = 25.f;
	Power = MaxPower;
	PowerRegenerateRate = 1.f;

	// Allow ticking for the pawn.
	PrimaryActorTick.bCanEverTick = true;

	// We should match this to FlybotShot (life span * speed) so they are destroyed at the same distance.
	NetCullDistanceSquared = 1600000000.f;

	// Force pawn to always spawn, even if it is colliding with another object.
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
}

void AFlybotPlayerPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFlybotPlayerPawn, bShooting, COND_SimulatedOnly);
	DOREPLIFETIME_CONDITION(AFlybotPlayerPawn, Health, COND_OwnerOnly);
}

void AFlybotPlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAction("Move", IE_Pressed, this, &AFlybotPlayerPawn::Move);
    PlayerInputComponent->BindAction("Rotate", IE_Pressed, this, &AFlybotPlayerPawn::Rotate);
    PlayerInputComponent->BindAction("FreeFly", IE_Pressed, this, &AFlybotPlayerPawn::ToggleFreeFly);
    PlayerInputComponent->BindAction("SpringArmLength", IE_Pressed, this, &AFlybotPlayerPawn::UpdateSpringArmLength);
    PlayerInputComponent->BindAction("Shoot", IE_Pressed, this, &AFlybotPlayerPawn::Shoot);
    PlayerInputComponent->BindAction("Shoot", IE_Released, this, &AFlybotPlayerPawn::Shoot);
}

void AFlybotPlayerPawn::BeginPlay()
{
    Super::BeginPlay();

    if (PlayerHUDClass)
    {
        PlayerHUD = CreateWidget<UFlybotPlayerHUD>(GetController(), PlayerHUDClass);
        if (PlayerHUD)
        {
            PlayerHUD->AddToViewport();
            PlayerHUD->SetHealth(Health, MaxHealth);
            PlayerHUD->SetPower(Power, MaxPower);
        }
    }
}

void AFlybotPlayerPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (PlayerHUD)
    {
        PlayerHUD->RemoveFromViewport();
        PlayerHUD = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void AFlybotPlayerPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    RegeneratePower();
    TryShooting();
    UpdatePawnAnimation();
}

void AFlybotPlayerPawn::UpdateSpringArmLength(const FInputActionValue& ActionValue)
{
    SpringArm->TargetArmLength += ActionValue * GetWorld()->GetDeltaSeconds() * SpringArmLengthScale;
    SpringArm->TargetArmLength = FMath::Clamp(SpringArm->TargetArmLength, SpringArmLengthMin, SpringArmLengthMax);
}

void AFlybotPlayerPawn::Move(FVector Input)
{
    AddMovementInput(GetActorRotation().RotateVector(Input), MoveScale);
    TiltInput += Input.Y * TiltMoveScale * MoveScale;
}

void AFlybotPlayerPawn::Rotate(FRotator Input)
{
    Input *= GetWorld()->GetDeltaSeconds() * RotateScale;
    TiltInput += Input.Yaw * TiltRotateScale;

    if (!bFreeFly) {
        Input += GetActorRotation();
        Input.Pitch = FMath::ClampAngle(Input.Pitch, -89.9f, 89.9f);
        SetActorRotation(Input);
    }
    else {
        AddActorLocalRotation(Input);
    }
}



void AFlybotPlayerPawn::ToggleFreeFly()
{
    bFreeFly = !bFreeFly;
}


void AFlybotPlayerPawn::Shoot(bool bIsShooting)
{
    bShooting = bIsShooting;
}



void AFlybotPlayerPawn::TryShooting()
{
    float Now = GetWorld()->GetRealTimeSeconds();
    float PowerDelta = Cast<AFlybotShot>(ShotClass->GetDefaultObject())->PowerDelta;

    if (!bShooting || Now - ShootingLastTime < ShootingInterval || Power + PowerDelta <= 0)
    {
        return;
    }

    FRotator ShotRotation = Body->GetComponentRotation();
    FVector ShotStart = Body->GetComponentLocation() + ShotRotation.RotateVector(ShootingOffset);
    AFlybotShot* Shot = GetWorld()->SpawnActor<AFlybotShot>(ShotClass, ShotStart, ShotRotation);
    if (Shot)
    {
        ShootingLastTime = Now;
        Shot->SetInstigator(this);

        Power += PowerDelta;
        if (PlayerHUD)
        {
            PlayerHUD->SetPower(Power, MaxPower);
        }
    }
}

void AFlybotPlayerPawn::UpdateHealth(float HealthDelta)
{
    Health = FMath::Clamp(Health + HealthDelta, 0.f, MaxHealth);

    if (Health == 0.f)
    {
        // Handle player elimination.
    }
}

void AFlybotPlayerPawn::RegeneratePower()
{
    Power = FMath::Clamp(Power + (PowerRegenerateRate * GetWorld()->GetDeltaSeconds()), 0.f, MaxPower);
    if (PlayerHUD)
    {
        PlayerHUD->SetPower(Power, MaxPower);
    }
}