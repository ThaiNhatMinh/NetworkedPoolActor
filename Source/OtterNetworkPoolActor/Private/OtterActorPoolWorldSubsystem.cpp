// Fill out your copyright notice in the Description page of Project Settings.


#include "OtterActorPoolWorldSubsystem.h"
#include "Stats/StatsMisc.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameModeBase.h"
#include "OtterPoolActorInterface.h"
#include <bitset>

constexpr uint8 MAX_ELEMENT = sizeof(uint64) * 8;

void AReplicateProxyActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, ActorPools);
}

bool UOtterPoolActorWorldSubsystem::ShouldCreateSubsystem(UObject * Outer) const
{
	return Super::ShouldCreateSubsystem(Outer);
}

void UOtterPoolActorWorldSubsystem::OnWorldBeginPlay(UWorld & InWorld)
{
	auto GameMode = InWorld.GetAuthGameMode();
	if (!IsValid(GameMode))
		return;
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save game states or network managers into a map		
	ReplicateActor = InWorld.SpawnActor<AReplicateProxyActor>(SpawnInfo);
	ReplicateActor->bAlwaysRelevant = true;
}

void UOtterPoolActorWorldSubsystem::Deinitialize()
{
	Super::Deinitialize();
	if (IsValid(ReplicateActor))
		ReplicateActor->Destroy();
}


bool UOtterPoolActorWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

AActor * UOtterPoolActorWorldSubsystem::SpawnActor(TSubclassOf<AActor> ActorClass, FTransform const & Transform, AActor * Owner, APawn * Instigator)
{
	if (!IsValid(ReplicateActor))
		return nullptr;
	return ReplicateActor->SpawnActor(ActorClass, Transform, Owner, Instigator);
}

AActor* UOtterPoolActorWorldSubsystem::SpawnActor(const FPoolActorSpawnParameters& SpawnParameter)
{
	if (!IsValid(ReplicateActor))
		return nullptr;
	return ReplicateActor->SpawnActor(SpawnParameter);
}

bool UOtterPoolActorWorldSubsystem::ReleaseToPool(AActor* Actor)
{
	if (!IsValid(ReplicateActor))
		return false;
	return ReplicateActor->ReleaseToPool(Actor);
}

AReplicateProxyActor::AReplicateProxyActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10);
}

void AReplicateProxyActor::EndPlay(EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
	if (!HasAuthority())
		return;

	for (auto& ActorEntry : ActorPools.Items)
	{
		for (auto CacheActor : ActorEntry.CacheActors)
		{
			if (!ensure(IsValid(CacheActor)))
				continue;
			CacheActor->Destroy();
		}
	}
	ActorPools.Items.Empty();
}

AActor* AReplicateProxyActor::SpawnActor(const FPoolActorSpawnParameters& SpawnParameter)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AReplicateProxyActor::SpawnActor);
	if (!SpawnParameter.ActorClass)
		return nullptr;
	AActor* Found = nullptr;
	FOtterPoolActorEntry* FoundEntry = nullptr;
	for (auto& ActorEntry : ActorPools.Items)
	{
		if (ActorEntry.ActorClass == SpawnParameter.ActorClass)
		{
			// Find unused actor
			Found = ActorEntry.FindUnusedActor();
			if (!IsValid(Found))
			{
				if (ActorEntry.IsFull())
					continue;
				Found = ActorEntry.SpawnActor(GetWorld(), SpawnParameter);
				if (IsValid(Found))
					return Found;
			}
			FoundEntry = &ActorEntry;
			ActorPools.MarkItemDirty(ActorEntry);
			break;
		}
	}

	if (!IsValid(Found))
	{
		FOtterPoolActorEntry& Entry = ActorPools.Items.AddDefaulted_GetRef();
		Entry.UsingBit = 0;
		Entry.CacheClientUsingBit = 0;
		Entry.ActorClass = SpawnParameter.ActorClass;
		Entry.bStartWithTickEnable = SpawnParameter.ActorClass.GetDefaultObject()->PrimaryActorTick.bStartWithTickEnabled;
		Found = Entry.SpawnActor(GetWorld(), SpawnParameter, true);
		if (!IsValid(Found))
			return nullptr;
		ActorPools.MarkItemDirty(Entry);
	}
	else
	{
		Found->SetActorTransform(SpawnParameter.Transform, false, nullptr);
		Found->SetInstigator(SpawnParameter.Instigator);
		Found->SetOwner(SpawnParameter.Owner);
		if (!Found->GetRootComponent()->GetIsReplicated() && !Found->IsReplicatingMovement())
		{
			Found->SetReplicateMovement(true);
		}
		if (FoundEntry)
		{
			Found->SetActorTickEnabled(FoundEntry->bStartWithTickEnable);
		}
		Found->SetActorHiddenInGame(false);
		if (!SpawnParameter.bDisableCollisionOnSpawn)
			Found->SetActorEnableCollision(true);
		Found->SetNetDormancy(ENetDormancy::DORM_Awake);
		Found->InitializeComponents();
		SpawnParameter.PreBeginPlayDelegate.ExecuteIfBound(Found);
		Found->DispatchBeginPlay();
		if (FoundEntry)
		{
			FoundEntry->SetComponentTick(Found, true);
		}
		Found->ForceNetUpdate();
	}
	return Found;
}

AActor * AReplicateProxyActor::SpawnActor(TSubclassOf<AActor> ActorClass, FTransform const & Transform, AActor * OwnerActor, APawn * InstigatorActor)
{
	FPoolActorSpawnParameters Parameters;
	Parameters.ActorClass = ActorClass;
	Parameters.Transform = Transform;
	Parameters.Owner = OwnerActor;
	Parameters.Instigator = InstigatorActor;
	return SpawnActor(Parameters);
}

bool AReplicateProxyActor::ReleaseToPool(AActor* Actor)
{
	if (!IsValid(Actor) || !Actor->HasAuthority())
		return false;

	for (auto& ActorEntry : ActorPools.Items)
	{
		if (ActorEntry.ActorClass != Actor->GetClass())
			continue;
		if (!ActorEntry.PushToPool(Actor))
			continue;
		ActorPools.MarkItemDirty(ActorEntry);
		return true;
	}
	Actor->Destroy();
	return true;
}

bool FOtterPoolActorEntry::IsFull() const
{
	return CacheActors.Num() >= MAX_ELEMENT;
}

AActor* FOtterPoolActorEntry::FindUnusedActor()
{
	if (CacheActors.IsEmpty())
		return nullptr;

	std::bitset<MAX_ELEMENT> bitset(UsingBit);
	if (bitset.all())
		return nullptr;
	uint8 Index = 0;
	while (bitset.test(Index) && Index < MAX_ELEMENT)
		Index++;
	if (Index >= MAX_ELEMENT || !CacheActors.IsValidIndex(Index))
		return nullptr;

	bitset.set(Index, true);
	UsingBit = bitset.to_ullong();

#if !UE_BUILD_SHIPPING
	if (!IsValid(CacheActors[Index]))
	{
		UE_LOG(LogTemp, Error, TEXT("Pool: Index=%d has invalid actor"), Index);
	}
#endif
	return CacheActors[Index];
}

AActor* FOtterPoolActorEntry::SpawnActor(UWorld* InWorld, const FPoolActorSpawnParameters& SpawnParameter, bool bUsedNow)
{
	UE_LOG(LogTemp, Verbose, TEXT("Pool: SpawnActor: count %d actor for class %s"), CacheActors.Num(), *GetNameSafe(SpawnParameter.ActorClass));
	if (CacheActors.Num() >= MAX_ELEMENT)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Pool: Reach max %d actor for class %s"), MAX_ELEMENT, *GetNameSafe(SpawnParameter.ActorClass));
		return nullptr;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(OtterSkill::SpawnNewActorInPool);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = SpawnParameter.Owner;
	SpawnParameters.Instigator = SpawnParameter.Instigator;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.bDeferConstruction = true;
	auto Found = InWorld->SpawnActor<AActor>(SpawnParameter.ActorClass, SpawnParameter.Transform, SpawnParameters);

	if (!IsValid(Found))
		return nullptr;
	if (SpawnParameter.bDisableCollisionOnSpawn)
		Found->SetActorEnableCollision(false);
	SpawnParameter.PreBeginPlayDelegate.ExecuteIfBound(Found);
	Found->FinishSpawning(SpawnParameter.Transform);
	if (auto PoolInterface = Cast<IOtterPoolActorInterface>(Found))
	{
		PoolInterface->SetEnable(true);
		if (PoolInterface->ShouldCollectProperty())
			PoolInterface->CollectProperty(Found, SpawnParameter.RootActorClass);
	}
	CacheActors.Add(Found);
	if (bUsedNow)
	{
		SetSlot(CacheActors.Num() - 1, true);
	}
	return Found;
}

void FOtterPoolActorEntry::SetSlot(int Index, bool bUsed)
{
	std::bitset<MAX_ELEMENT> bitset(UsingBit);
	bitset.set(Index, bUsed);
	UsingBit = bitset.to_ullong();
}

bool FOtterPoolActorEntry::PushToPool(AActor* InActor)
{
	for (auto Index = 0; Index < CacheActors.Num(); Index++)
	{
		if (CacheActors[Index] == InActor)
		{
			SetSlot(Index, false);
			OnActorEndPlay(InActor);
			InActor->SetNetDormancy(ENetDormancy::DORM_DormantAll);
			return true;
		}
	}

	return false;
}

void FOtterPoolActorEntry::OnActorEndPlay(AActor* InActor)
{
	InActor->RouteEndPlay(EEndPlayReason::Destroyed);
	InActor->SetActorEnableCollision(false);
	InActor->SetActorHiddenInGame(true);
	InActor->SetActorTickEnabled(false);
	SetComponentTick(InActor, false);
#if !UE_BUILD_SHIPPING
	InActor->MarkComponentsRenderStateDirty();
#endif
	InActor->GetWorldTimerManager().ClearAllTimersForObject(InActor);
	if (auto PoolInterface = Cast<IOtterPoolActorInterface>(InActor))
	{
		PoolInterface->ResetProperty(InActor);
	}
}

void FOtterPoolActorEntry::SetComponentTick(AActor* InActor, bool bEnable)
{
	TInlineComponentArray<UActorComponent*> Components;
	InActor->GetComponents(Components);

	for(int32 CompIdx=0; CompIdx<Components.Num(); CompIdx++)
	{
		if (bEnable)
		{
			if (Components[CompIdx]->PrimaryComponentTick.bStartWithTickEnabled)
				Components[CompIdx]->SetComponentTickEnabled(true);
		}
		else
		{
			Components[CompIdx]->SetComponentTickEnabled(false);
		}
	}
}

void FOtterPoolActorEntry::PostReplicatedChange(const struct FOtterPoolActorArray& InArraySerializer)
{
	if (UsingBit != CacheClientUsingBit)
	{
		std::bitset<MAX_ELEMENT> newbitset(UsingBit);
		std::bitset<MAX_ELEMENT> oldbitset(CacheClientUsingBit);
		std::bitset<MAX_ELEMENT> changedbitset = newbitset ^ oldbitset;
		if (!changedbitset.any())
			return;
		for (auto Index = 0; Index < NumActor;  Index++)
		{
			if (!changedbitset.test(Index))
				continue;
			auto CacheActor = CacheActors[Index];
			if (!ensure(IsValid(CacheActor)))
				continue;
			if (newbitset.test(Index))
			{
				CacheActor->SetActorTickEnabled(bStartWithTickEnable);
				CacheActor->SetActorEnableCollision(true);
#if !UE_BUILD_SHIPPING
				CacheActor->MarkComponentsRenderStateDirty();
#endif
				CacheActor->DispatchBeginPlay();
				SetComponentTick(CacheActor, true);
			}
			else
			{
				OnActorEndPlay(CacheActor);
			}
		}
		NumActor = CacheActors.Num();
		CacheClientUsingBit = UsingBit;
	}
}


void FOtterPoolActorEntry::PostReplicatedAdd(const struct FOtterPoolActorArray& InArraySerializer)
{
	if (ActorClass)
	{
		bStartWithTickEnable = ActorClass.GetDefaultObject()->PrimaryActorTick.bStartWithTickEnabled;
	}
	CacheClientUsingBit = UsingBit;
	NumActor = CacheActors.Num();
	for (auto Actor : CacheActors)
	{
		auto PoolInterface = Cast<IOtterPoolActorInterface>(Actor);
		if (PoolInterface && PoolInterface->ShouldCollectProperty())
			PoolInterface->CollectProperty(Actor, AActor::StaticClass());
	}
}

void FOtterPoolActorArray::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
}



