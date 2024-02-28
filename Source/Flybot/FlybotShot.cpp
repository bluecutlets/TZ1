// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.

#include "FlybotShot.h"
#include "Flybot.h"
#include "FlybotPlayerPawn.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

AFlybotShot::AFlybotShot()
{
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->SetSphereRadius(25.f);
    Collision->OnComponentHit.AddDynamic(this, &AFlybotShot::OnHit);
    Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    FlySystemComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
    FlySystemComponent->SetupAttachment(Collision);

    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    Movement->InitialSpeed = 20000.f;
    Movement->MaxSpeed = 20000.f;
    Movement->ProjectileGravityScale = 0.f;

    InitialLifeSpan = 2.f;
    HealthDelta = -1.f;
}

void AFlybotShot::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
    FVector NormalImpulse, const FHitResult& Hit)
{
    UE_LOG(LogTemp, Log, TEXT("Shot hit %s"), *OtherActor->GetName());

    // Check if the hit actor is a player pawn
    AFlybotPlayerPawn* Target = Cast<AFlybotPlayerPawn>(OtherActor);
    if (Target)
    {
        Target->UpdateHealth(HealthDelta);
    }

    // Spawn hit effect
    if (HitSystem)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitSystem,
            Collision->GetComponentLocation(), Collision->GetComponentRotation());
    }

    Destroy();
}