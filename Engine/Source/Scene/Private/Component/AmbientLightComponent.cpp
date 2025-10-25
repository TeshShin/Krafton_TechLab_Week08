#include "pch.h"
#include "Scene/Public/Component/AmbientLightComponent.h"
#include "Manager/Public/AssetManager.h"
#include "Renderer/Public/LightData.h"

IMPLEMENT_CLASS(UAmbientLightComponent, ULightComponent)

FUnifiedDynamicLight UAmbientLightComponent::GetUnifiedLightData() const
{
    FUnifiedDynamicLight LightData = {};

    LightData.Intensity = GetIntensity();
    LightData.Color = GetLightColor();
	LightData.LightType = static_cast<uint32>(EDynamicLightType::Ambient);
	LightData.ShadowBias = 0;
	LightData.bCastShadows = bCastShadows;
	LightData.ShadowMapIndex = ShadowMapIdx;

    return LightData;
}

class UTexture* UAmbientLightComponent::GetLightBillboardTexture()
{
	return UAssetManager::GetInstance().LoadTexture("Data/Icons/SkyLight_64x.png");
}
