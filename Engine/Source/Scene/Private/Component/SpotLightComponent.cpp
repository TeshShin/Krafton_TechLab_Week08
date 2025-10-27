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
#include "Renderer/Public/Shadow/PSMBuilder.h"

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
	if (!URenderer::GetInstance().GetUseSpotLightPSM())
	{
		const float Aspect = 1.0f;
		const float FovRad = GetOuterConeAngle() * 2.0f * ToRad;
		const float NearZ = 0.1f;
		const float FarZ = GetAttenuationRadius();
		CachedLightViewProjection = LightView * FMatrix::CreatePerspectiveFOV(FovRad, Aspect, NearZ, FarZ);
		// bIsLightVPDirty = false;
		return CachedLightViewProjection;
	}
	// 2) 활성 카메라
	UCamera* ActiveCamera = nullptr;
	if (URenderer::GetInstance().GetViewportClient())
	{
		ActiveCamera = URenderer::GetInstance().GetViewportClient()->GetActiveCamera();
	}
	// 카메라가 없으면 활성 뷰포트가 없다는 뜻이므로, 뷰포트 목록에서 폴백 카메라를 선택
	if (!ActiveCamera)
	{
		FViewport* ViewportClient = URenderer::GetInstance().GetViewportClient();
		if (ViewportClient)
		{
			TArray<FViewportClient>& Clients = ViewportClient->GetViewports();

			// 1) 원근 카메라 우선
			for (FViewportClient& Client : Clients)
			{
				if (Client.GetCameraType() == EViewportCameraType::Perspective)
				{
					ActiveCamera = &Client.Camera;
					break;
				}
			}
			// 2) 그래도 없으면 첫 번째 카메라
			if (!ActiveCamera && Clients.size() > 0)
			{
				ActiveCamera = &Clients[0].Camera;
			}
		}

	}

	// 카메라가 없으면 유니폼(기존 스팟)으로 폴백
	if (!ActiveCamera)
	{
		float Aspect = 1.0f;
		float FovRad = GetOuterConeAngle() * 2.0f * ToRad;
		float NearZ = 0.1f;
		float FarZ = GetAttenuationRadius();
		CachedLightViewProjection = LightView * FMatrix::CreatePerspectiveFOV(FovRad, Aspect, NearZ, FarZ);
		// bIsLightVPDirty = false;
		return CachedLightViewProjection;
	}

	FMatrix Out;

	const bool bBuilt = FPSMBuilder::BuildSpotLightPSM(
		LightPosition, Right, Up, Forward,
		GetOuterConeAngle(), GetAttenuationRadius(),
		Out);

	if (bBuilt)
	{
		CachedLightViewProjection = Out;
		// bIsLightVPDirty = false;
		return CachedLightViewProjection;
	}
	else // 실패 시 표준 스팟 투영 폴백
	{
		const float Aspect = 1.0f;
		const float FovRad = GetOuterConeAngle() * 2.0f * ToRad;
		const float NearZ = 0.1f;
		const float FarZ = GetAttenuationRadius();
		CachedLightViewProjection = (FMatrix::TranslationMatrixInverse(LightPosition) *
			FMatrix(Right, Up, Forward).Transpose()) *
			FMatrix::CreatePerspectiveFOV(FovRad, Aspect, NearZ, FarZ);
		// bIsLightVPDirty = false;
		return CachedLightViewProjection;
	}
}
