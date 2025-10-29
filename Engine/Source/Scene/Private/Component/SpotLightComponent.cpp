#include "pch.h"
#include "Scene/Public/Component/PointLightComponent.h"
#include "Scene/Public/Component/SpotLightComponent.h"
#include "Asset/Public/JsonSerializer.h"
#include "Editor/Public/Line/BatchLineManager.h"
#include "Editor/Public/UI/Widget/Component/SpotLightComponentWidget.h"
#include "Manager/Public/AssetManager.h"
#include "Renderer/Public/LightData.h"
#include "Renderer/Public/ShadowMapManager.h"

IMPLEMENT_CLASS(USpotLightComponent, UPointLightComponent)

USpotLightComponent::USpotLightComponent()
{
	CachedLightViewProjection.reserve(1);
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
    LightData.LightType = static_cast<uint32>(GetLightType());
	LightData.ShadowBias = 0.001f;

    return LightData;
}

void USpotLightComponent::DrawDebugArrow(TArray<FName>& InOutLabels)
{
	if (!IsVisibleInHierarchy()) { return;	}
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

void USpotLightComponent::UpdateLightMatricesInternal(const FCameraConstants& InCameraInvConstants) const
{
	//if (!bIsLightVPDirty) { return; }

	CachedLightViewMatrices.clear();
	CachedLightViewProjection.clear();

	const FVector LightPosition = GetWorldLocation();
	const FVector Right = GetWorldRightVector();
	const FVector Up = GetWorldUpVector();
	const FVector Forward = GetWorldForwardVector();

	const FMatrix LightView = FMatrix::CreateViewFromAxes(LightPosition, Right, Up, Forward);

	const float NearZ_LVP = 0.1f;
	const float FarZ_LVP = GetAttenuationRadius();
	const float FovY = OuterConeAngle * 2.0f * ToRad;
	const float AspectRatio = 1.0f;
	const FMatrix LVPProjection = FMatrix::CreatePerspectiveFOV(FovY, AspectRatio, NearZ_LVP, FarZ_LVP);

	if (GetShadowProjectionMode() == EShadowProjectionMode::LVP)
	{
		CachedLightViewMatrices.emplace_back(LightView);
		CachedLightProjectionMatrix = LVPProjection;
		CachedLightViewProjection.emplace_back(CachedLightViewMatrices[0] * CachedLightProjectionMatrix);
		//bIsLightVPDirty = false;
		return;
	}
	else if (GetShadowProjectionMode() == EShadowProjectionMode::PSM)
	{
		// TODO : PSM 구현
		{
			// 1) 캐시 초기화
			CachedLightViewMatrices.clear();
			CachedLightViewProjection.clear();

			// 2) 카메라 View(정방향) 복원
			//    InCameraInvConstants.View = V^{-1} = R * T 형태
			const FMatrix CameraViewInverseMatrix = InCameraInvConstants.View;
			const FVector CameraRight(
				CameraViewInverseMatrix.Data[0][0],
				CameraViewInverseMatrix.Data[0][1],
				CameraViewInverseMatrix.Data[0][2]);
			const FVector CameraUp(
				CameraViewInverseMatrix.Data[1][0],
				CameraViewInverseMatrix.Data[1][1],
				CameraViewInverseMatrix.Data[1][2]);
			const FVector CameraForward(
				CameraViewInverseMatrix.Data[2][0],
				CameraViewInverseMatrix.Data[2][1],
				CameraViewInverseMatrix.Data[2][2]);
			const FVector CameraPosition = InCameraInvConstants.ViewWorldLocation;
			const FMatrix CameraViewMatrix = FMatrix::CreateViewFromAxes(
				CameraPosition, CameraRight, CameraUp, CameraForward);

			// 3) 카메라 Projection(정방향) 복원
			//    InCameraInvConstants.Projection = P^{-1} 구성:
			//    PInv[1][1] = tan(FovY/2), PInv[0][0] = Aspect * tan(FovY/2)
			const FMatrix CameraProjectionInverseMatrix = InCameraInvConstants.Projection;
			const float TanHalfFovY = CameraProjectionInverseMatrix.Data[1][1];
			// 보호: 직교 카메라 등 예외 시 LVP로 폴백
			if (TanHalfFovY <= 0.0001f)
			{
				CachedLightViewMatrices.emplace_back(CameraViewMatrix);
				const float NearZ_LVP = 0.1f;
				const float FarZ_LVP = GetAttenuationRadius();
				const float FovYSpot = OuterConeAngle * 2.0f * ToRad;
				const float AspectSpot = 1.0f;
				CachedLightProjectionMatrix = FMatrix::CreatePerspectiveFOV(FovYSpot, AspectSpot, NearZ_LVP, FarZ_LVP);
				CachedLightViewProjection.emplace_back(CachedLightViewMatrices[0] * CachedLightProjectionMatrix);
				//bIsLightVPDirty = false;
				return;
			}
			const float CameraFovYRadians = 2.0f * std::atan(TanHalfFovY);
			const float CameraAspectRatio = CameraProjectionInverseMatrix.Data[0][0] / TanHalfFovY;
			const float CameraNearZ = InCameraInvConstants.NearClip;
			const float CameraFarZ = InCameraInvConstants.FarClip;
			const FMatrix CameraProjectionMatrix = FMatrix::CreatePerspectiveFOV(
				CameraFovYRadians, CameraAspectRatio, CameraNearZ, CameraFarZ);

			// 4) 라이트 월드 위치를 카메라 후투영(Post-perspective) 공간으로 변환
			const FVector LightWorldPosition = GetWorldLocation();
			const FVector4 LightInViewH = CameraViewMatrix.TransformHomogeneous(LightWorldPosition);
			FVector LightInView = FVector(LightInViewH.X, LightInViewH.Y, LightInViewH.Z);
			// Near+ε 이상으로 클램프해 W<0 회피 (D3D LH에서 w=z)
			const float EpsZ = 1e-4f;
			if (LightInView.Z < CameraNearZ + EpsZ)
			{
				LightInView.Z = CameraNearZ + EpsZ;
			}

			// 클립 좌표 및 후투영 정규화
			const FVector4 LightInClipH = CameraProjectionMatrix.TransformHomogeneous(LightInView);
			FVector LightInPostPerspective = FVector(LightInClipH.X, LightInClipH.Y, LightInClipH.Z);
			const float InvW = 1.0f / std::max(LightInClipH.W, EpsZ);
			LightInPostPerspective.X *= InvW;
			LightInPostPerspective.Y *= InvW;
			LightInPostPerspective.Z *= InvW; // DX: [0,1]

			// 5) 후투영 공간에서 라이트 뷰(MVL) 구성: 라이트를 L에 두고 원점(0,0,0)을 바라봄
			const FVector Eye = LightInPostPerspective;
			const FVector Target(0.0f, 0.0f, 0.5f);
			FVector ForwardPP = Target - Eye;
			if (!ForwardPP.IsNearlyZero()) { ForwardPP.Normalize(); }
			FVector UpPP = FVector::UnitZ();
			// 전방과 상방이 평행하면 대체 상벡터 사용
			const float ParallelCheck = std::abs(ForwardPP.Dot(UpPP));
			if (ParallelCheck > 0.999f)
			{
				UpPP = FVector::UnitY();
			}
			FVector RightPP = UpPP.Cross(ForwardPP);
			if (!RightPP.IsNearlyZero()) { RightPP.Normalize(); }
			UpPP = ForwardPP.Cross(RightPP);
			if (!UpPP.IsNearlyZero()) { UpPP.Normalize(); }

			const FMatrix LightViewInPostPerspective = FMatrix::CreateViewFromAxes(
				Eye, RightPP, UpPP, ForwardPP);

			// 6) 유닛 큐브([-1,1]^2 x [0,1])의 8개 코너가 모두 보이도록 PL 계산
			//    MVL 공간으로 코너들을 변환하여 FOV/근/원거리 산출
			const FVector CubeCorners[8] =
			{
				FVector(-1.0f, -1.0f, 0.0f),
				FVector(-1.0f,  1.0f, 0.0f),
				FVector(1.0f, -1.0f, 0.0f),
				FVector(1.0f,  1.0f, 0.0f),
				FVector(-1.0f, -1.0f, 1.0f),
				FVector(-1.0f,  1.0f, 1.0f),
				FVector(1.0f, -1.0f, 1.0f),
				FVector(1.0f,  1.0f, 1.0f),
			};

			float MaxTanHalfFovX = 0.0f;
			float MaxTanHalfFovY = 0.0f;
			float NearZPSM = FLT_MAX;
			float FarZPSM = 0.0f;

			const float EpsilonZ = 1e-4f;
			for (int CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
			{
				const FVector4 CornerV = LightViewInPostPerspective.TransformHomogeneous(CubeCorners[CornerIndex]);
				// 뷰 공간 z는 LH에서 +Z가 전방
				const float Zv = (CornerV.W != 0.0f) ? (CornerV.Z / CornerV.W) : CornerV.Z;

				// 시야에 들어오는 양의 z만 고려
				const float ZvClamped = std::max(Zv, EpsilonZ);

				const float Xv = (CornerV.W != 0.0f) ? (CornerV.X / CornerV.W) : CornerV.X;
				const float Yv = (CornerV.W != 0.0f) ? (CornerV.Y / CornerV.W) : CornerV.Y;

				const float TanX = std::abs(Xv) / ZvClamped;
				const float TanY = std::abs(Yv) / ZvClamped;

				if (TanX > MaxTanHalfFovX) { MaxTanHalfFovX = TanX; }
				if (TanY > MaxTanHalfFovY) { MaxTanHalfFovY = TanY; }

				if (Zv > 0.0f)
				{
					if (Zv < NearZPSM) { NearZPSM = Zv; }
					if (Zv > FarZPSM) { FarZPSM = Zv; }
				}
			}

			// 안정화: 근/원거리, FOV, 종횡비(섀도우 아틀라스 정사각형)
			if (!(NearZPSM < FLT_MAX)) { NearZPSM = 0.01f; }
			NearZPSM = std::max(NearZPSM, 0.001f);
			FarZPSM = std::max(FarZPSM, NearZPSM + 0.01f);

			const float TanHalfFovMax = std::max(MaxTanHalfFovX, MaxTanHalfFovY);
			const float FovPSM = 2.0f * std::atan(TanHalfFovMax);
			const float AspectPSM = 1.0f;

			const FMatrix LightProjectionInPostPerspective =
				FMatrix::CreatePerspectiveFOV(FovPSM, AspectPSM, NearZPSM, FarZPSM);

			// View = V * P * MVL  (PSM에서 깊이 z는 이 공간의 z)
			const FMatrix LightViewPSM = CameraViewMatrix * CameraProjectionMatrix * LightViewInPostPerspective;
			CachedLightViewMatrices.emplace_back(LightViewPSM);
			// Projection = PL * Crop
			const FMatrix LightProjectionPSM = LightProjectionInPostPerspective;
			CachedLightProjectionMatrix = LightProjectionPSM;

			CachedLightViewProjection.emplace_back(CachedLightViewMatrices[0] * CachedLightProjectionMatrix);
			bIsLightVPDirty = false;
			return;
		}
	}
}
