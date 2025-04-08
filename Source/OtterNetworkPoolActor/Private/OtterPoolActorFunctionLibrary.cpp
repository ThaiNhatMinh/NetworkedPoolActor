// Fill out your copyright notice in the Description page of Project Settings.


#include "OtterPoolActorFunctionLibrary.h"

#include "OtterActorPoolWorldSubsystem.h"

UOtterPoolActorWorldSubsystem* UOtterPoolActorFunctionLibrary::Get(UObject* WorldContextObject)
{
	if (!WorldContextObject || !WorldContextObject->GetWorld())
		return nullptr;
	return WorldContextObject->GetWorld()->GetSubsystem<UOtterPoolActorWorldSubsystem>();
}

AActor* UOtterPoolActorFunctionLibrary::SpawnActorFromPool(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform, AActor* OwnerActor, APawn* Instigator)
{
	if (auto System = Get(WorldContextObject))
	{
		return System->SpawnActor(ActorClass, SpawnTransform, OwnerActor, Instigator);
	}
	return nullptr;
}

bool UOtterPoolActorFunctionLibrary::DestroyActorFromPool(AActor* ActorToDestroy)
{
	if (auto System = Get(ActorToDestroy))
	{
		return System->ReleaseToPool(ActorToDestroy);
	}
	return false;
}
