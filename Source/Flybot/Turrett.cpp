// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.


#include "Turrett.h"

// Sets default values
ATurrett::ATurrett()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ATurrett::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATurrett::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

