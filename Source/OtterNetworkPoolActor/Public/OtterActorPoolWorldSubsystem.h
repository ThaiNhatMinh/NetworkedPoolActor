// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "OtterActorPoolWorldSubsystem.generated.h"

class AReplicateProxyActor;

DECLARE_DELEGATE_OneParam(FOtterPoolPreBeginPlay, AActor*);

struct OTTERNETWORKPOOLACTOR_API FPoolActorSpawnParameters : public FActorSpawnParameters
{
	TSubclassOf<AActor> ActorClass;
	TSubclassOf<AActor> RootActorClass = AActor::StaticClass();

	FTransform Transform;
	bool bDisableCollisionOnSpawn = false;

	FOtterPoolPreBeginPlay PreBeginPlayDelegate;
};

USTRUCT()
struct FOtterPoolActorEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<AActor*> CacheActors;

	UPROPERTY()
	TSubclassOf<AActor> ActorClass;

	UPROPERTY()
	uint64 UsingBit = 0; // 64 bit, max 64 actor support
	uint64 CacheClientUsingBit = 0;
	uint8 NumActor;

	bool bStartWithTickEnable = false;

	bool IsFull() const;
	AActor* FindUnusedActor();
	AActor* SpawnActor(UWorld* InWorld, const FPoolActorSpawnParameters& SpawnParameter, bool bUsedNow = true);
	bool PushToPool(AActor* InActor);
	void SetSlot(int Index, bool bUsed);
	void OnActorEndPlay(AActor* InActor);
	void SetComponentTick(AActor* InActor, bool bEnable);

	void PreReplicatedRemove(const struct FOtterPoolActorArray& InArraySerializer) {};
	void PostReplicatedAdd(const struct FOtterPoolActorArray& InArraySerializer);
	void PostReplicatedChange(const struct FOtterPoolActorArray& InArraySerializer);

};

USTRUCT()
struct OTTERNETWORKPOOLACTOR_API FOtterPoolActorArray : public FFastArraySerializer
{
	GENERATED_BODY()
public:
	FOtterPoolActorArray() : Owner(nullptr) {}

	FOtterPoolActorArray(AReplicateProxyActor* InOwnerComponent)
		: Owner(InOwnerComponent)
	{
	}

	UPROPERTY()
	TArray<FOtterPoolActorEntry> Items;

	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize) {};
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FOtterPoolActorEntry, FOtterPoolActorArray>(Items, DeltaParms, *this );
	}
	//~End of FFastArraySerializer contract

	UPROPERTY(Transient)
	TWeakObjectPtr<AReplicateProxyActor> Owner;

	friend FOtterPoolActorEntry;
};

template<>
struct TStructOpsTypeTraits<FOtterPoolActorArray> : public TStructOpsTypeTraitsBase2<FOtterPoolActorArray>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

UCLASS()
class AReplicateProxyActor : public AInfo
{
	GENERATED_BODY()
public:
	AReplicateProxyActor();

	void EndPlay(EEndPlayReason::Type Reason);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const;

	AActor* SpawnActor(TSubclassOf<AActor> ActorClass,
		FTransform const& Transform,
		AActor* Owner = nullptr,
		APawn* Instigator = nullptr
	);

	AActor* SpawnActor(const FPoolActorSpawnParameters& SpawnParameter);

	bool ReleaseToPool(AActor* Actor);

protected:
	UPROPERTY(Replicated)
	FOtterPoolActorArray ActorPools;
};

/**
 * 
 */
UCLASS()
class OTTERNETWORKPOOLACTOR_API UOtterPoolActorWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld);
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const;
	virtual void Deinitialize();

	AActor* SpawnActor(TSubclassOf<AActor> ActorClass,
		FTransform const& Transform,
		AActor* Owner = nullptr,
		APawn* Instigator = nullptr
	);
	AActor* SpawnActor(const FPoolActorSpawnParameters& SpawnParameter);

	bool ReleaseToPool(AActor* Actor);

protected:
	UPROPERTY()
	AReplicateProxyActor* ReplicateActor;
};
