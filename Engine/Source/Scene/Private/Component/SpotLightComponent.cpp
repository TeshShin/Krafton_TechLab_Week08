#include "pch.h"
#include "Scene/Public/Component/SpotLightComponent.h"
#include "Asset/Public/JsonSerializer.h"
#include "Editor/Public/UI/Widget/Component/SpotLightComponentWidget.h"
#include "Renderer/Public/LightData.h"

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

FUnifiedDynamicLight USpotLightComponent::GetUnifiedLightData() const
{
    FUnifiedDynamicLight LightData = {};

    LightData.Position = GetWorldLocation();
    LightData.Intensity = GetIntensity();
    LightData.Color = GetLightColor();
    LightData.SourceRadius = SourceRadius;
    LightData.Direction = GetWorldForwardVector();
    LightData.FalloffExponent = FalloffExponent;
    LightData.Param0 = InnerConeAngle;
    LightData.Param1 = OuterConeAngle;
    LightData.LightType = static_cast<uint32>(EDynamicLightType::Spot);

    return LightData;
}
