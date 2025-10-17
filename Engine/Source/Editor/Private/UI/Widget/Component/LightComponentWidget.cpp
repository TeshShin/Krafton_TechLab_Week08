#include "pch.h"
#include "Editor/Public/UI/Widget/Component/LightComponentWidget.h"
#include "Editor/Public/Editor.h"
#include "Scene/Public/Component/LightComponentBase.h"

IMPLEMENT_CLASS(ULightComponentWidget, UWidget)

void ULightComponentWidget::Initialize()
{
    
}
void ULightComponentWidget::Update()
{
}
void ULightComponentWidget::RenderWidget()
{
    ULevel* CurrentLevel = GWorld->GetLevel();
    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
        if (ULightComponentBase* LightComponent = Cast<ULightComponentBase>(NewSelectedComponent))
        {
            ImGui::Separator();

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

            ImGui::Separator();
        }
    }
}

