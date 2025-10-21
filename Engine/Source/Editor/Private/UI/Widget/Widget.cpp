#include "pch.h"
#include "Editor/Public/UI/Widget/Widget.h"
#include "Core/Public/Object/Property.h"
#include "Core/Public/Object/Class.h"
#include "ImGui/imgui.h"

IMPLEMENT_ABSTRACT_CLASS(UWidget, UObject);

void UWidget::RenderProperties(UObject* TargetObject, bool bShowHeader)
{
	if (!TargetObject)
		return;

	UClass* TargetClass = TargetObject->GetClass();
	if (!TargetClass)
		return;

	const TArray<UPropertyBase*>& Properties = TargetClass->GetProperties();
	if (Properties.empty())
		return;

	// 헤더 표시
	if (bShowHeader)
	{
		ImGui::Text("=== Properties (Reflection) ===");
	}

	// 각 프로퍼티 렌더링
	for (UPropertyBase* Prop : Properties)
	{
		if (!Prop)
			continue;

		// VisibleAnywhere 또는 EditAnywhere 플래그가 있는 프로퍼티만 표시
		EPropertyFlags VisibilityFlags = EPropertyFlags::VisibleAnywhere | EPropertyFlags::EditAnywhere;
		if (!Prop->HasAnyFlags(VisibilityFlags))
		{
			continue; // 플래그가 없으면 표시하지 않음
		}

		// 메타데이터에서 편집 가능 여부 확인
		bool bIsEditable = Prop->IsEditable();
		const FPropertyMetadata& Meta = Prop->GetMetadata();

		// 프로퍼티 정보 (DisplayName 우선 사용)
		const char* PropName = Prop->GetDisplayName();
		EPropertyType PropType = Prop->GetPropertyType();

		// 타입별 UI 렌더링
		switch (PropType)
		{
		case EPropertyType::Float:
		{
			FString ValueStr = Prop->ToString(TargetObject);
			float Value = 0.0f;

			try
			{
				Value = std::stof(ValueStr);
			}
			catch (...)
			{
				ImGui::Text("%s: [Parse Error]", PropName);
				break;
			}

			if (bIsEditable)
			{
				// Min/Max 제약 가져오기
				float MinValue = static_cast<float>(Meta.Min);
				float MaxValue = static_cast<float>(Meta.Max);
				float Step = static_cast<float>(Meta.Step);

				bool bChanged = false;

				// Slider 또는 DragFloat 선택
				if (Meta.bSlider)
				{
					bChanged = ImGui::SliderFloat(PropName, &Value, MinValue, MaxValue);
				}
				else
				{
					bChanged = ImGui::DragFloat(PropName, &Value, Step, MinValue, MaxValue);
				}

				if (bChanged)
				{
					Prop->FromString(TargetObject, FString(std::to_string(Value).c_str()));
				}

				// Tooltip 표시
				if (Meta.Tooltip && ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", Meta.Tooltip);
				}
			}
			else
			{
				ImGui::Text("%s: %.3f", PropName, Value);
			}
			break;
		}

		case EPropertyType::Int32:
		{
			FString ValueStr = Prop->ToString(TargetObject);
			int32 Value = 0;

			try
			{
				Value = std::stoi(ValueStr);
			}
			catch (...)
			{
				ImGui::Text("%s: [Parse Error]", PropName);
				break;
			}

			if (bIsEditable)
			{
				// Min/Max 제약 가져오기
				int MinValue = static_cast<int>(Meta.Min);
				int MaxValue = static_cast<int>(Meta.Max);
				float Step = static_cast<float>(Meta.Step);

				bool bChanged = false;

				// Slider 또는 DragInt 선택
				if (Meta.bSlider)
				{
					bChanged = ImGui::SliderInt(PropName, &Value, MinValue, MaxValue);
				}
				else
				{
					bChanged = ImGui::DragInt(PropName, &Value, Step, MinValue, MaxValue);
				}

				if (bChanged)
				{
					Prop->FromString(TargetObject, FString(std::to_string(Value).c_str()));
				}

				// Tooltip 표시
				if (Meta.Tooltip && ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", Meta.Tooltip);
				}
			}
			else
			{
				ImGui::Text("%s: %d", PropName, Value);
			}
			break;
		}

		case EPropertyType::Bool:
		{
			FString ValueStr = Prop->ToString(TargetObject);
			bool Value = (ValueStr == "true" || ValueStr == "1");

			if (bIsEditable)
			{
				if (ImGui::Checkbox(PropName, &Value))
				{
					Prop->FromString(TargetObject, Value ? FString("true") : FString("false"));
				}

				// Tooltip 표시
				if (Meta.Tooltip && ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", Meta.Tooltip);
				}
			}
			else
			{
				ImGui::Text("%s: %s", PropName, Value ? "true" : "false");
			}
			break;
		}

		case EPropertyType::String:
		{
			FString ValueStr = Prop->ToString(TargetObject);

			// 프로퍼티마다 고유한 버퍼 사용 (static 제거)
			char TextBuffer[256];
			strncpy_s(TextBuffer, ValueStr.c_str(), sizeof(TextBuffer) - 1);
			TextBuffer[sizeof(TextBuffer) - 1] = '\0';

			if (bIsEditable)
			{
				if (ImGui::InputText(PropName, TextBuffer, sizeof(TextBuffer),
					ImGuiInputTextFlags_EnterReturnsTrue))
				{
					Prop->FromString(TargetObject, FString(TextBuffer));
				}

				// Tooltip 표시
				if (Meta.Tooltip && ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", Meta.Tooltip);
				}
			}
			else
			{
				ImGui::Text("%s: %s", PropName, TextBuffer);
			}
			break;
		}

		default:
		{
			// 기타 타입 - 문자열로 표시만
			FString ValueStr = Prop->ToString(TargetObject);
			ImGui::Text("%s: %s", PropName, ValueStr.c_str());
			break;
		}
		}

		// 추가 플래그 힌트 표시
		if (Prop->HasAnyFlags(EPropertyFlags::SaveGame))
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "[Saved]");
		}

		if (bIsEditable)
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "[Editable]");
		}
	}

	if (bShowHeader)
	{
		ImGui::Separator();
	}
}
