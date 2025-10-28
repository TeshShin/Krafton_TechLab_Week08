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
	if (!bIsLightVPDirty) { return; }

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
		bIsLightVPDirty = false;
		return;
	}
	else if (GetShadowProjectionMode() == EShadowProjectionMode::PSM)
	{
		// TODO : PSM 구현
	}
}
