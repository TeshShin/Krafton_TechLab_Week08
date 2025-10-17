# include "pch.h"
# include "Scene/Public/Component/LightComponent.h"
# include "Asset/Public/JsonSerializer.h"
# include "Renderer/Public/LightData.h"

IMPLEMENT_ABSTRACT_CLASS(ULightComponent, ULightComponentBase)

void ULightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
}

UObject* ULightComponent::Duplicate()
{
	ULightComponent* LightComponent = Cast<ULightComponent>(Super::Duplicate());
	return LightComponent;
}

void ULightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

FUnifiedDynamicLight ULightComponent::GetUnifiedLightData() const
{
	// Base implementation returns dummy data
	// Derived classes should override this to provide specific light data
	return FUnifiedDynamicLight();
}
