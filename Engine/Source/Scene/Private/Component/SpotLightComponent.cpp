#include "pch.h"
#include "Scene/Public/Component/PointLightComponent.h"
#include "Scene/Public/Component/SpotLightComponent.h"
#include "Asset/Public/JsonSerializer.h"
#include "Editor/Public/Line/BatchLineManager.h"
#include "Editor/Public/UI/Widget/Component/SpotLightComponentWidget.h"
#include "Manager/Public/AssetManager.h"
#include "Renderer/Public/LightData.h"
#include "Renderer/Public/Renderer.h"
#include "Editor/Public/Viewport.h"
#include "Editor/Public/Camera.h"
#include "Renderer/Public/ShadowMapManager.h"
// 카메라 NDC 8코너
static void BuildCameraNDCCorners(FVector4 OutCorners[8])
{
	OutCorners[0] = FVector4(-1.0f, -1.0f, 0.0f, 1.0f);
	OutCorners[1] = FVector4(1.0f, -1.0f, 0.0f, 1.0f);
	OutCorners[2] = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
	OutCorners[3] = FVector4(-1.0f, 1.0f, 0.0f, 1.0f);
	OutCorners[4] = FVector4(-1.0f, -1.0f, 1.0f, 1.0f);
	OutCorners[5] = FVector4(1.0f, -1.0f, 1.0f, 1.0f);
	OutCorners[6] = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	OutCorners[7] = FVector4(-1.0f, 1.0f, 1.0f, 1.0f);
}

static FVector4 MultiplyAndDoPerspectiveDivide(const FVector4& Point, const FMatrix& Matrix)
{
	FVector4 Result = FMatrix::VectorMultiply(Point, Matrix);
	if (Result.W != 0.0f)
	{
		Result.X /= Result.W;
		Result.Y /= Result.W;
		Result.Z /= Result.W;
		Result.W = 1.0f;
	}
	return Result;
}

// 카메라 역행렬(에디터 카메라 제공)로 NDC 코너 → 월드
static void BuildCameraFrustumCornersWorld(const UCamera* InCamera, FVector4 OutWorld[8])
{
	FCameraConstants Inverse = InCamera->GetFViewProjConstantsInverse(); // View=V^-1, Projection=P^-1
	FVector4 Ndc[8];
	BuildCameraNDCCorners(Ndc);

	for (int32 i = 0; i < 8; ++i)
	{
		FVector4 View = MultiplyAndDoPerspectiveDivide(Ndc[i], Inverse.Projection);
		OutWorld[i] = MultiplyAndDoPerspectiveDivide(View, Inverse.View);
	}

}

// 워프된 XY 바운드를 직교 투영으로 하는 행렬(D3D LH, z: 0..1)
static FMatrix BuildOrthographicFromBounds(float Left, float Right, float Bottom, float Top, float NearZ, float FarZ)
{
	FMatrix P = FMatrix::Identity();
	P.Data[0][0] = 2.0f / (Right - Left);
	P.Data[1][1] = 2.0f / (Top - Bottom);
	P.Data[2][2] = 1.0f / (FarZ - NearZ);
	P.Data[3][0] = -(Right + Left) / (Right - Left);
	P.Data[3][1] = -(Top + Bottom) / (Top - Bottom);
	P.Data[3][2] = -NearZ / (FarZ - NearZ);
	P.Data[3][3] = 1.0f;
	return P;
}

// 스포트라이트용 PSM 워프(카메라 근거리 확대 유도): w' = z 형태(프로젝티브), 라이트 View와 SpotPerspective 사이에 둠
static FMatrix BuildSpotPSMWarp(float NearZ, float FarZ, float Lambda /* 0..1 */ )
{
	NearZ = std::max(0.01f, NearZ);
	FarZ = std::max(FarZ, NearZ + 0.01f);

	const float Den = (FarZ - NearZ);
	const float A = FarZ / Den;
	const float B = -NearZ * FarZ / Den;

	// Full PSM: w' = z, z' = A*z + 1*w  (row-vector * matrix)
	FMatrix Full = FMatrix::Identity();
	Full.Data[2][2] = A;
	Full.Data[2][3] = 1.0f;
	Full.Data[3][2] = B;
	Full.Data[3][3] = 0.0f;

	if (Lambda <= 0.0f) return FMatrix::Identity();
	if (Lambda >= 1.0f) return Full;

	// 람다 보간(안정화)
	FMatrix Lerp = FMatrix::Identity();
	Lerp.Data[2][2] = 1.0f + Lambda * (Full.Data[2][2] - 1.0f);
	Lerp.Data[2][3] = 0.0f + Lambda * (Full.Data[2][3] - 0.0f);
	Lerp.Data[3][2] = 0.0f + Lambda * (Full.Data[3][2] - 0.0f);
	Lerp.Data[3][3] = 1.0f + Lambda * (Full.Data[3][3] - 1.0f);
	return Lerp;

}
IMPLEMENT_CLASS(USpotLightComponent, UPointLightComponent)

USpotLightComponent::USpotLightComponent()
{
	bCanEverTick = true;
	bCastShadows = true;
}

void USpotLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "InnerConeAngle", InnerConeAngle);
		FJsonSerializer::ReadFloat(InOutHandle, "OuterConeAngle", OuterConeAngle);
	}
	else
	{
		InOutHandle["InnerConeAngle"] = InnerConeAngle;
		InOutHandle["OuterConeAngle"] = OuterConeAngle;
	}
}

UObject* USpotLightComponent::Duplicate()
{
	USpotLightComponent* SpotLightComponent = Cast<USpotLightComponent>(Super::Duplicate());
	SpotLightComponent->InnerConeAngle = InnerConeAngle;
	SpotLightComponent->OuterConeAngle = OuterConeAngle;

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
	FUnifiedDynamicLight LightData = Super::GetUnifiedLightData();

    LightData.Direction = GetWorldForwardVector();
    LightData.Param0 = InnerConeAngle;
    LightData.Param1 = OuterConeAngle;
    LightData.LightType = static_cast<uint32>(EDynamicLightType::Spot);

    return LightData;
}

void USpotLightComponent::DrawDebugArrow(TArray<FName>& InOutLabels)
{
	auto& LineManager = UBatchLineManager::GetInstance();
	const FVector Tip = GetWorldLocation();
	const FVector Dir = GetWorldForwardVector();
	const FVector End = Tip + Dir * 2.0f;
	const FVector4 Color(1.0f, 0.0f, 0.0f, 1.0f);
	FName Label = FName(std::format("{}_Arrow", GetName().ToString()));
	LineManager.AddDebugArrow(Label, Tip, End, Color, 1.0f, InOutLabels);
}

void USpotLightComponent::DrawDebugLines()
{
	auto& LineManager = UBatchLineManager::GetInstance();
	const FVector Tip = GetWorldLocation();
	const FVector Dir = GetWorldForwardVector();
	const float Radius = GetAttenuationRadius();

	// 1. 외부 원뿔(Outer Cone) 그리기
	LineManager.AddDebugCone(FName(std::format("{}_Outer", GetName().ToString())),
		Tip, Dir, Radius, GetOuterConeAngle(), FVector4(1.0f, 1.0f, 0.0f, 1.0f), DebugLineLabels);

	// 2. 내부 원뿔(Inner Cone) 그리기
	LineManager.AddDebugCone(FName(std::format("{}_Inner", GetName().ToString())),
		Tip, Dir, Radius, GetInnerConeAngle(), FVector4(0.0f, 1.0f, 0.0f, 1.0f), DebugLineLabels);
}

void USpotLightComponent::SetInnerConeAngle(float InInnerConeAngle)
{
	InnerConeAngle = std::clamp(InInnerConeAngle, 0.0f, OuterConeAngle);
	if (bIsSelected)
	{
		ClearDebugLines();
		DrawDebugLines();
	}
}

void USpotLightComponent::SetOuterConeAngle(float InOuterConeAngle)
{
	OuterConeAngle = std::clamp(InOuterConeAngle, InnerConeAngle, 90.0f);
	if (bIsSelected)
	{
		ClearDebugLines();
		DrawDebugLines();
	}
}

UTexture* USpotLightComponent::GetLightBillboardTexture()
{
	return UAssetManager::GetInstance().LoadTexture("Data/Icons/SpotLight_64x.png");
}

const FMatrix& USpotLightComponent::GetLightViewProjectionMatrix() const
{
	/*
	if (bIsLightVPDirty)
	{
		const FVector LightPosition = GetWorldLocation();
		const FVector Right = GetWorldRightVector();
		const FVector Up = GetWorldUpVector();
		const FVector Forward = GetWorldForwardVector();

		FMatrix T = FMatrix::TranslationMatrixInverse(LightPosition);
		FMatrix R = FMatrix(Right, Up, Forward);
		R = R.Transpose();

		FMatrix ViewMatrix = T * R;

		// Projection Matrix 생성 (Perspective)
		// 섀도우 맵은 보통 정사각형이므로 종횡비(AspectRatio)는 1.0
		float AspectRatio = 1.0f;
		float FOV = OuterConeAngle * 2.0f * ToRad;

		// Near/Far 클립 평면 설정
		float NearZ = 0.1f;
		float FarZ = GetAttenuationRadius(); // 빛의 최대 도달 거리
		FMatrix ProjMatrix = FMatrix::CreatePerspectiveFOV(FOV, AspectRatio, NearZ, FarZ);

		CachedLightViewProjection = ViewMatrix * ProjMatrix;
		bIsLightVPDirty = false;
	}

	return CachedLightViewProjection;
	*/
	// 1) LightView
	const FVector LightPosition = GetWorldLocation();
	const FVector Right = GetWorldRightVector();
	const FVector Up = GetWorldUpVector();
	const FVector Forward = GetWorldForwardVector();

	FMatrix T = FMatrix::TranslationMatrixInverse(LightPosition);
	FMatrix R = FMatrix(Right, Up, Forward).Transpose();
	FMatrix LightView = T * R;

	// 2) 활성 카메라
	UCamera* ActiveCamera = nullptr;
	if (URenderer::GetInstance().GetViewportClient())
	{
		ActiveCamera = URenderer::GetInstance().GetViewportClient()->GetActiveCamera();
	}

	// 카메라가 없으면 유니폼(기존 스팟)으로 폴백
	if (!ActiveCamera)
	{
		float Aspect = 1.0f;
		float FovRad = GetOuterConeAngle() * 2.0f * ToRad;
		float NearZ = 0.1f;
		float FarZ = GetAttenuationRadius();
		CachedLightViewProjection = LightView * FMatrix::CreatePerspectiveFOV(FovRad, Aspect, NearZ, FarZ);
		bIsLightVPDirty = false;
		return CachedLightViewProjection;
	}

	// 3) 카메라 절두체 코너(World) → 라이트 뷰로 변환
	FVector4 FrustumWorld[8];
	BuildCameraFrustumCornersWorld(ActiveCamera, FrustumWorld);

	FVector4 InLightView[8];
	for (int32 i = 0; i < 8; ++i)
	{
		InLightView[i] = FMatrix::VectorMultiply(FrustumWorld[i], LightView);
	}

	// 4) 워프용 z 범위(라이트 뷰 z) 추정 + 안정화
	float MinZ = FLT_MAX, MaxZ = -FLT_MAX;
	for (int32 i = 0; i < 8; ++i)
	{
		MinZ = std::min(MinZ, InLightView[i].Z);
		MaxZ = std::max(MaxZ, InLightView[i].Z);
	}
	const float Eps = 0.05f;
	float WarpNear = std::max(Eps, MinZ);
	float WarpFar = std::max(WarpNear + Eps, std::min(MaxZ + Eps, GetAttenuationRadius()));

	// 5) 스포트라이트 퍼스펙티브(Spot FOV 유지)
	const float Aspect = 1.0f;
	const float FovRad = GetOuterConeAngle() * 2.0f * ToRad;
	FMatrix SpotPerspective = FMatrix::CreatePerspectiveFOV(FovRad, Aspect, WarpNear, WarpFar);

	// 6) 카메라 기반 PSM 워프(라이트 뷰와 SpotPerspective 사이에 둠)
	const float Lambda = 0.9f; // 0~1, 0.7~0.9 추천
	FMatrix WarpPSM = BuildSpotPSMWarp(WarpNear, WarpFar, Lambda);

	// 7) 카메라 절두체를 LightView → WarpPSM → SpotPerspective로 보내 NDC XY 바운드 구함
	float MinX = FLT_MAX, MinY = FLT_MAX;
	float MaxX = -FLT_MAX, MaxY = -FLT_MAX;
	float MinZW = FLT_MAX, MaxZW = -FLT_MAX;

	for (int32 i = 0; i < 8; ++i)
	{
		FVector4 Clip = FMatrix::VectorMultiply(InLightView[i], WarpPSM);
		Clip = FMatrix::VectorMultiply(Clip, SpotPerspective);
		if (Clip.W != 0.0f)
		{
			float X = Clip.X / Clip.W;
			float Y = Clip.Y / Clip.W;
			float Z = Clip.Z / Clip.W;
			MinX = std::min(MinX, X);  MaxX = std::max(MaxX, X);
			MinY = std::min(MinY, Y);  MaxY = std::max(MaxY, Y);
			MinZW = std::min(MinZW, Z); MaxZW = std::max(MaxZW, Z);
		}
	}
	// 깊이 여유
	float OrthoNear = MinZW;
	float OrthoFar = MaxZW + Eps;

	// 8) 워프 후 XY 바운드를 딱 맞게 채우는 2D Crop(직교)
	FMatrix Crop = BuildOrthographicFromBounds(MinX, MaxX, MinY, MaxY, OrthoNear, OrthoFar);

	// 9) 최종: LightView × WarpPSM × SpotPerspective × Crop
	CachedLightViewProjection = LightView * WarpPSM * SpotPerspective * Crop;
	bIsLightVPDirty = false;
	return CachedLightViewProjection;
}
