#include "pch.h"
#include "Editor/Public/UI/Widget/Component/LightComponentWidget.h"
#include "Editor/Public/Editor.h"
#include "Renderer/Public/Renderer.h"
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
			// Shadow Projection Mode Toggle (Spot/Directional 지원)
			if (LightComponent->GetLightType() == ELightComponentType::LightType_Spot ||
				LightComponent->GetLightType() == ELightComponentType::LightType_Directional)
			{
				int CurrentMode = static_cast<int>(LightComponent->GetShadowProjectionMode());

				if (LightComponent->GetLightType() == ELightComponentType::LightType_Directional)
				{
					const char* ModeItems[] = { "LVP", "PSM", "LiSPSM (TODO)", "CSM"};
					if (ImGui::Combo("Shadow Projection", &CurrentMode, ModeItems, IM_ARRAYSIZE(ModeItems)))
					{
						LightComponent->SetShadowProjectionMode(static_cast<EShadowProjectionMode>(CurrentMode));
					}
				}
				else
				{
					const char* ModeItems[] = { "LVP", "PSM" };
					if (ImGui::Combo("Shadow Projection", &CurrentMode, ModeItems, IM_ARRAYSIZE(ModeItems)))
					{
						LightComponent->SetShadowProjectionMode(static_cast<EShadowProjectionMode>(CurrentMode));
					}
				}
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
					if (LightComponent->GetShadowProjectionMode() == EShadowProjectionMode::CSM)
					{
						auto& SMM = FShadowMapManager::GetInstance();

						// 썸네일 옵션(원하면 UPROPERTY/설정으로 빼도 됨)
						const float Thumb = 256.0f;
						const int   Cols = 2;

						if (ImGui::CollapsingHeader("CSM Depth Slices", ImGuiTreeNodeFlags_DefaultOpen))
						{
							ImGui::Text("Cascades: %u", (uint32)SMM.GetDirectionalNumCascades());
							for (uint32 i = 0; i < (uint32)SMM.GetDirectionalNumCascades(); ++i)
							{
								ID3D11ShaderResourceView* srv = SMM.GetDirectionalSRVForImGuiDebug(i);
								if (srv)
								{
									ImGui::BeginGroup();
									ImGui::Text("Cascade %u", i);
									ImGui::Image((ImTextureID)srv, ImVec2(Thumb, Thumb));
									ImGui::EndGroup();
								}
								if ((i % Cols) != (Cols - 1)) ImGui::SameLine();
							}

						}
					}
					else
					{ 
						ImGui::Image(FShadowMapManager::GetInstance().GetDirectionalSRVForImGuiDebug(), ImVec2(512, 512));
					}
					break;
    			case ELightComponentType::LightType_Point:
    				FShadowMapManager::GetInstance().UpdatePointShadowDebugTextures(ShadowMapIdx);
    				for (uint32 Idx = 0; Idx < 6; ++Idx)
    				{
    					ImGui::Image(FShadowMapManager::GetInstance().GetPointSRVForImGuiDebug(ShadowMapIdx, Idx), ImVec2(512, 512));
    				}
    				break;
    			default: ;
    			}
    		}

    		// Override Camera
    		if (FViewport* ViewportClient = URenderer::GetInstance().GetViewportClient())
    		{
    			bool bIsThisLightBeingPiloted = ViewportClient->IsPilotingComponent(LightComponent);
    			if (ImGui::Checkbox("Override Camera With Light's Perspective", &bIsThisLightBeingPiloted))
    			{
    				if (bIsThisLightBeingPiloted == true)
    				{
    					ViewportClient->StartPiloting(LightComponent);
    				}
    				else
    				{
    					ViewportClient->StopPiloting();
    				}
    			}
    		}

    		ImGui::Separator();
    	}
    }
}

