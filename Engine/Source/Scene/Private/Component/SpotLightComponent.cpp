#include "pch.h"
#include "Scene/Public/Component/PointLightComponent.h"
#include "Scene/Public/Component/SpotLightComponent.h"
#include "Asset/Public/JsonSerializer.h"
#include "Editor/Public/Line/BatchLineManager.h"
#include "Editor/Public/UI/Widget/Component/SpotLightComponentWidget.h"
#include "Manager/Public/AssetManager.h"
#include "Renderer/Public/LightData.h"

IMPLEMENT_CLASS(USpotLightComponent, UPointLightComponent)

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

void USpotLightComponent::DrawDebugLines()
{
	auto& LineManager = UBatchLineManager::GetInstance();
    const FVector TipLocation = GetWorldLocation();          // 원뿔의 꼭짓점 (라이트 위치)
    const FVector Direction = GetWorldForwardVector();            // 라이트가 바라보는 방향
    const FVector RightVector = GetWorldRightVector();            // 원을 그릴 때 사용할 오른쪽 벡터
    const FVector UpVector = GetWorldUpVector();                  // 원을 그릴 때 사용할 위쪽 벡터

    const FVector4 OuterColor(1.0f, 1.0f, 0.0f, 1.0f);   // 외부 원뿔 (노란색)
    const FVector4 InnerColor(1.0f, 0.7f, 0.0f, 1.0f);   // 내부 원뿔 (주황색)

    // 1. 각도를 라디안으로 변환
    const float OuterAngleRad = ToRad * OuterConeAngle;
    const float InnerAngleRad = ToRad * InnerConeAngle;

    // 2. 원뿔 밑면의 반지름과 중심 위치 계산
    const float OuterRadius = GetAttenuationRadius() * tan(OuterAngleRad);
    const float InnerRadius = GetAttenuationRadius() * tan(InnerAngleRad);
    const FVector BaseCenter = TipLocation + Direction * GetAttenuationRadius();

    // 3. 밑면 원 그리기 (외부 원, 내부 원)
    // AddDebugCircle 함수를 라이트 방향에 맞게 수정하거나, 아래처럼 직접 구현합니다.
    const int32 Segments = 32;
    for (int32 i = 0; i < Segments; ++i)
    {
        const float Angle1 = static_cast<float>(i) / Segments * 2.0f * PI;
        const float Angle2 = static_cast<float>(i + 1) / Segments * 2.0f * PI;

        // 외부 원의 선분 계산
        FVector P1_Outer = BaseCenter + (RightVector * cos(Angle1) + UpVector * sin(Angle1)) * OuterRadius;
        FVector P2_Outer = BaseCenter + (RightVector * cos(Angle2) + UpVector * sin(Angle2)) * OuterRadius;

        // 내부 원의 선분 계산
        FVector P1_Inner = BaseCenter + (RightVector * cos(Angle1) + UpVector * sin(Angle1)) * InnerRadius;
        FVector P2_Inner = BaseCenter + (RightVector * cos(Angle2) + UpVector * sin(Angle2)) * InnerRadius;

        // 라인 추가
        const FName OuterLabel = FName(std::format("{}_OuterCircle_{}", GetName().ToString(), i));
        LineManager.AddDebugLine(OuterLabel, P1_Outer, P2_Outer, OuterColor);

    	const FName InnerLabel = FName(std::format("{}_InnerCircle_{}", GetName().ToString(), i));
        LineManager.AddDebugLine(InnerLabel, P1_Inner, P2_Inner, InnerColor);

    	DebugLineLabels.emplace_back(OuterLabel);
    	DebugLineLabels.emplace_back(InnerLabel);
    }

    // 4. 원뿔 꼭짓점과 밑면을 잇는 선 4개 그리기
    for (int32 i = 0; i < 20; ++i)
    {
        const float Angle = (float)i / 20.0f * 2.0f * PI;
        FVector EdgePoint = BaseCenter + (RightVector * cos(Angle) + UpVector * sin(Angle)) * OuterRadius;

        const FName EdgeLabel(std::format("{}_EdgeLine_{}", GetName().ToString(), i));
        LineManager.AddDebugLine(EdgeLabel, TipLocation, EdgePoint, OuterColor);
    	DebugLineLabels.emplace_back(EdgeLabel);
    }
}

void USpotLightComponent::SetInnerConeAngle(float InInnerConeAngle)
{
	InnerConeAngle = std::clamp(InInnerConeAngle, 0.0f, OuterConeAngle - 1.0f);
	if (bIsSelected)
	{
		ClearDebugLines();
		DrawDebugLines();
	}
}

void USpotLightComponent::SetOuterConeAngle(float InOuterConeAngle)
{
	OuterConeAngle = std::clamp(InOuterConeAngle, InnerConeAngle + 1.0f, 90.0f);
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
