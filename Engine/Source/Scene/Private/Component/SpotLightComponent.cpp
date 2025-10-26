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
// 카메라 NDC 코너 8개 생성 (z=0: Near, z=1: Far)
static void BuildCameraNDCCorners(FVector4 OutCorners[8])
{
	// Near plane (z=0)
	OutCorners[0] = FVector4(-1.0f, -1.0f, 0.0f, 1.0f);
	OutCorners[1] = FVector4(1.0f, -1.0f, 0.0f, 1.0f);
	OutCorners[2] = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
	OutCorners[3] = FVector4(-1.0f, 1.0f, 0.0f, 1.0f);
	// Far plane (z=1)
	OutCorners[4] = FVector4(-1.0f, -1.0f, 1.0f, 1.0f);
	OutCorners[5] = FVector4(1.0f, -1.0f, 1.0f, 1.0f);
	OutCorners[6] = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	OutCorners[7] = FVector4(-1.0f, 1.0f, 1.0f, 1.0f);
}

// 행렬 곱 후 w로 나눠주는 안전한 역투영 유틸
static FVector4 MultiplyAndProject(const FVector4& Point, const FMatrix& Matrix)
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

// 카메라 절두체 8코너를 월드 공간으로 변환
static void BuildCameraFrustumCornersWorld(const UCamera* InCamera, FVector4 OutWorld[8])
{
	FCameraConstants Inv = InCamera->GetFViewProjConstantsInverse(); // View=View^-1, Projection=Proj^-1
	FVector4 Ndc[8];
	BuildCameraNDCCorners(Ndc);

	for (int i = 0; i < 8; ++i)
	{
		FVector4 View = MultiplyAndProject(Ndc[i], Inv.Projection);
		OutWorld[i] = MultiplyAndProject(View, Inv.View);
	}

}

// 라이트 투영 뒤 NDC 영역을 [-1,1]^2로 정규화하는 Crop/Warp 행렬 (post-projection 2D warp)
static FMatrix BuildCropMatrixFromNDC(float MinX, float MaxX, float MinY, float MaxY)
{
	// 안전장치: 면적이 0에 가까우면 단위 행렬 반환
	const float Epsilon = 1e-6f;
	float ScaleX = 2.0f / std::max(MaxX - MinX, Epsilon);
	float ScaleY = 2.0f / std::max(MaxY - MinY, Epsilon);
	float OffsetX = -(MaxX + MinX) * 0.5f;
	float OffsetY = -(MaxY + MinY) * 0.5f;

	// row-major, row-vector 기준: x' = x*Sx + w*Tx, y' = y*Sy + w*Ty, z' = z, w'=w
	FMatrix Crop = FMatrix::Identity();
	Crop.Data[0][0] = ScaleX;
	Crop.Data[1][1] = ScaleY;
	Crop.Data[3][0] = OffsetX * ScaleX;
	Crop.Data[3][1] = OffsetY * ScaleY;
	// z, w는 그대로
	return Crop;

}

// PSM: View * Proj(Spot FOV) 뒤, 카메라 절두체 기반 Crop/Warp 를 곱해 LightViewProjection 구성
static FMatrix BuildSpotPSMLightViewProjection(const FMatrix& LightView, float OuterConeAngleDeg, float LightFar)
{
	// 1) 활성 카메라
	UCamera* ActiveCamera = nullptr;
	if (URenderer::GetInstance().GetViewportClient())
	{
		ActiveCamera = URenderer::GetInstance().GetViewportClient()->GetActiveCamera();
	}
	if (!ActiveCamera)
	{
		// 카메라가 없으면 기본 대칭 투영으로 반환
		float Aspect = 1.0f;
		float FovRad = OuterConeAngleDeg * 2.0f * ToRad;
		float NearZ = 0.1f;
		float FarZ = std::max(LightFar, NearZ + 0.1f);
		FMatrix Proj = FMatrix::CreatePerspectiveFOV(FovRad, Aspect, NearZ, FarZ);
		return LightView * Proj;
	}

	// 2) 카메라 절두체 8코너 월드 좌표
	FVector4 CameraWorld[8];
	BuildCameraFrustumCornersWorld(ActiveCamera, CameraWorld);

	// 3) 월드 → 라이트 View 변환
	FVector4 CornersLightView[8];
	for (int i = 0; i < 8; ++i)
	{
		CornersLightView[i] = FMatrix::VectorMultiply(CameraWorld[i], LightView);
	}

	// 4) 라이트 투영 근/원거리 안정화: 카메라 절두체 z 범위와 라이트 도달 반경을 고려
	float MinZ = FLT_MAX;
	float MaxZ = -FLT_MAX;
	for (int i = 0; i < 8; ++i)
	{
		MinZ = std::min(MinZ, CornersLightView[i].Z);
		MaxZ = std::max(MaxZ, CornersLightView[i].Z);
	}
	// 스포트라이트는 +Z(라이트 전방) 방향으로 보는 것으로 구성했으므로, Near>0이 되도록 보정
	const float Epsilon = 0.05f;
	float NearZ = std::max(Epsilon, MinZ);
	float FarZ = std::max(NearZ + Epsilon, std::min(MaxZ + Epsilon, LightFar));

	// 5) 기본 대칭 투영(Spot FOV, Aspect=1)
	float Aspect = 1.0f;
	float FovRad = OuterConeAngleDeg * 2.0f * ToRad;
	FMatrix LightProj = FMatrix::CreatePerspectiveFOV(FovRad, Aspect, NearZ, FarZ);

	// 6) 코너들을 Clip→NDC로 변환하여 XY 박스 구하기
	float MinX = FLT_MAX, MinY = FLT_MAX;
	float MaxX = -FLT_MAX, MaxY = -FLT_MAX;
	for (int i = 0; i < 8; ++i)
	{
		FVector4 Clip = FMatrix::VectorMultiply(CornersLightView[i], LightProj);
		// 퍼스펙티브 나눗셈
		if (Clip.W != 0.0f)
		{
			float X = Clip.X / Clip.W;
			float Y = Clip.Y / Clip.W;
			MinX = std::min(MinX, X);  MaxX = std::max(MaxX, X);
			MinY = std::min(MinY, Y);  MaxY = std::max(MaxY, Y);
		}
	}

	// 7) 카메라 절두체가 차지하는 NDC 영역을 [-1,1]^2로 누르는 Crop/Warp
	FMatrix Crop = BuildCropMatrixFromNDC(MinX, MaxX, MinY, MaxY);

	// 최종: View * Proj * Crop  (post-projection 워핑)
	return LightView * LightProj * Crop;

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
	// 라이트 View 구성(기존 코드 그대로)
	const FVector LightPosition = GetWorldLocation();
	const FVector Right = GetWorldRightVector();
	const FVector Up = GetWorldUpVector();
	const FVector Forward = GetWorldForwardVector();

	FMatrix T = FMatrix::TranslationMatrixInverse(LightPosition);
	FMatrix R = FMatrix(Right, Up, Forward);
	R = R.Transpose();
	FMatrix ViewMatrix = T * R;

	// 라이트 도달 반경(Spot의 최대 깊이)
	const float LightFar = GetAttenuationRadius();

	// PSM: 카메라 절두체 기반 Crop/Warp를 적용하여 LightViewProjection 생성
	CachedLightViewProjection =
		BuildSpotPSMLightViewProjection(ViewMatrix, GetOuterConeAngle(), LightFar);

	// 카메라 종속이므로 캐시는 실질적으로 의미가 적습니다. 필요 시 항상 true로 유지해도 됩니다.
	bIsLightVPDirty = false;
	return CachedLightViewProjection;
}
