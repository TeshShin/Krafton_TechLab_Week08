#include "pch.h"
#include "Scene/Public/Component/AmbientLightComponent.h"
#include "Manager/Public/AssetManager.h"
#include "Renderer/Public/LightData.h"

IMPLEMENT_CLASS(UAmbientLightComponent, ULightComponent)

FUnifiedDynamicLight UAmbientLightComponent::GetUnifiedLightData() const
{
    FUnifiedDynamicLight UnifiedLight = {};

    UnifiedLight.Intensity = GetIntensity();
    UnifiedLight.Color = GetLightColor();
    UnifiedLight.LightType = static_cast<uint32>(EDynamicLightType::Ambient);

    return UnifiedLight;
}

class UTexture* UAmbientLightComponent::GetLightBillboardTexture()
{
	return UAssetManager::GetInstance().LoadTexture("Data/Icons/SkyLight_64x.png");
}
