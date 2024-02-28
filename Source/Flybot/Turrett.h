// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Turrett.generated.h"

UCLASS()
class FLYBOT_API ATurrett : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATurrett();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
