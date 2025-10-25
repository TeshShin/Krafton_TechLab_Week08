#include "pch.h"
#include "Editor/Public/UI/Widget/Component/PointLightComponentWidget.h"
#include "Scene/Public/Component/PointLightComponent.h"
#include "Editor/Public/Editor.h"
#include "Editor/Public/UI/Widget/Component/ComponentWidget.h"
#include "Scene/Public/Level/Level.h"
#include "Scene/Public/Component/ActorComponent.h"
#include "ImGui/imgui.h"

IMPLEMENT_CLASS(UPointLightComponentWidget, ULightComponentWidget)

void UPointLightComponentWidget::Initialize()
{
}

void UPointLightComponentWidget::Update()
{
    ULevel* CurrentLevel = GWorld->GetLevel();
    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
        if (PointLightComponent != NewSelectedComponent)
        {
            PointLightComponent = Cast<UPointLightComponent>(NewSelectedComponent);
        }
    }
}

void UPointLightComponentWidget::RenderWidget()
{
	if (!PointLightComponent) { return; }
	Super::RenderWidget();

    // Attenuation Radius
    float AttenuationRadius = PointLightComponent->GetAttenuationRadius();
    if (ImGui::DragFloat("Attenuation Radius", &AttenuationRadius, 0.1f, 0.0f, 1000.0f))
    {
        PointLightComponent->SetAttenuationRadius(AttenuationRadius);
    }

    // Light Falloff Exponent
    float LightFalloffExponent = PointLightComponent->GetLightFalloffExponent();
    if (ImGui::DragFloat("Light Falloff Exponent", &LightFalloffExponent, 0.1f, 2.0f, 16.0f))
    {
        PointLightComponent->SetLightFalloffExponent(LightFalloffExponent);
    }

    ImGui::Separator();
}
