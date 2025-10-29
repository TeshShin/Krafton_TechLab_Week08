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
	FShadowSettings ShadowSettings = ShadowMapManager.GetShadowSettings();

	if (ImGui::CollapsingHeader("Shadow Settings"))
    {
        const char* filterNames[] = {
            "None (No Shadows)",
            "PCF (Hard)",
            "VSM (Linear)",
            "VSM (Box Filter)",
            "VSM (Gaussian Filter)"
        };

        int CurrentFilterType = static_cast<int>(ShadowSettings.FilterType);
        if (ImGui::Combo("Filter Type", &CurrentFilterType, filterNames, IM_ARRAYSIZE(filterNames)))
        {
            ShadowSettings.FilterType = static_cast<EShadowFilterType>(CurrentFilterType);
        }

        // --- PCF 설정 ---
        if (ShadowSettings.FilterType == EShadowFilterType::SFT_PCF)
        {
            ImGui::Separator();
            ImGui::Text("PCF Quality Range");
            ImGui::TextDisabled("Set the Min(Sharp) and Max(Soft) kernel size.");

            ImGui::DragInt("Min PCF Size (Sharp)", &ShadowSettings.MinPCFSize, 2.0f, 1, 49);
            ImGui::DragInt("Max PCF Size (Soft)", &ShadowSettings.MaxPCFSize, 2.0f, 1, 49);

            if (ShadowSettings.MinPCFSize % 2 == 0) ShadowSettings.MinPCFSize += 1;
            if (ShadowSettings.MaxPCFSize % 2 == 0) ShadowSettings.MaxPCFSize += 1;

            ShadowSettings.MinPCFSize = std::min(ShadowSettings.MinPCFSize, ShadowSettings.MaxPCFSize);
            ShadowSettings.MaxPCFSize = std::max(ShadowSettings.MinPCFSize, ShadowSettings.MaxPCFSize);
        }

        // --- VSM 공통 설정 ---
        if (ShadowSettings.FilterType >= EShadowFilterType::SFT_VSM)
        {
            ImGui::Separator();
            ImGui::Text("VSM Settings");

            ImGui::SliderFloat("Light Bleed Reduction", &ShadowSettings.VSM_LightBleedReduction, 0.0f, 1.0f);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Reduces light bleeding artifacts common in VSM.");

            // --- VSM Box ---
            if (ShadowSettings.FilterType == EShadowFilterType::SFT_VSM_Box)
            {
                ImGui::Separator();
                ImGui::Text("VSM Box Quality Range");
                ImGui::TextDisabled("Set the Min(Sharp) and Max(Soft) kernel size.");

                // ⭐️ 변경점: Min/Max 값을 직접 수정합니다.
                ImGui::DragInt("Min Box Size (Sharp)", &ShadowSettings.MinBoxSize, 2.0f, 1, 49);
                ImGui::DragInt("Max Box Size (Soft)", &ShadowSettings.MaxBoxSize, 2.0f, 1, 49);

                if (ShadowSettings.MinBoxSize % 2 == 0) ShadowSettings.MinBoxSize += 1;
                if (ShadowSettings.MaxBoxSize % 2 == 0) ShadowSettings.MaxBoxSize += 1;

                ShadowSettings.MinBoxSize = std::min(ShadowSettings.MinBoxSize, ShadowSettings.MaxBoxSize);
                ShadowSettings.MaxBoxSize = std::max(ShadowSettings.MinBoxSize, ShadowSettings.MaxBoxSize);
            }
            // --- VSM Gaussian ---
            else if (ShadowSettings.FilterType == EShadowFilterType::SFT_VSM_Gaussian)
            {
                ImGui::Separator();
                ImGui::Text("VSM Gaussian Quality Range");
                ImGui::TextDisabled("Set the Min(Sharp) and Max(Soft) radius/sigma.");

                ImGui::SliderInt("Min Radius (Sharp)", &ShadowSettings.MinGaussRadius, 1, 20);
                ImGui::SliderInt("Max Radius (Soft)", &ShadowSettings.MaxGaussRadius, 1, 20);
                ShadowSettings.MinGaussRadius = std::min(ShadowSettings.MinGaussRadius, ShadowSettings.MaxGaussRadius);
                ShadowSettings.MaxGaussRadius = std::max(ShadowSettings.MinGaussRadius, ShadowSettings.MaxGaussRadius);

                ImGui::SliderFloat("Min Sigma (Sharp)", &ShadowSettings.MinGaussSigma, 0.1f, 10.0f);
                ImGui::SliderFloat("Max Sigma (Soft)", &ShadowSettings.MaxGaussSigma, 0.1f, 10.0f);
                ShadowSettings.MinGaussSigma = std::min(ShadowSettings.MinGaussSigma, ShadowSettings.MaxGaussSigma);
                ShadowSettings.MaxGaussSigma = std::max(ShadowSettings.MinGaussSigma, ShadowSettings.MaxGaussSigma);
            }
        }
    }

	ShadowMapManager.UpdateShadowSettings(ShadowSettings);
}
