#include "pch.h"
#include "Editor/Public/UI/Widget/Component/LightComponentWidget.h"
#include "Editor/Public/Editor.h"
#include "Renderer/Public/ShadowMapManager.h"
#include "Scene/Public/Component/LightComponentBase.h"
#include "Renderer/Public/Renderer.h"
// 간단한 툴팁 헬퍼: 직전 위젯이 Hover 되면 설명 표시
static void ShowTooltipOnHover(const char* InText)
{
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", InText);
	}
}
IMPLEMENT_CLASS(ULightComponentWidget, UComponentWidget)

void ULightComponentWidget::Initialize()
{

}
void ULightComponentWidget::Update()
{
}
void ULightComponentWidget::RenderWidget()
{
    // 먼저 부모 클래스의 RenderWidget을 호출하여 UPROPERTY 자동 렌더링
    Super::RenderWidget();

    // 기존 커스텀 UI 유지
    ULevel* CurrentLevel = GWorld->GetLevel();
    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
        if (ULightComponentBase* LightComponent = Cast<ULightComponentBase>(NewSelectedComponent))
        {
            ImGui::Separator();

            // Visibility
            bool bVisible = LightComponent->IsVisible();
            if (ImGui::Checkbox("Visible", &bVisible))
            {
                LightComponent->SetVisible(bVisible);
            }

            // Light Color
            FVector LightColor = LightComponent->GetLightColor();
            if (ImGui::ColorEdit3("Light Color", &LightColor.X))
            {
                LightComponent->SetLightColor(LightColor);
            }

            // Intensity
            float Intensity = LightComponent->GetIntensity();
            if (ImGui::DragFloat("Intensity", &Intensity, 0.1f, 0.0f, 20.0f))
            {
                LightComponent->SetIntensity(Intensity);
            }

            // bCastShadows
            bool bCastShadows = LightComponent->DoesCastShadows();
            if (ImGui::Checkbox("Cast Shadows", &bCastShadows))
            {
				LightComponent->SetCastShadows(bCastShadows);
            }
			// Shadow Bias
			float ShadowBias = LightComponent->GetShadowBias();
			if (ImGui::DragFloat("Shadow Bias", &ShadowBias, 0.0001f, 0.0f, 0.01f, "%.5f"))
			{
				LightComponent->SetShadowBias(ShadowBias);
			}
			ShowTooltipOnHover(
				"Shadow Bias: 깊이 테스트를 미세하게 오프셋합니다.\n"
				"- 낮추면 셰도우 아크네(자기 음영 점무늬) 감소가 약함\n"
				"- 높이면 페터 패닝(떠 보임) 발생\n"
				"권장: 0.0005 ~ 0.005"
			);
			ImGui::Text("Shadow Rasterizer (Global)");

			URenderer& Renderer = URenderer::GetInstance();

			float SlopeScaled = Renderer.GetShadowSlopeScaledDepthBias();
			if (ImGui::DragFloat("Slope Scaled Depth Bias", &SlopeScaled, 0.05f, 0.0f, 5.0f, "%.2f"))
			{
				Renderer.SetShadowSlopeScaledDepthBias(SlopeScaled);
			}
			ShowTooltipOnHover(
				"폴리곤 기울기에 비례해 깊이를 오프셋합니다.\n"
				"- 높일수록 아크네 감소, 페터 패닝 증가\n"
				"권장 시작값: 1.0 ~ 2.0, 기본 1.5"
			);

			float Clamp = Renderer.GetShadowDepthBiasClamp();
			if (ImGui::DragFloat("Depth Bias Clamp", &Clamp, 0.001f, 0.0f, 0.1f, "%.3f"))
			{
				Renderer.SetShadowDepthBiasClamp(Clamp);
			}
			ShowTooltipOnHover(
				"바이어스 상한. 급경사에서 과도한 오프셋을 제한합니다.\n"
				"0.0 ~ 0.02 범위로 필요 시만, 기본 - 0.0"
			);

			int DepthBias = Renderer.GetShadowDepthBias();
			if (ImGui::DragInt("Depth Bias", &DepthBias, 1, -100, 100))
			{
				Renderer.SetShadowDepthBias(DepthBias);
			}
			ShowTooltipOnHover(
				"상수 바이어스. D32_FLOAT에서는 0 유지 권장\n"
				"필요 시 수십정도만 조정."
			);
            // Light Depth Map
            int32 ShadowMapIdx = LightComponent->GetShadowMapIdx();
            if (ShadowMapIdx > -1)
            {
            	ImGui::Text("Shadow Map Idx: %d", ShadowMapIdx);
            	ImGui::Image(FShadowMapManager::GetInstance().GetSRVForImGuiDebug(ShadowMapIdx), ImVec2(512, 512));
            }

            ImGui::Separator();
        }
    }
}

