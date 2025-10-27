#include "pch.h"
#include "Scene/Public/Component/DirectionalLightComponent.h"
#include "Manager/Public/AssetManager.h"
#include "Renderer/Public/LightData.h"
#include "Editor/Public/Line/BatchLineManager.h"
#include "Renderer/Public/Shadow/PSMBuilder.h"
#include "Renderer/Public/Renderer.h"
IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent)

UDirectionalLightComponent::UDirectionalLightComponent()
{
	bCanEverTick = true;
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
	LightData.LightType = static_cast<uint32>(EDynamicLightType::Directional);
	LightData.LightViewProjection = GetLightViewProjectionMatrix();
	LightData.ShadowBias = 0;
	LightData.bCastShadows = bCastShadows;
	LightData.ShadowMapIndex = ShadowMapIdx;

    return LightData;
}

class UTexture* UDirectionalLightComponent::GetLightBillboardTexture()
{
	return UAssetManager::GetInstance().LoadTexture("Data/Icons/DirectionalLight_64x.png");
}

const FMatrix& UDirectionalLightComponent::GetLightViewProjectionMatrix() const
{
	const FVector Forward = GetWorldForwardVector();

	// 1) 전역 토글 OFF면: 워프 없이 오쏘 크롭으로 즉시 반환
	if (!URenderer::GetInstance().GetUseDirectionalPSM())
	{
		UCamera* ActiveCamera = FPSMBuilder::ResolveActiveOrFallbackCamera();
		if (!ActiveCamera)
		{
			CachedLightViewProjection = FMatrix::Identity();
			bIsLightVPDirty = false;
			return CachedLightViewProjection;
		}

		// 라이트 뷰(오리엔테이션)
		FVector WorldUp(0.0f, 0.0f, 1.0f);
		FVector ForwardSafe = Forward.GetNormalized();
		if (std::abs(ForwardSafe.Dot(WorldUp)) > 0.99f)
		{
			WorldUp = FVector(0.0f, 1.0f, 0.0f);
		}
		FVector Right = WorldUp.Cross(ForwardSafe).GetNormalized();
		FVector Up = ForwardSafe.Cross(Right).GetNormalized();
		const FMatrix LightView = FMatrix(Right, Up, ForwardSafe).Transpose();

		// 카메라 절두체 → 라이트뷰로 변환 후 바운드 계산
		FVector4 FrustumWorld[8];
		FPSMBuilder::BuildCameraFrustumCornersWorld(ActiveCamera, FrustumWorld);

		float MinX = FLT_MAX, MinY = FLT_MAX, MinZ = FLT_MAX;
		float MaxX = -FLT_MAX, MaxY = -FLT_MAX, MaxZ = -FLT_MAX;

		for (int32 Index = 0; Index < 8; ++Index)
		{
			const FVector4 P = FMatrix::VectorMultiply(FrustumWorld[Index], LightView);
			MinX = std::min(MinX, P.X); MaxX = std::max(MaxX, P.X);
			MinY = std::min(MinY, P.Y); MaxY = std::max(MaxY, P.Y);
			MinZ = std::min(MinZ, P.Z); MaxZ = std::max(MaxZ, P.Z);
		}

		const float Epsilon = 0.05f;
		const FMatrix Ortho = FPSMBuilder::BuildOrthographicFromBounds(
			MinX, MaxX, MinY, MaxY, MinZ, MaxZ + Epsilon);

		CachedLightViewProjection = LightView * Ortho;
		bIsLightVPDirty = false;
		return CachedLightViewProjection;
	}

	// 2) 전역 토글 ON: 정석 PSM 경로
	FMatrix BuiltLightViewProjection;
	const bool bBuilt = FPSMBuilder::BuildDirectionalLightPSM(Forward, BuiltLightViewProjection);
	if (bBuilt)
	{
		CachedLightViewProjection = BuiltLightViewProjection;
		bIsLightVPDirty = false;
		return CachedLightViewProjection;
	}

	// 3) 빌더 실패 시 폴백(워프 없는 오쏘 크롭)
	{
		UCamera* ActiveCamera = FPSMBuilder::ResolveActiveOrFallbackCamera();
		if (!ActiveCamera)
		{
			CachedLightViewProjection = FMatrix::Identity();
			bIsLightVPDirty = false;
			return CachedLightViewProjection;
		}

		// 라이트 뷰(오리엔테이션)
		FVector WorldUp(0.0f, 0.0f, 1.0f);
		FVector ForwardSafe = Forward.GetNormalized();
		if (std::abs(ForwardSafe.Dot(WorldUp)) > 0.99f)
		{
			WorldUp = FVector(0.0f, 1.0f, 0.0f);
		}
		FVector Right = WorldUp.Cross(ForwardSafe).GetNormalized();
		FVector Up = ForwardSafe.Cross(Right).GetNormalized();
		const FMatrix LightView = FMatrix(Right, Up, ForwardSafe).Transpose();

		FVector4 FrustumWorld[8];
		FPSMBuilder::BuildCameraFrustumCornersWorld(ActiveCamera, FrustumWorld);

		float MinX = FLT_MAX, MinY = FLT_MAX, MinZ = FLT_MAX;
		float MaxX = -FLT_MAX, MaxY = -FLT_MAX, MaxZ = -FLT_MAX;

		for (int32 Index = 0; Index < 8; ++Index)
		{
			const FVector4 P = FMatrix::VectorMultiply(FrustumWorld[Index], LightView);
			MinX = std::min(MinX, P.X); MaxX = std::max(MaxX, P.X);
			MinY = std::min(MinY, P.Y); MaxY = std::max(MaxY, P.Y);
			MinZ = std::min(MinZ, P.Z); MaxZ = std::max(MaxZ, P.Z);
		}

		const float Epsilon = 0.05f;
		const FMatrix Ortho = FPSMBuilder::BuildOrthographicFromBounds(
			MinX, MaxX, MinY, MaxY, MinZ, MaxZ + Epsilon);

		CachedLightViewProjection = LightView * Ortho;
		bIsLightVPDirty = false;
		return CachedLightViewProjection;
	}

}
