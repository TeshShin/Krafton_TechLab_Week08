#include "pch.h"
#include "Scene/Public/Component/DirectionalLightComponent.h"
#include "Manager/Public/AssetManager.h"
#include "Renderer/Public/LightData.h"
#include "Editor/Public/Line/BatchLineManager.h"

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent)

UDirectionalLightComponent::UDirectionalLightComponent()
{
	bCastShadows = true;
}


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
	LightData.LightType = static_cast<uint32>(GetLightType());
	LightData.ShadowBias = 0.001f;
	LightData.bCastShadows = bCastShadows;
	LightData.ShadowMapIndex = ShadowMapIdx;

    return LightData;
}

class UTexture* UDirectionalLightComponent::GetLightBillboardTexture()
{
	return UAssetManager::GetInstance().LoadTexture("Data/Icons/DirectionalLight_64x.png");
}

void UDirectionalLightComponent::UpdateLightMatricesInternal(const FCameraConstants& InCameraInvConstants) const
{
	CachedLightViewMatrices.clear();
	CachedLightViewProjection.clear();

	// --- 0) 카메라 역행렬 준비 ---
	const FMatrix InverseView = InCameraInvConstants.View;        // View^-1
	const FMatrix InverseProjection = InCameraInvConstants.Projection; // Proj^-1
	const FMatrix InverseVP = InverseProjection * InverseView;

	// --- 1) 카메라 절두체 8점 (NDC) -> 월드 ---
	FVector FrustumCornersNDC[8] =
	{
		FVector(-1.0f, -1.0f, 0.0f), FVector(1.0f, -1.0f, 0.0f),
		FVector(-1.0f,  1.0f, 0.0f), FVector(1.0f,  1.0f, 0.0f),
		FVector(-1.0f, -1.0f, 1.0f), FVector(1.0f, -1.0f, 1.0f),
		FVector(-1.0f,  1.0f, 1.0f), FVector(1.0f,  1.0f, 1.0f)
	};
	FVector FrustumCornersWorld[8];
	FVector FrustumCenterWorld(0.0f, 0.0f, 0.0f);
	for (uint32 Idx = 0; Idx < 8; ++Idx)
	{
		const FVector4 CornerWorldH = InverseVP.TransformHomogeneous(FrustumCornersNDC[Idx]);
		if (std::abs(CornerWorldH.W) > 1e-6f)
		{
			FrustumCornersWorld[Idx] = FVector(
				CornerWorldH.X / CornerWorldH.W,
				CornerWorldH.Y / CornerWorldH.W,
				CornerWorldH.Z / CornerWorldH.W
			);
		}
		else
		{
			FrustumCornersWorld[Idx] = FVector(0, 0, 0);
		}
		FrustumCenterWorld += FrustumCornersWorld[Idx];
	}
	FrustumCenterWorld *= 1.0f / 8.0f;

	// --- 2) 라이트 뷰 L (라이트 방향 기준) ---
	const FVector LightDirection = GetWorldForwardVector().GetNormalized();
	const FVector LightRight = GetWorldRightVector().GetNormalized();
	const FVector LightUp = GetWorldUpVector().GetNormalized();
	const FVector LightPosition = FrustumCenterWorld - LightDirection * 1000.0f; // 프러스텀 중심에서 뒤로
	const FMatrix LightView = FMatrix::CreateViewFromAxes(LightPosition, LightRight, LightUp, LightDirection);

	// --- 3) 기존 LVP: Ortho 투영 (깊이 안정용) ---
	FVector MinBounds(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	for (int i = 0; i < 8; ++i)
	{
		FVector CornerInLightView = LightView.TransformPosition(FrustumCornersWorld[i]);
		MinBounds.X = min(MinBounds.X, CornerInLightView.X);
		MaxBounds.X = max(MaxBounds.X, CornerInLightView.X);
		MinBounds.Y = min(MinBounds.Y, CornerInLightView.Y);
		MaxBounds.Y = max(MaxBounds.Y, CornerInLightView.Y);
		MinBounds.Z = min(MinBounds.Z, CornerInLightView.Z);
		MaxBounds.Z = max(MaxBounds.Z, CornerInLightView.Z);
	}
	float NearZ = MinBounds.Z;
	float FarZ = MaxBounds.Z;
	const FMatrix OrthographicProjection = FMatrix::CreateOrthographicOffCenter(
		MinBounds.X, MaxBounds.X, MinBounds.Y, MaxBounds.Y, NearZ, FarZ);

	// --- LVP 모드: 기존 동작 유지 ---
	if (GetShadowProjectionMode() == EShadowProjectionMode::LVP)
	{
		CachedLightViewMatrices.emplace_back(LightView);
		CachedLightProjectionMatrix = OrthographicProjection;
		CachedLightViewProjection.emplace_back(CachedLightViewMatrices[0] * CachedLightProjectionMatrix);
		return;
	}
	// --- LiSPSM 모드: 추후 구현, 현재는 LVP로 폴백 ---
	else if (GetShadowProjectionMode() == EShadowProjectionMode::LiSPSM)
	{
		// TODO : LiSPSM 구현
	}
	else if (GetShadowProjectionMode() == EShadowProjectionMode::PSM)
	{
		// TODO : PSM 구현
	}
}
