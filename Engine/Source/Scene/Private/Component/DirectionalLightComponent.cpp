#include "pch.h"
#include "Scene/Public/Component/DirectionalLightComponent.h"
#include "Manager/Public/AssetManager.h"
#include "Renderer/Public/LightData.h"

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent)

FUnifiedDynamicLight UDirectionalLightComponent::GetUnifiedLightData() const
{
    FUnifiedDynamicLight LightData = {};

    LightData.Direction = GetWorldForwardVector();
    LightData.Intensity = GetIntensity();
    LightData.Color = GetLightColor();
    LightData.LightType = static_cast<uint32>(EDynamicLightType::Directional);

    return LightData;
}

class UTexture* UDirectionalLightComponent::GetLightBillboardTexture()
{
	return UAssetManager::GetInstance().LoadTexture("Data/Icons/DirectionalLight_64x.png");
}
