#include "pch.h"
#include "Scene/Public/Component/DirectionalLightComponent.h"
#include "Manager/Public/AssetManager.h"
#include "Renderer/Public/LightData.h"
#include "Editor/Public/Line/BatchLineManager.h"

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent)

void UDirectionalLightComponent::DrawDebugArrow(TArray<FName>& InOutLabels)
{
	auto& LineManager = UBatchLineManager::GetInstance();
	const FVector Start = GetWorldLocation();
	const FVector End = Start + GetWorldForwardVector() * 2.0f;
	const FVector4 Color(1.0f, 0.0f, 0.0f, 1.0f);
	FName Label = FName(std::format("{}_Arrow", GetName().ToString()));

	LineManager.AddDebugArrow(Label, Start, End, Color, 1.0f, InOutLabels);
}

FUnifiedDynamicLight UDirectionalLightComponent::GetUnifiedLightData() const
{
    FUnifiedDynamicLight LightData = {};

    LightData.Direction = GetWorldForwardVector();
    LightData.Intensity = GetIntensity();
    LightData.Color = GetLightColor();
	LightData.LightType = static_cast<uint32>(EDynamicLightType::Directional);
	LightData.ShadowBias = 0;
	LightData.bCastShadows = bCastShadows;
	LightData.ShadowMapIndex = ShadowMapIdx;

    return LightData;
}

class UTexture* UDirectionalLightComponent::GetLightBillboardTexture()
{
	return UAssetManager::GetInstance().LoadTexture("Data/Icons/DirectionalLight_64x.png");
}
