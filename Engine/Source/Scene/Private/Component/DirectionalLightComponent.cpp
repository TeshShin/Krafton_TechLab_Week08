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

void UDirectionalLightComponent::UpdateLightMatricesInternal(const FMatrix& InCameraInverseVP) const
{
	CachedLightViewMatrices.clear();
    CachedLightViewProjection.clear();

    // --- 1. 메인 카메라의 절두체(Frustum) 8개 꼭짓점 계산 ---
    FVector FrustumCornersNDC[8] =
    {
        FVector(-1.0f, -1.0f, 0.0f), // Near-Bottom-Left
        FVector( 1.0f, -1.0f, 0.0f), // Near-Bottom-Right
        FVector(-1.0f,  1.0f, 0.0f), // Near-Top-Left
        FVector( 1.0f,  1.0f, 0.0f), // Near-Top-Right
        FVector(-1.0f, -1.0f, 1.0f), // Far-Bottom-Left
        FVector( 1.0f, -1.0f, 1.0f), // Far-Bottom-Right
        FVector(-1.0f,  1.0f, 1.0f), // Far-Top-Left
        FVector( 1.0f,  1.0f, 1.0f)  // Far-Top-Right
    };

    // NDC 좌표를 월드 좌표로 변환
    FVector FrustumCornersWorld[8];
    FVector FrustumCenterWorld = FVector(0, 0, 0);
    for (uint32 Idx = 0; Idx < 8; ++Idx)
    {
        // NDC -> World
    	const FVector4 CornerWorldH = InCameraInverseVP.TransformHomogeneous(FrustumCornersNDC[Idx]);
    	if (abs(CornerWorldH.W) < 1e-6f)
    	{
    		FrustumCornersWorld[Idx] = FVector(0, 0, 0);
    	}
    	else
    	{
    		FrustumCornersWorld[Idx] = FVector(
				CornerWorldH.X / CornerWorldH.W,
				CornerWorldH.Y / CornerWorldH.W,
				CornerWorldH.Z / CornerWorldH.W
			);
    	}

    	FrustumCenterWorld += FrustumCornersWorld[Idx];
    }
    FrustumCenterWorld *= 1 / 8.0f;

    // --- 2. 라이트 뷰 매트릭스 (V) 생성 (Orthographic) ---
    const FVector LightDirection = GetWorldForwardVector().GetNormalized();
	const FVector LightRight = GetWorldRightVector().GetNormalized();
	const FVector LightUp = GetWorldUpVector().GetNormalized();
    const FVector LightPosition = FrustumCenterWorld - LightDirection * 1000.0f;

	CachedLightViewMatrices.emplace_back(FMatrix::CreateViewFromAxes(LightPosition, LightRight, LightUp, LightDirection));

    // --- 3. 라이트 프로젝션 매트릭스 (P) 생성 (Orthographic) ---
    FVector MinBounds(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector MaxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (int i = 0; i < 8; ++i)
    {
        FVector CornerInLightView = CachedLightViewMatrices[0].TransformPosition(FrustumCornersWorld[i]);

        MinBounds.X = min(MinBounds.X, CornerInLightView.X);
        MaxBounds.X = max(MaxBounds.X, CornerInLightView.X);
        MinBounds.Y = min(MinBounds.Y, CornerInLightView.Y);
        MaxBounds.Y = max(MaxBounds.Y, CornerInLightView.Y);
        MinBounds.Z = min(MinBounds.Z, CornerInLightView.Z);
        MaxBounds.Z = max(MaxBounds.Z, CornerInLightView.Z);
    }
	float NearZ = MinBounds.Z;
	float FarZ = MaxBounds.Z;

    CachedLightProjectionMatrix = FMatrix::CreateOrthographicOffCenter(
        MinBounds.X, MaxBounds.X, MinBounds.Y, MaxBounds.Y, NearZ, FarZ
    );

    // --- 4. 최종 VP 매트릭스 캐시 ---
    CachedLightViewProjection.emplace_back(CachedLightViewMatrices[0] * CachedLightProjectionMatrix);
}
