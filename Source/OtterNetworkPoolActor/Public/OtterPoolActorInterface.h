// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "OtterPoolActorInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UOtterPoolActorInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class OTTERNETWORKPOOLACTOR_API IOtterPoolActorInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	void SetEnable(bool bIsEnable) { bEnable = bIsEnable; }
	bool IsEnable() const { return bEnable; }

	virtual bool ShouldCollectProperty() const { return true; }
	virtual void CollectProperty(AActor* Self, UClass* RootClass);
	virtual void ResetProperty(AActor* Self);

private:
	bool bEnable = false;

	struct FPropertyRestoreData
	{
		FProperty* Name;
		uint8* InstanceValueAddress;
		const uint8* DefaultValueAddress;
	};
	// CDO to Instance
	TArray<FPropertyRestoreData> PropertyToReset;
	
};
