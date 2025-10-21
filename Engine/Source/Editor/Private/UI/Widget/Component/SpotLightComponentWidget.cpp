#include "pch.h"
#include "Editor/Public/UI/Widget/Component/SpotLightComponentWidget.h"
#include "Scene/Public/Component/SpotLightComponent.h"
#include "Editor/Public/Editor.h"
#include "Scene/Public/Level/Level.h"
#include "Scene/Public/Component/ActorComponent.h"
#include "ImGui/imgui.h"

IMPLEMENT_CLASS(USpotLightComponentWidget, UPointLightComponentWidget)

void USpotLightComponentWidget::Initialize()
{
	Super::Initialize();
}

void USpotLightComponentWidget::Update()
{
	Super::Update();
	SpotLightComponent = Cast<USpotLightComponent>(PointLightComponent);
}

void USpotLightComponentWidget::RenderWidget()
{
	if (!SpotLightComponent)
	{
		return;
	}

	// UPROPERTY 시스템을 사용한 자동 UI 생성 (UWidget의 기본 구현 사용)
	RenderProperties(SpotLightComponent, true);

	Super::RenderWidget();

	float InnerConeAngle = SpotLightComponent->GetInnerConeAngle();
	if (ImGui::DragFloat("Inner Cone Angle", &InnerConeAngle, 0.1f, 0.0f, 90.0f))
	{
		SpotLightComponent->SetInnerConeAngle(InnerConeAngle);
	}

	float OuterConeAngle = SpotLightComponent->GetOuterConeAngle();
	if (ImGui::DragFloat("Outer Cone Angle", &OuterConeAngle, 0.1f, 0.0f, 90.0f))
	{
		SpotLightComponent->SetOuterConeAngle(OuterConeAngle);
	}

    ImGui::Separator();
}
