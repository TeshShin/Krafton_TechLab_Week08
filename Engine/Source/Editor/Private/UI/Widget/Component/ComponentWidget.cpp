#include "pch.h"
#include "Editor/Public/UI/Widget/Component/ComponentWidget.h"
#include "Editor/Public/UI/ImGuiStyleHelper.h"
#include "Editor/Public/Editor.h"
#include "Scene/Public/Component/ActorComponent.h"
#include "Core/Public/Object/Property.h"
#include "Core/Public/Object/Class.h"
#include "ImGui/imgui.h"

IMPLEMENT_CLASS(UComponentWidget, UWidget)

void UComponentWidget::RenderWidget()
{
	// 선택된 컴포넌트 가져오기
	UActorComponent* SelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
	if (!SelectedComponent)
		return;

	// UPROPERTY 자동 렌더링
	RenderProperties(SelectedComponent, true);
}

void UComponentWidget::RenderProperties(UObject* TargetObject, bool bShowHeader)
{
	if (!TargetObject)
		return;

	UClass* TargetClass = TargetObject->GetClass();
	if (!TargetClass)
		return;

	// 부모 클래스의 프로퍼티도 포함하여 가져오기
	TArray<UPropertyBase*> AllProperties;
	TargetClass->GetAllProperties(AllProperties);
	if (AllProperties.empty())
		return;

	// 헤더 표시
	if (bShowHeader)
	{
		ImGui::Separator();
	}

	// 각 프로퍼티 렌더링
	for (UPropertyBase* Prop : AllProperties)
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
				// Min/Max 제약 가져오기 (infinity 체크)
				float MinValue = std::isinf(Meta.Min) ? -FLT_MAX : static_cast<float>(Meta.Min);
				float MaxValue = std::isinf(Meta.Max) ? FLT_MAX : static_cast<float>(Meta.Max);
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
				// Min/Max 제약 가져오기 (infinity 체크)
				int MinValue = std::isinf(Meta.Min) ? INT_MIN : static_cast<int>(Meta.Min);
				int MaxValue = std::isinf(Meta.Max) ? INT_MAX : static_cast<int>(Meta.Max);
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

		case EPropertyType::Vector2:
		{
			FString ValueStr = Prop->ToString(TargetObject);
			float values[2] = {0.0f, 0.0f};

			// Parse "[x, y]" format
			sscanf_s(ValueStr.c_str(), "[%f, %f]", &values[0], &values[1]);

			if (bIsEditable)
			{
				float Step = static_cast<float>(Meta.Step);
				if (ImGui::DragFloat2(PropName, values, Step))
				{
					char buffer[128];
					snprintf(buffer, sizeof(buffer), "[%.6f, %.6f]", values[0], values[1]);
					Prop->FromString(TargetObject, FString(buffer));
				}

				if (Meta.Tooltip && ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", Meta.Tooltip);
				}
			}
			else
			{
				ImGui::Text("%s: [%.3f, %.3f]", PropName, values[0], values[1]);
			}
			break;
		}

		case EPropertyType::Vector:
		{
			FString ValueStr = Prop->ToString(TargetObject);
			float values[3] = {0.0f, 0.0f, 0.0f};

			// Parse "[x, y, z]" format
			sscanf_s(ValueStr.c_str(), "[%f, %f, %f]", &values[0], &values[1], &values[2]);

			if (bIsEditable)
			{
				float Step = static_cast<float>(Meta.Step);
				if (ImGui::DragFloat3(PropName, values, Step))
				{
					char buffer[128];
					snprintf(buffer, sizeof(buffer), "[%.6f, %.6f, %.6f]", values[0], values[1], values[2]);
					Prop->FromString(TargetObject, FString(buffer));
				}

				if (Meta.Tooltip && ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", Meta.Tooltip);
				}
			}
			else
			{
				ImGui::Text("%s: [%.3f, %.3f, %.3f]", PropName, values[0], values[1], values[2]);
			}
			break;
		}

		case EPropertyType::Vector4:
		{
			FString ValueStr = Prop->ToString(TargetObject);
			float values[4] = {0.0f, 0.0f, 0.0f, 0.0f};

			// Parse "[x, y, z, w]" format
			sscanf_s(ValueStr.c_str(), "[%f, %f, %f, %f]", &values[0], &values[1], &values[2], &values[3]);

			if (bIsEditable)
			{
				float Step = static_cast<float>(Meta.Step);
				if (ImGui::DragFloat4(PropName, values, Step))
				{
					char buffer[128];
					snprintf(buffer, sizeof(buffer), "[%.6f, %.6f, %.6f, %.6f]", values[0], values[1], values[2], values[3]);
					Prop->FromString(TargetObject, FString(buffer));
				}

				if (Meta.Tooltip && ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", Meta.Tooltip);
				}
			}
			else
			{
				ImGui::Text("%s: [%.3f, %.3f, %.3f, %.3f]", PropName, values[0], values[1], values[2], values[3]);
			}
			break;
		}

		case EPropertyType::LinearColor3:
		{
			FString ValueStr = Prop->ToString(TargetObject);
			float values[3] = {1.0f, 1.0f, 1.0f};

			// Parse "[r, g, b]" format
			sscanf_s(ValueStr.c_str(), "[%f, %f, %f]", &values[0], &values[1], &values[2]);

			if (bIsEditable)
			{
				if (ImGui::ColorEdit3(PropName, values))
				{
					char buffer[128];
					snprintf(buffer, sizeof(buffer), "[%.6f, %.6f, %.6f]", values[0], values[1], values[2]);
					Prop->FromString(TargetObject, FString(buffer));
				}

				if (Meta.Tooltip && ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", Meta.Tooltip);
				}
			}
			else
			{
				ImGui::Text("%s: [%.3f, %.3f, %.3f]", PropName, values[0], values[1], values[2]);
			}
			break;
		}

		case EPropertyType::LinearColor:
		{
			FString ValueStr = Prop->ToString(TargetObject);
			float values[4] = {1.0f, 1.0f, 1.0f, 1.0f};

			// Parse "[r, g, b, a]" format
			sscanf_s(ValueStr.c_str(), "[%f, %f, %f, %f]", &values[0], &values[1], &values[2], &values[3]);

			if (bIsEditable)
			{
				if (ImGui::ColorEdit4(PropName, values))
				{
					char buffer[128];
					snprintf(buffer, sizeof(buffer), "[%.6f, %.6f, %.6f, %.6f]", values[0], values[1], values[2], values[3]);
					Prop->FromString(TargetObject, FString(buffer));
				}

				if (Meta.Tooltip && ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", Meta.Tooltip);
				}
			}
			else
			{
				ImGui::Text("%s: [%.3f, %.3f, %.3f, %.3f]", PropName, values[0], values[1], values[2], values[3]);
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
			ImGui::TextColored(FImGuiStyleHelper::LogSuccess(), "[Saved]");
		}

		if (bIsEditable)
		{
			ImGui::SameLine();
			ImGui::TextColored(FImGuiStyleHelper::LogWarning(), "[Editable]");
		}
	}

	if (bShowHeader)
	{
		//ImGui::Separator();
	}
}
