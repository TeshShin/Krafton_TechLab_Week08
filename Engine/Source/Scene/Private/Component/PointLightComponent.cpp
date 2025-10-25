#include "pch.h"
#include "Scene/Public/Component/PointLightComponent.h"
#include "Asset/Public/JsonSerializer.h"
#include "Editor/Public/Line/BatchLineManager.h"
#include "Editor/Public/UI/Widget/Component/PointLightComponentWidget.h"
#include "Manager/Public/AssetManager.h"
#include "Renderer/Public/LightData.h"

IMPLEMENT_CLASS(UPointLightComponent, ULightComponentBase)

UPointLightComponent::UPointLightComponent()
{
	bCastShadows = true;
	CachedLightViewProjection.reserve(6);
}

void UPointLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "LightFalloffExponent", LightFalloffExponent);
		FJsonSerializer::ReadFloat(InOutHandle, "AttenuationRadius", AttenuationRadius);
	}
	else
	{
		InOutHandle["LightFalloffExponent"] = LightFalloffExponent;
		InOutHandle["AttenuationRadius"] = AttenuationRadius;
	}
}

UObject* UPointLightComponent::Duplicate()
{
	UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>(Super::Duplicate());
	PointLightComponent->LightFalloffExponent = LightFalloffExponent;
	PointLightComponent->AttenuationRadius = AttenuationRadius;

	return PointLightComponent;
}

void UPointLightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

UClass* UPointLightComponent::GetSpecificWidgetClass() const
{
    return UPointLightComponentWidget::StaticClass();
}

void UPointLightComponent::DrawDebugLines()
{
	auto& LineManager = UBatchLineManager::GetInstance();
	const FVector Center = GetWorldLocation();
	const FVector4 Color(1.0f, 1.0f, 0.0f, 1.0f);
	const FName BaseLabel(GetName());

	LineManager.AddDebugCircle(BaseLabel, Center, AttenuationRadius, Color, DebugLineLabels);
}

FUnifiedDynamicLight UPointLightComponent::GetUnifiedLightData() const
{
    FUnifiedDynamicLight LightData = {};

    LightData.Position = GetWorldLocation();
    LightData.Intensity = GetIntensity();
    LightData.Color = GetLightColor();
	LightData.AttenuationRadius = GetAttenuationRadius();
    LightData.FalloffExponent = GetLightFalloffExponent();
    LightData.LightType = static_cast<uint32>(EDynamicLightType::Point);
	LightData.ShadowBias = 0.05f;
	LightData.bCastShadows = bCastShadows;
	LightData.ShadowMapIndex = ShadowMapIdx;

    return LightData;
}

void UPointLightComponent::SetAttenuationRadius(float InAttenuationRadius)
{
	AttenuationRadius = InAttenuationRadius;

	if (bIsSelected)
	{
		ClearDebugLines();
		DrawDebugLines();
	}
}

UTexture* UPointLightComponent::GetLightBillboardTexture()
{
	return UAssetManager::GetInstance().LoadTexture("Data/Icons/PointLight_64x.png");
}

const TArray<FMatrix>& UPointLightComponent::GetLightViewProjectionMatrices() const
{
	if (bIsLightVPDirty)
    {
        CachedLightViewProjection.clear();

        float NearZ = 0.1f;
        float FarZ = GetAttenuationRadius();
        float FOV = 90.0f * ToRad;
        float AspectRatio = 1.0f;
        FMatrix ProjMatrix = FMatrix::CreatePerspectiveFOV(FOV, AspectRatio, NearZ, FarZ);

		const FVector LightPosition = GetWorldLocation();
		const FVector Right = FVector::RightVector();
		const FVector Up = FVector::UpVector();
		const FVector Forward = FVector::ForwardVector();

        FMatrix ViewMatrices[6];
		// D3D Slice 0: (+X) = Unreal +Y (Right)
		ViewMatrices[0] = FMatrix::CreateViewFromAxes(LightPosition, -Forward, Up, Right);

		// D3D Slice 1: (-X) = Unreal -Y (Left)
		ViewMatrices[1] = FMatrix::CreateViewFromAxes(LightPosition, Forward, Up, -Right);

		// D3D Slice 2: (+Y) = Unreal +Z (Up)
		ViewMatrices[2] = FMatrix::CreateViewFromAxes(LightPosition, Right, -Forward, Up);

		// D3D Slice 3: (-Y) = Unreal -Z (Down)
		ViewMatrices[3] = FMatrix::CreateViewFromAxes(LightPosition, Right, Forward, -Up);

		// D3D Slice 4: (+Z) = Unreal +X (Forward)
		ViewMatrices[4] = FMatrix::CreateViewFromAxes(LightPosition, Right, Up, Forward);

		// D3D Slice 5: (-Z) = Unreal -X (Backward)
		ViewMatrices[5] = FMatrix::CreateViewFromAxes(LightPosition, -Right, Up, -Forward);

        for (const auto& ViewMatrix : ViewMatrices)
        {
            CachedLightViewProjection.emplace_back(ViewMatrix * ProjMatrix);
        }

        bIsLightVPDirty = false;
    }

    return CachedLightViewProjection;
}
