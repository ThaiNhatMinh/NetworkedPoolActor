// Fill out your copyright notice in the Description page of Project Settings.


#include "OtterPoolActorInterface.h"


void IOtterPoolActorInterface::CollectProperty(AActor* Self, UClass* RootClass)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IOtterPoolActorInterface::CollectProperty);
	auto Collect = [this, Self](UClass* Class)
		{
			if (!Class)
				return;
			auto CDO = Self->GetClass()->GetDefaultObject<AActor>();
			//auto CDO = Class->GetDefaultObject();
			for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				FProperty * Prop = *It;
				const auto bIsComponent = CPF_InstancedReference | CPF_ContainsInstancedReference;
				auto Flag = Prop->GetPropertyFlags();
				if (Prop == nullptr || (!Prop->HasAnyPropertyFlags(EPropertyFlags::CPF_Edit) && !Prop->HasAnyPropertyFlags(CPF_Net)) || Prop->HasAnyPropertyFlags(CPF_EditorOnly))
					continue;
				if (auto ObjectProperty = CastField<FObjectPropertyBase>(Prop))
				{
					auto Object = ObjectProperty->LoadObjectPropertyValue(ObjectProperty->ContainerPtrToValuePtr<void*>(Self));
					if (Object && Object->HasAnyFlags(EObjectFlags::RF_DefaultSubObject))
					{
						continue;
					}
				}
				else if (Prop->HasAnyPropertyFlags(EPropertyFlags::CPF_BlueprintReadOnly))
					continue;

				auto TargetPropertyMemory = Prop->ContainerPtrToValuePtr<uint8>(Self);
				auto SourcePropertyMemory = Prop->ContainerPtrToValuePtrForDefaults<uint8>(Self->GetClass(), CDO);
				if (TargetPropertyMemory && SourcePropertyMemory)
					PropertyToReset.Add({Prop, TargetPropertyMemory, SourcePropertyMemory});
			}

		};
	auto Class = Self->GetClass();
	while (Class)
	{
		Collect(Class);
		Class = Class->GetSuperClass();
		if (Class == RootClass)
			break;
	}
}

void IOtterPoolActorInterface::ResetProperty(AActor* Self)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IOtterPoolActorInterface::ResetProperty);
	for (auto& Property : PropertyToReset)
	{
		//auto TargetPropertyMemory = Property->ContainerPtrToValuePtr<uint8>(Self);
		//auto SourcePropertyMemory = Property->ContainerPtrToValuePtrForDefaults<uint8>(Self->GetClass(), CDO);
		auto TargetPropertyMemory = Property.InstanceValueAddress;
		auto SourcePropertyMemory = Property.DefaultValueAddress;
		Property.Name->CopyCompleteValue(TargetPropertyMemory, SourcePropertyMemory);
	}
}