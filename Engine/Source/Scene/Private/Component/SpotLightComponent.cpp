#include "pch.h"
#include "Scene/Public/Component/PointLightComponent.h"
#include "Scene/Public/Component/SpotLightComponent.h"
#include "Asset/Public/JsonSerializer.h"
#include "Editor/Public/Line/BatchLineManager.h"
#include "Editor/Public/UI/Widget/Component/SpotLightComponentWidget.h"
#include "Manager/Public/AssetManager.h"
#include "Renderer/Public/LightData.h"
#include "Editor/Public/Camera.h"

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
		FJsonSerializer::ReadBool(InOutHandle, "UsePSM", bUsePSM, false);
		FJsonSerializer::ReadFloat(InOutHandle, "PSMFovScale", PSMFovScale, 1.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "PSMNearOffset", PSMNearOffset, 0.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "PSMFarOffset", PSMFarOffset, 0.0f);
	}
	else
	{
		InOutHandle["InnerConeAngle"] = InnerConeAngle;
		InOutHandle["OuterConeAngle"] = OuterConeAngle;
		InOutHandle["UsePSM"] = bUsePSM;
		InOutHandle["PSMFovScale"] = PSMFovScale;
		InOutHandle["PSMNearOffset"] = PSMNearOffset;
		InOutHandle["PSMFarOffset"] = PSMFarOffset;
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
}

FMatrix USpotLightComponent::ComputePSMLightViewProjection(const UCamera& InCamera) const
{
	// 1) 라이트 뷰 행렬 구성 (기존 방식과 동일)
	const FVector LightPosition = GetWorldLocation();
	const FVector Right = GetWorldRightVector();
	const FVector Up = GetWorldUpVector();
	const FVector Forward = GetWorldForwardVector();

	FMatrix T = FMatrix::TranslationMatrixInverse(LightPosition);
	FMatrix R = FMatrix(Right, Up, Forward);
	R = R.Transpose();
	const FMatrix LightView = T * R;

	// 2) 카메라 역 뷰/역 투영 행렬
	const FCameraConstants Inv = InCamera.GetFViewProjConstantsInverse();
	const FMatrix InvView = Inv.View;
	const FMatrix InvProj = Inv.Projection;

	// 3) 카메라 절두체 8 코너를 월드 → 라이트 뷰로 변환
	const float Ndc[2] = { -1.0f, 1.0f };
	float MaxAngleRad = 0.0f;
	float MinZ = FLT_MAX;
	float MaxZ = -FLT_MAX;

	for (int iz = 0; iz < 2; ++iz)
	{
		const float Z = (iz == 0) ? 0.0f : 1.0f; // D3D 클립 z [0..1]
		for (int iy = 0; iy < 2; ++iy)
		{
			for (int ix = 0; ix < 2; ++ix)
			{
				const FVector4 CornerNDC(Ndc[ix], Ndc[iy], Z, 1.0f);
				// NDC -> View -> World
				FVector4 CornerView = CornerNDC * InvProj;
				if (CornerView.W != 0.0f) { CornerView = CornerView * (1.0f / CornerView.W); }
				FVector4 CornerWorld = CornerView * InvView;
				if (CornerWorld.W != 0.0f) { CornerWorld = CornerWorld * (1.0f / CornerWorld.W); }

				// 라이트 시점 Z 범위
				FVector4 CornerLight = CornerWorld * LightView;
				MinZ = std::min(MinZ, CornerLight.Z);
				MaxZ = std::max(MaxZ, CornerLight.Z);

				// 라이트 전방축과의 최대 각도 측정
				const FVector ToCorner = FVector(CornerWorld.X, CornerWorld.Y, CornerWorld.Z) - LightPosition;
				FVector DirToCorner = ToCorner;
				if (DirToCorner.LengthSquared() > MATH_EPSILON) { DirToCorner.Normalize(); }
				const float CosTheta = std::clamp(Forward.Dot(DirToCorner), -1.0f, 1.0f);
				const float Angle = std::acos(CosTheta); // [0..pi]
				MaxAngleRad = std::max(MaxAngleRad, Angle);
			}
		}
	}

	// 4) FOV/near/far 결정 및 콘 제한
	const float OuterConeDeg = GetOuterConeAngle();
	const float OuterConeRad = OuterConeDeg * ToRad;

	// 절두체를 덮는 최소 FOV, 단 콘을 넘지 않게 클램프
	float FovRad = std::min(2.0f * MaxAngleRad * PSMFovScale, 2.0f * OuterConeRad);

	// z 범위 정리: D3D RH 계열 투영을 가정(현재 카메라/라이트 투영과 동일)
	const float Atten = GetAttenuationRadius();
	float NearZ = std::max(0.1f, MinZ + PSMNearOffset);
	float FarZ = std::min(Atten, MaxZ + PSMFarOffset);

	if (!(FarZ > NearZ + 1e-3f))
	{
		// 퇴행 케이스: 기본(기존) 설정으로 폴백
		FovRad = OuterConeRad * 2.0f;
		NearZ = 0.1f;
		FarZ = Atten;
	}

	// 5) 투영 행렬 생성 (정사각 shadow map이므로 aspect=1)
	const float Aspect = 1.0f;
	const FMatrix Proj = FMatrix::CreatePerspectiveFOV(FovRad, Aspect, NearZ, FarZ);

	return LightView * Proj;

}
