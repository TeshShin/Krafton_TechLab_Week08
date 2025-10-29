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
    		ELightComponentType LightType = LightComponent->GetLightType();
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

    		// bCastShadows (Spot/Point/Directional)
    		if (LightType == ELightComponentType::LightType_Spot || LightType == ELightComponentType::LightType_Point
    			|| LightType == ELightComponentType::LightType_Directional)
    		{
    			bool bCastShadows = LightComponent->DoesCastShadows();
    			if (ImGui::Checkbox("Cast Shadows", &bCastShadows))
    			{
    				LightComponent->SetCastShadows(bCastShadows);
    			}

    			float ShadowSharpen = LightComponent->GetShadowSharpen();
    			if (ImGui::DragFloat("Shadow Sharpen", &ShadowSharpen, 0.01f, 0.f, 1.0f))
    			{
    				LightComponent->SetShadowSharpen(ShadowSharpen);
    			}

    			float ShadowResolutionScale = LightComponent->GetShadowResolutionScale();
    			if (ImGui::DragFloat("Shadow Resolution Scale", &ShadowResolutionScale, 0.01f, 0.f, 1.0f))
    			{
    				LightComponent->SetShadowResolutionScale(ShadowResolutionScale);
    			}

    			float ShadowBias = LightComponent->GetShadowBias();
    			if (ImGui::DragFloat("Shadow Bias", &ShadowBias, 0.001f, 0.f, 1.0f))
    			{
    				LightComponent->SetShadowBias(ShadowBias);
    			}

    			float ShadowSlopeBias = LightComponent->GetShadowSlopeBias();
    			if (ImGui::DragFloat("Shadow Slope Bias", &ShadowSlopeBias, 0.001f, 0.f, 1.0f))
    			{
    				LightComponent->SetShadowSlopeBias(ShadowSlopeBias);
    			}
    		}
			// Shadow Projection Mode Toggle (Spot/Directional 지원)
			if (LightType == ELightComponentType::LightType_Spot ||
				LightType == ELightComponentType::LightType_Directional)
			{
				int CurrentMode = static_cast<int>(LightComponent->GetShadowProjectionMode());

				if (LightType == ELightComponentType::LightType_Directional)
				{
					const char* ModeItems[] = { "LVP", "CSM" };
					if (ImGui::Combo("Shadow Projection", &CurrentMode, ModeItems, IM_ARRAYSIZE(ModeItems)))
					{
						EShadowProjectionMode ShadowProjectionMode = CurrentMode == 0 ? EShadowProjectionMode::LVP : EShadowProjectionMode::CSM;
						LightComponent->SetShadowProjectionMode(ShadowProjectionMode);
					}
				}
				else
				{
					const char* ModeItems[] = { "LVP", "PSM" };
					if (ImGui::Combo("Shadow Projection", &CurrentMode, ModeItems, IM_ARRAYSIZE(ModeItems)))
					{
						EShadowProjectionMode ShadowProjectionMode = CurrentMode == 0 ? EShadowProjectionMode::LVP : EShadowProjectionMode::PSM;
						LightComponent->SetShadowProjectionMode(ShadowProjectionMode);
					}
				}
			}
    		// Light Depth Map
    		int32 ShadowMapIdx = LightComponent->GetShadowMapIdx();
    		if (ShadowMapIdx > -1)
    		{
    			ImGui::Text("Shadow Map Idx: %d", ShadowMapIdx);
    			switch (LightType)
    			{
    			case ELightComponentType::LightType_Spot:
    				ImGui::Image(FShadowMapManager::GetInstance().GetSpotSRVForImGuiDebug(ShadowMapIdx), ImVec2(256, 256));
    				break;
    			case ELightComponentType::LightType_Directional:
					if (LightComponent->GetShadowProjectionMode() == EShadowProjectionMode::CSM)
					{
						auto& ShadowMapManager = FShadowMapManager::GetInstance();

						// 썸네일 옵션(원하면 UPROPERTY/설정으로 빼도 됨)
						const float Thumb = 256.0f;
						const int   Cols = 2;

						if (ImGui::CollapsingHeader("CSM Depth Slices", ImGuiTreeNodeFlags_DefaultOpen))
						{
							const uint32 CascadesNum   = ShadowMapManager.GetDirectionalMaxNumCascades();
							int cascadesEditable = static_cast<int>(LightComponent->GetNumOfCascade());
							if (ImGui::SliderInt("Num Cascades", &cascadesEditable, 1, static_cast<int>(CascadesNum)))
							{
								LightComponent->SetNumOfCascade(static_cast<uint32>(cascadesEditable));
							}

							float lambda = LightComponent->GetCascadeSplitLambda();
							if (ImGui::SliderFloat("Split Lambda", &lambda, 0.0f, 1.0f))
							{
								LightComponent->SetCascadeSplitLambda(lambda);
							}

							const uint32 LightCascacdesNum = LightComponent->GetNumOfCascade();
							const uint32 DebugCascades = (LightCascacdesNum < CascadesNum) ? LightCascacdesNum : CascadesNum;
							ImGui::Text("Cascades Preview: %u", DebugCascades);
							for (uint32 i = 0; i < DebugCascades; ++i)
							{
								ID3D11ShaderResourceView* srv = ShadowMapManager.GetDirectionalSRVForImGuiDebug(i);
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
						ID3D11ShaderResourceView* SRV = FShadowMapManager::GetInstance().GetDirectionalSRVForImGuiDebug(0);
						if (SRV)
						{
							ImGui::BeginGroup();
							ImGui::Image((ImTextureID)SRV, ImVec2(256, 256));
							ImGui::EndGroup();
						}
					}
					break;
    			case ELightComponentType::LightType_Point:
    				FShadowMapManager::GetInstance().UpdatePointShadowDebugTextures(ShadowMapIdx);
    				for (uint32 Idx = 0; Idx < 6; ++Idx)
    				{
    					ImGui::Image(FShadowMapManager::GetInstance().GetPointSRVForImGuiDebug(ShadowMapIdx, Idx), ImVec2(256, 256));
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

