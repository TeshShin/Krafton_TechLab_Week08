#include "pch.h"
#include "Editor/Public/UI/Widget/Component/LightComponentWidget.h"
#include "Editor/Public/Editor.h"
#include "Renderer/Public/ShadowMapManager.h"
#include "Scene/Public/Component/LightComponentBase.h"

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

            // Light Depth Map
            int32 ShadowMapIdx = LightComponent->GetShadowMapIdx();
            if (ShadowMapIdx > -1)
            {
            	ImGui::Text("Shadow Map Idx: %d", ShadowMapIdx);
            	switch (LightComponent->GetLightType())
            	{
            	case ELightComponentType::LightType_Spot:
            		ImGui::Image(FShadowMapManager::GetInstance().GetSpotSRVForImGuiDebug(ShadowMapIdx), ImVec2(512, 512));
            		break;
            	case ELightComponentType::LightType_Directional:
            		ImGui::Image(FShadowMapManager::GetInstance().GetDirectionalSRVForImGuiDebug(), ImVec2(512, 512));
            		break;
            	case ELightComponentType::LightType_Point:
            		for (uint32 Idx = 0; Idx < 6; ++Idx)
            		{
            			ImGui::Image(FShadowMapManager::GetInstance().GetPointSRVForImGuiDebug(ShadowMapIdx, Idx), ImVec2(512, 512));
            		}
            		break;
	            default: ;
            	}
            }

            ImGui::Separator();
        }
    }
}

