#include "pch.h"
#include "Scene/Public/Component/SpotLightComponent.h"
#include "Asset/Public/JsonSerializer.h"
#include "Editor/Public/UI/Widget/Component/SpotLightComponentWidget.h"

IMPLEMENT_CLASS(USpotLightComponent, ULightComponentBase)

void USpotLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// TODO: Serialize SpotLight specific properties
	if (bInIsLoading)
	{
		// TODO: Read SpotLight specific properties
	}
	else
	{
		// TODO: Write SpotLight specific properties
	}
}

UObject* USpotLightComponent::Duplicate()
{
	USpotLightComponent* SpotLightComponent = Cast<USpotLightComponent>(Super::Duplicate());

	// TODO: Duplicate SpotLight specific properties

	return SpotLightComponent;
}

void USpotLightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

UClass* USpotLightComponent::GetSpecificWidgetClass() const
{
    return USpotLightComponentWidget::StaticClass();
}
