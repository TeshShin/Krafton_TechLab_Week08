#include "pch.h"
#include "Editor/Public/UI/Widget/Component/RotatingMovementComponentWidget.h"
#include "Editor/Public/Editor.h"
#include "Scene/Public/Component/RotatingMovementComponent.h"
#include "Scene/Public/Level/Level.h"

IMPLEMENT_CLASS(URotatingMovementComponentWidget, UWidget)

void URotatingMovementComponentWidget::RenderWidget()
{
    ULevel* CurrentLevel = GWorld->GetLevel();

    if (!CurrentLevel)
    {
        ImGui::TextUnformatted("No Level Loaded");
        return;
    }

    UActorComponent* Component = GEditor->GetEditorModule()->GetSelectedComponent();
    if (!Component)
    {
        ImGui::TextUnformatted("No Object Selected");
        return;
    }
    
	URotatingMovementComponent* RotatingMovementComponent = Cast<URotatingMovementComponent>(Component);
    if (!RotatingMovementComponent) { return; }

    ImGui::DragFloat3("Rotation Rate", &RotatingMovementComponent->RotationRate.X);
    ImGui::DragFloat3("Pivot Translation", &RotatingMovementComponent->PivotTranslation.X);
    ImGui::Checkbox("Rotation In Local Space", &RotatingMovementComponent->bRotationInLocalSpace);
}
