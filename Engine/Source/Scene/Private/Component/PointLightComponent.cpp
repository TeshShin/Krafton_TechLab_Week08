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
		FJsonSerializer::ReadFloat(InOutHandle, "LightFalloffExtent", LightFalloffExtent);
		FJsonSerializer::ReadFloat(InOutHandle, "SourceRadius", SourceRadius);
	}
	else
	{
		InOutHandle["LightFalloffExtent"] = LightFalloffExtent;
		InOutHandle["SourceRadius"] = SourceRadius;
	}
}

UObject* UPointLightComponent::Duplicate()
{
	UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>(Super::Duplicate());
	PointLightComponent->LightFalloffExtent = LightFalloffExtent;
	PointLightComponent->SourceRadius = SourceRadius;

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
    LightData.SourceRadius = max(GetSourceRadius(), 1000.0f);  // Temp: ensure minimum radius
    LightData.FalloffExponent = GetLightFalloffExtent();
    LightData.LightType = static_cast<uint32>(EDynamicLightType::Point);

    return LightData;
}
