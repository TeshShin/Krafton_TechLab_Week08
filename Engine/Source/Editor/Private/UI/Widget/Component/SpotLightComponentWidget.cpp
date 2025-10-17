#include "pch.h"
#include "Editor/Public/UI/Widget/Component/SpotLightComponentWidget.h"
#include "Scene/Public/Component/SpotLightComponent.h"
#include "Editor/Public/Editor.h"
#include "Scene/Public/Level/Level.h"
#include "Scene/Public/Component/ActorComponent.h"
#include "ImGui/imgui.h"

IMPLEMENT_CLASS(USpotLightComponentWidget, UWidget)

void USpotLightComponentWidget::Initialize()
{
}

void USpotLightComponentWidget::Update()
{
    ULevel* CurrentLevel = GWorld->GetLevel();
    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
        if (SpotLightComponent != NewSelectedComponent)
        {
            SpotLightComponent = Cast<USpotLightComponent>(NewSelectedComponent);
        }
    }
}

void USpotLightComponentWidget::RenderWidget()
{
    if (!SpotLightComponent)
    {
        return;
    }

    ImGui::Separator();

    // TODO: SpotLight properties will be added here

    ImGui::Separator();
}
