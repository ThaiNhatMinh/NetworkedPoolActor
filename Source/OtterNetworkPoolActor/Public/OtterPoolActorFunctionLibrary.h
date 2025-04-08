// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OtterPoolActorFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class OTTERNETWORKPOOLACTOR_API UOtterPoolActorFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static class UOtterPoolActorWorldSubsystem* Get(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category="Otter|Pool", meta=(WorldContext = "WorldContextObject"))
	static AActor* SpawnActorFromPool(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform, AActor* OwnerActor, APawn* Instigator);
	UFUNCTION(BlueprintCallable, Category="Otter|Pool", meta=(WorldContext = "WorldContextObject"))
	static bool DestroyActorFromPool(AActor* ActorToDestroy);
};
