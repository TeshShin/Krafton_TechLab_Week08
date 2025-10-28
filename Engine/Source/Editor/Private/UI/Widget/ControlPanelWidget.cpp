#include "pch.h"
#include "Editor/Public/UI/Widget/ControlPanelWidget.h"
#include "Renderer/Public/ShadowMapManager.h"

IMPLEMENT_CLASS(UControlPanelWidget, UWidget)

void UControlPanelWidget::Initialize()
{
}

void UControlPanelWidget::Update()
{
}

void UControlPanelWidget::RenderWidget()
{
	// Shadow
	FShadowMapManager& ShadowMapManager = FShadowMapManager::GetInstance();
	EShadowFilterType ShadowFilterType = ShadowMapManager.GetFilterType();
	FShadowSettings ShadowSettings = ShadowMapManager.GetShadowSettings();

    if (ImGui::CollapsingHeader("Shadow Settings"))
    {
        // 1. 섀도우 필터 타입 선택 (Combo Box)
        const char* filterNames[] = {
            "None (No Shadows)",
            "PCF (Hard)",
            "VSM (Linear)",
            "VSM (Box Filter)",
            "VSM (Gaussian Filter)"
        };

        // ImGui::Combo는 int*를 사용하므로 enum을 int로 변환했다가 다시 돌려받습니다.
        int CurrentFilterType = static_cast<int>(ShadowFilterType);
        if (ImGui::Combo("Filter Type", &CurrentFilterType, filterNames, IM_ARRAYSIZE(filterNames)))
        {
            ShadowFilterType = static_cast<EShadowFilterType>(CurrentFilterType);
        	ShadowSettings.FilterType = CurrentFilterType;
        }

        // 2. 필터 타입에 따른 조건부 UI

        // --- PCF 설정 ---
        if (ShadowFilterType == EShadowFilterType::SFT_PCF)
        {
            ImGui::Separator();
            ImGui::Text("PCF Settings");

            // PCF_FilterSize가 커널의 한 변의 크기(width)라고 가정 (예: 3 -> 3x3)
            // 홀수만 선택하도록 DragInt를 사용 (speed = 2)
            ImGui::DragInt("PCF Filter Size", &ShadowSettings.PCF_FilterSize, 2.0f, 1, 15);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Kernel size (e.g., 1 = 1x1, 3 = 3x3, 5 = 5x5). \nMust be an odd number.");

            if (ShadowSettings.PCF_FilterSize % 2 == 0) {
                ShadowSettings.PCF_FilterSize += 1;
            }
            ShadowSettings.PCF_FilterSize = max(ShadowSettings.PCF_FilterSize, 1);
        }

        // --- VSM 설정 ---
        // VSM(Linear), VSM_Box, VSM_Gaussian 모두에 공통적인 설정
        if (ShadowFilterType >= EShadowFilterType::SFT_VSM)
        {
            ImGui::Separator();
            ImGui::Text("VSM Settings");

            ImGui::SliderFloat("Light Bleed Reduction", &ShadowSettings.VSM_LightBleedReduction, 0.0f, 1.0f);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Reduces light bleeding artifacts common in VSM.");

            // Box 필터 전용 설정
            if (ShadowFilterType == EShadowFilterType::SFT_VSM_Box)
            {
                ImGui::DragInt("Box Filter Size", &ShadowSettings.VSM_BoxFilterSize, 2.0f, 1, 35);
                 if (ShadowSettings.VSM_BoxFilterSize % 2 == 0) {
                    ShadowSettings.VSM_BoxFilterSize += 1;
                }
            }
            // Gaussian 필터 전용 설정
            else if (ShadowFilterType == EShadowFilterType::SFT_VSM_Gaussian)
            {
                ImGui::SliderInt("Gaussian Radius", &ShadowSettings.VSM_GaussianKernelRadius, 1, 20);
                ImGui::SliderFloat("Gaussian Sigma", &ShadowSettings.VSM_GaussianSigma, 0.1f, 10.0f);
            }
        }
    }

	ShadowMapManager.UpdateShadowSettings(ShadowSettings);
	ShadowMapManager.UpdateFilterType(ShadowFilterType);
}
