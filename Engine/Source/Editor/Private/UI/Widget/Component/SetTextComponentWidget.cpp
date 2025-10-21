#include "pch.h"
#include "Editor/Public/UI/Widget/Component/SetTextComponentWidget.h"
#include "Scene/Public/Component/TextComponent.h"
#include "Scene/Public/Component/ActorComponent.h"
#include "Editor/Public/Editor.h"
#include <climits>

IMPLEMENT_CLASS(USetTextComponentWidget, UComponentWidget)

void USetTextComponentWidget::Initialize()
{
	// Do Nothing Here
}

void USetTextComponentWidget::Update()
{
	UActorComponent* Component = GEditor->GetEditorModule()->GetSelectedComponent();
	if (!Component) { return; }

	SelectedTextComponent = Cast<UTextComponent>(Component);
}

void USetTextComponentWidget::RenderWidget()
{
	// 먼저 부모 클래스의 RenderWidget을 호출하여 UPROPERTY 자동 렌더링
	Super::RenderWidget();

	if (!SelectedTextComponent) { return; }
	
	ImGui::Text("Type Text");
	ImGui::Spacing();
	
	static char Buffer[256] = "";
	const FString& TextOfComponent = SelectedTextComponent->GetText();
	memcpy(Buffer, TextOfComponent.c_str(), std::min(sizeof(Buffer), TextOfComponent.size()));
	FString TagName = FString("TypeText##");

	if (ImGui::InputText(TagName.c_str(), Buffer, IM_ARRAYSIZE(Buffer)))
	{
		SelectedTextComponent->SetText(FString(Buffer));
	}
	
	ImGui::Separator();
}
