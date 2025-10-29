#include "pch.h"
#include "Scene/Public/Component/PointLightComponent.h"
#include "Scene/Public/Component/SpotLightComponent.h"
#include "Asset/Public/JsonSerializer.h"
#include "Editor/Public/Line/BatchLineManager.h"
#include "Editor/Public/UI/Widget/Component/SpotLightComponentWidget.h"
#include "Manager/Public/AssetManager.h"
#include "Renderer/Public/LightData.h"

IMPLEMENT_CLASS(USpotLightComponent, UPointLightComponent)

USpotLightComponent::USpotLightComponent()
{
	CachedLightViewProjection.reserve(1);
	bCanEverTick = true;
	bCastShadows = true;
	ShadowBias = 0.001f;
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
	LightData.ShadowBias = ShadowBias;
	LightData.ShadowSlopeBias = ShadowSlopeBias;

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

	const float NearZLVP = 0.1f;
	const float FarZLVP = GetAttenuationRadius();
	const float FovY = OuterConeAngle * 2.0f * ToRad;
	const float AspectRatio = 1.0f;
	const FMatrix LVPProjection = FMatrix::CreatePerspectiveFOV(FovY, AspectRatio, NearZLVP, FarZLVP);

	if (GetShadowProjectionMode() == EShadowProjectionMode::Basic)
	{
		CachedLightViewMatrices.emplace_back(LightView);
		CachedLightProjectionMatrix = LVPProjection;
		CachedLightViewProjection.emplace_back(CachedLightViewMatrices[0] * CachedLightProjectionMatrix);
		//bIsLightVPDirty = false;
		return;
	}
	else if (GetShadowProjectionMode() == EShadowProjectionMode::PSM)
	{
		// 1) 캐시 초기화
		//CachedLightViewMatrices.clear();
		//CachedLightViewProjection.clear();

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
		// 보호: 직교 카메라 등 예외 시 Basic로 폴백
		if (TanHalfFovY <= 0.0001f)
		{
			CachedLightViewMatrices.emplace_back(CameraViewMatrix);
			const float NearZLVP = 0.1f;
			const float FarZLVP = GetAttenuationRadius();
			const float FovYSpot = OuterConeAngle * 2.0f * ToRad;
			const float AspectSpot = 1.0f;
			CachedLightProjectionMatrix = FMatrix::CreatePerspectiveFOV(FovYSpot, AspectSpot, NearZLVP, FarZLVP);
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
		// 클립 좌표 및 후투영 정규화
		const FVector4 LightInClipH = CameraProjectionMatrix.TransformHomogeneous(LightInView);
		const float WClip = LightInClipH.W;
		const float EpsilonW = 1e-6f;
		const float EpsilonZ = 1e-4f;

		// W == 0(또는 너무 작음): 안전하게 Basic로 폴백
		if (std::abs(WClip) <= EpsilonW)
		{
			CachedLightViewMatrices.emplace_back(LightView);
			CachedLightProjectionMatrix = LVPProjection;
			CachedLightViewProjection.emplace_back(CachedLightViewMatrices[0] * CachedLightProjectionMatrix);
			//bIsLightVPDirty = false;
			return;
		}
		else if (WClip > 0.0f) // W > 0: 기존 PSM(후투영 공간) 경로
		{
			// 클립 → 후투영 정규화
			FVector LightInPostPerspective = FVector(LightInClipH.X, LightInClipH.Y, LightInClipH.Z);
			const float InverseW = 1.0f / WClip;
			LightInPostPerspective.X *= InverseW;
			LightInPostPerspective.Y *= InverseW;
			LightInPostPerspective.Z *= InverseW; // DX: [0,1]

			// 후투영 공간에서 MVL 구성 (라이트를 Eye에 두고 NDC 중심을 바라봄)
			const FVector Eye = LightInPostPerspective;
			const FVector Target(0.0f, 0.0f, 0.5f);
			FVector ForwardPP = Target - Eye;
			if (!ForwardPP.IsNearlyZero()) { ForwardPP.Normalize(); }
			FVector UpPP = FVector::UnitZ();
			const float ParallelCheckPP = std::abs(ForwardPP.Dot(UpPP));
			if (ParallelCheckPP > 0.999f) { UpPP = FVector::UnitY(); }
			FVector RightPP = UpPP.Cross(ForwardPP);
			if (!RightPP.IsNearlyZero()) { RightPP.Normalize(); }
			UpPP = ForwardPP.Cross(RightPP);
			if (!UpPP.IsNearlyZero()) { UpPP.Normalize(); }

			const FMatrix LightViewInPostPerspective = FMatrix::CreateViewFromAxes(
				Eye, RightPP, UpPP, ForwardPP);

			// NDC 유닛 큐브([-1,1]^2 x [0,1]) 코너들을 MVL 공간으로 보내 FOV/근원거리 계산
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

			for (int CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
			{
				const FVector4 CornerV = LightViewInPostPerspective.TransformHomogeneous(CubeCorners[CornerIndex]);

				const float Zv = (CornerV.W != 0.0f) ? (CornerV.Z / CornerV.W) : CornerV.Z;
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

			if (!(NearZPSM < FLT_MAX)) { NearZPSM = 0.01f; }
			NearZPSM = std::max(NearZPSM, 0.001f);
			FarZPSM = std::max(FarZPSM, NearZPSM + 0.01f);

			const float TanHalfFovMax = std::max(MaxTanHalfFovX, MaxTanHalfFovY);
			const float FovPSM = 2.0f * std::atan(TanHalfFovMax);
			const float AspectPSM = 1.0f;

			const FMatrix LightProjectionInPostPerspective =
				FMatrix::CreatePerspectiveFOV(FovPSM, AspectPSM, NearZPSM, FarZPSM);

			// 최종 View / Projection
			const FMatrix LightViewPSM = CameraViewMatrix * CameraProjectionMatrix * LightViewInPostPerspective;
			CachedLightViewMatrices.emplace_back(LightViewPSM);
			CachedLightProjectionMatrix = LightProjectionInPostPerspective;

			CachedLightViewProjection.emplace_back(CachedLightViewMatrices[0] * CachedLightProjectionMatrix);
			//bIsLightVPDirty = false;
			return;
		}
		else // W < 0: 역투영(P^-1) 경로 — 전투영(뷰 공간)에서 MVL 구성
		{
			// 전투영(뷰) 공간에서 라이트 뷰 구성
			const FVector EyePre = LightInView;
			const FVector TargetPre(0.0f, 0.0f, (CameraNearZ + CameraFarZ) * 0.5f);
			FVector ForwardPre = TargetPre - EyePre;
			if (!ForwardPre.IsNearlyZero()) { ForwardPre.Normalize(); }
			FVector UpPre = FVector::UnitZ();
			const float ParallelCheckPre = std::abs(ForwardPre.Dot(UpPre));
			if (ParallelCheckPre > 0.999f) { UpPre = FVector::UnitY(); }
			FVector RightPre = UpPre.Cross(ForwardPre);
			if (!RightPre.IsNearlyZero()) { RightPre.Normalize(); }
			UpPre = ForwardPre.Cross(RightPre);
			if (!UpPre.IsNearlyZero()) { UpPre.Normalize(); }

			const FMatrix LightViewInPreProjection = FMatrix::CreateViewFromAxes(
				EyePre, RightPre, UpPre, ForwardPre);

			// NDC 유닛 큐브 코너를 P^-1로 뷰 공간으로 보낸 뒤, MVL로 평가
			const FVector NdCCorners[8] =
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

			// P^-1은 위에서 InCameraInvConstants.Projection 으로부터 이미 계산한 CameraProjectionInverseMatrix 사용
			for (int CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
			{
				// 뷰 공간으로 확장
				const FVector4 CornerViewH =
					CameraProjectionInverseMatrix.TransformHomogeneous(NdCCorners[CornerIndex]);
				FVector CornerView = FVector(CornerViewH.X, CornerViewH.Y, CornerViewH.Z);
				if (CornerViewH.W != 0.0f)
				{
					const float InvWCorner = 1.0f / CornerViewH.W;
					CornerView.X *= InvWCorner;
					CornerView.Y *= InvWCorner;
					CornerView.Z *= InvWCorner;
				}

				// 전투영(뷰) 공간의 MVL로 전송
				const FVector4 CornerV = LightViewInPreProjection.TransformHomogeneous(CornerView);

				const float Zv = (CornerV.W != 0.0f) ? (CornerV.Z / CornerV.W) : CornerV.Z;
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

			if (!(NearZPSM < FLT_MAX)) { NearZPSM = 0.01f; }
			NearZPSM = std::max(NearZPSM, 0.001f);
			FarZPSM = std::max(FarZPSM, NearZPSM + 0.01f);

			const float TanHalfFovMax = std::max(MaxTanHalfFovX, MaxTanHalfFovY);
			const float FovPSM = 2.0f * std::atan(TanHalfFovMax);
			const float AspectPSM = 1.0f;

			const FMatrix LightProjectionPreProjection =
				FMatrix::CreatePerspectiveFOV(FovPSM, AspectPSM, NearZPSM, FarZPSM);

			// 최종 View / Projection (전투영: V · MVL)
			const FMatrix LightViewPre = CameraViewMatrix * LightViewInPreProjection;
			CachedLightViewMatrices.emplace_back(LightViewPre);
			CachedLightProjectionMatrix = LightProjectionPreProjection;

			CachedLightViewProjection.emplace_back(CachedLightViewMatrices[0] * CachedLightProjectionMatrix);
			//bIsLightVPDirty = false;
			return;
		}
	}
}
