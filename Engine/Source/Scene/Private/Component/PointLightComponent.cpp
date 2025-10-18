#include "pch.h"
#include "Scene/Public/Component/PointLightComponent.h"
#include "Asset/Public/JsonSerializer.h"
#include "Editor/Public/UI/Widget/Component/PointLightComponentWidget.h"
#include "Renderer/Public/LightData.h"

IMPLEMENT_CLASS(UPointLightComponent, ULightComponentBase)

void UPointLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "LightFalloffExponent", LightFalloffExponent);
		FJsonSerializer::ReadFloat(InOutHandle, "AttenuationRadius", AttenuationRadius);
	}
	else
	{
		InOutHandle["LightFalloffExponent"] = LightFalloffExponent;
		InOutHandle["AttenuationRadius"] = AttenuationRadius;
	}
}

UObject* UPointLightComponent::Duplicate()
{
	UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>(Super::Duplicate());
	PointLightComponent->LightFalloffExponent = LightFalloffExponent;
	PointLightComponent->AttenuationRadius = AttenuationRadius;

	return PointLightComponent;
}

void UPointLightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

UClass* UPointLightComponent::GetSpecificWidgetClass() const
{
    return UPointLightComponentWidget::StaticClass();
}

FUnifiedDynamicLight UPointLightComponent::GetUnifiedLightData() const
{
    FUnifiedDynamicLight LightData = {};

    LightData.Position = GetWorldLocation();
    LightData.Intensity = GetIntensity();
    LightData.Color = GetLightColor();
	LightData.AttenuationRadius = GetAttenuationRadius();
    LightData.FalloffExponent = GetLightFalloffExponent();
    LightData.LightType = static_cast<uint32>(EDynamicLightType::Point);

    return LightData;
}
