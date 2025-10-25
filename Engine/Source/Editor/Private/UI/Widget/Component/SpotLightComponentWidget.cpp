#include "pch.h"
#include "Editor/Public/UI/Widget/Component/SpotLightComponentWidget.h"
#include "Scene/Public/Component/SpotLightComponent.h"
#include "Editor/Public/Editor.h"
#include "Scene/Public/Level/Level.h"
#include "Scene/Public/Component/ActorComponent.h"
#include "ImGui/imgui.h"
// 간단한 툴팁 헬퍼: 직전 위젯이 Hover 되면 설명 표시
static void ShowTooltipOnHover(const char* InText)
{
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", InText);
	}
}
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
	//RenderProperties(SpotLightComponent, true);

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
	bool bUsePSM = SpotLightComponent->IsUsingPSM();
	if (ImGui::Checkbox("Use PSM (Perspective Shadow Map)", &bUsePSM))
	{
		SpotLightComponent->SetUsePSM(bUsePSM);
	}
	ShowTooltipOnHover(
		"PSM: 카메라 절두체에 맞춰 라이트 VP를 타이트하게 계산해\n"
		"섀도우 해상도를 집중시킵니다. 빠른 카메라 이동에서의 안정성,\n"
		"클리핑과 해상도 낭비 사이의 트레이드오프가 있습니다."
	);
	if (bUsePSM)
	{
		float PSMFovScale = SpotLightComponent->GetPSMFovScale();
		if (ImGui::DragFloat("PSM FOV Scale", &PSMFovScale, 0.01f, 0.5f, 1.5f, "%.2f"))
		{
			SpotLightComponent->SetPSMFovScale(PSMFovScale);
		}
		ShowTooltipOnHover(
			"PSM FOV Scale: 카메라 기반 최소 FOV를 확대/축소합니다.\n"
			"- 1.0: 기본\n"
			"- >1.0: 여유 확대(깜빡임/클리핑 완화, 해상도 분산)\n"
			"- <1.0: 더 타이트(해상도 집중, 클리핑 위험)"
		);
		float PSMNearOffset = SpotLightComponent->GetPSMNearOffset();
		if (ImGui::DragFloat("PSM Near Offset", &PSMNearOffset, 0.1f, -10.0f, 10.0f, "%.2f"))
		{
			SpotLightComponent->SetPSMNearOffset(PSMNearOffset);
		}
		ShowTooltipOnHover(
			"PSM Near Offset: 근평면을 이동합니다.\n"
			"- 양수: 더 멀게(근거리 클리핑 증가)\n"
			"- 음수: 더 가깝게(정밀도 증가, 아크네 완화 가능)\n"
			"과도한 값은 근거리 셰도우가 잘릴 수 있습니다."
		);
		float PSMFarOffset = SpotLightComponent->GetPSMFarOffset();
		if (ImGui::DragFloat("PSM Far Offset", &PSMFarOffset, 0.1f, -10.0f, 10.0f, "%.2f"))
		{
			SpotLightComponent->SetPSMFarOffset(PSMFarOffset);
		}
		ShowTooltipOnHover(
			"PSM Far Offset: 원평면을 이동합니다.\n"
			"- 양수: 더 멀게(정밀도 감소 → 밴딩/깜빡임 위험)\n"
			"- 음수: 더 가깝게(정밀도 증가, 원거리 클리핑 가능)"
		);
	}
    ImGui::Separator();

}
