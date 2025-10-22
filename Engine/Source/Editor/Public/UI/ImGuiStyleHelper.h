#pragma once
#include "ImGui/imgui.h"
#include <cmath>

/**
 * @brief ImGui UI 색상 및 스타일을 중앙에서 관리하는 유틸리티 클래스
 * 통일된 디자인 시스템을 제공하여 일관된 UI를 구현합니다.
 *
 * @note SwapChain이 선형 색상 공간(DXGI_FORMAT_B8G8R8A8_UNORM)을 사용하므로,
 *       sRGB 색상을 선형으로 변환하여 올바르게 표시되도록 합니다.
 */
class FImGuiStyleHelper
{
private:
	/**
	 * @brief sRGB 색상을 선형 색상으로 변환하는 헬퍼 함수
	 * @param sRGB sRGB 색상 값 (0.0 ~ 1.0)
	 * @return 선형 색상 값 (0.0 ~ 1.0)
	 * @note 정확한 sRGB to Linear 변환 공식 사용 (IEC 61966-2-1 표준)
	 * @note C++20 constexpr로 컴파일 타임에 계산됨
	 */
	static constexpr float SRGBToLinear(float sRGB)
	{
		return sRGB <= 0.04045f
			? sRGB / 12.92f
			: std::pow((sRGB + 0.055f) / 1.055f, 2.4f);
	}

	/**
	 * @brief sRGB ImVec4 색상을 선형 색상으로 변환
	 * @param sRGB sRGB 색상
	 * @return 선형 색상
	 */
	static constexpr ImVec4 SRGBToLinear(const ImVec4& sRGB)
	{
		return ImVec4(
			SRGBToLinear(sRGB.x),
			SRGBToLinear(sRGB.y),
			SRGBToLinear(sRGB.z),
			sRGB.w  // Alpha는 변환하지 않음
		);
	}

public:
	// ==================== Semantic Colors ====================
	// 의미론적 색상 - 특정 의도를 가진 색상들 (채도를 낮춰 눈의 피로 감소)

	/** @brief 성공, 활성화 상태 (부드러운 녹색) */
	static ImVec4 Success() { return SRGBToLinear(ImVec4(0.30f, 0.69f, 0.31f, 1.0f)); }
	static ImVec4 SuccessHovered() { return SRGBToLinear(ImVec4(0.40f, 0.79f, 0.41f, 1.0f)); }
	static ImVec4 SuccessActive() { return SRGBToLinear(ImVec4(0.20f, 0.59f, 0.21f, 1.0f)); }

	/** @brief 경고, 일시정지 상태 (부드러운 노란색) */
	static ImVec4 Warning() { return SRGBToLinear(ImVec4(0.86f, 0.86f, 0.67f, 1.0f)); }
	static ImVec4 WarningHovered() { return SRGBToLinear(ImVec4(0.96f, 0.96f, 0.77f, 1.0f)); }
	static ImVec4 WarningActive() { return SRGBToLinear(ImVec4(0.76f, 0.76f, 0.57f, 1.0f)); }

	/** @brief 위험, 중지, 에러 (부드러운 빨간색) */
	static ImVec4 Danger() { return SRGBToLinear(ImVec4(0.96f, 0.53f, 0.44f, 1.0f)); }
	static ImVec4 DangerHovered() { return SRGBToLinear(ImVec4(1.0f, 0.63f, 0.54f, 1.0f)); }
	static ImVec4 DangerActive() { return SRGBToLinear(ImVec4(0.86f, 0.43f, 0.34f, 1.0f)); }

	/** @brief 정보, 일반 액센트 (부드러운 파란색) */
	static ImVec4 Info() { return SRGBToLinear(ImVec4(0.34f, 0.61f, 0.84f, 1.0f)); }
	static ImVec4 InfoHovered() { return SRGBToLinear(ImVec4(0.44f, 0.71f, 0.94f, 1.0f)); }
	static ImVec4 InfoActive() { return SRGBToLinear(ImVec4(0.24f, 0.51f, 0.74f, 1.0f)); }

	/** @brief 비활성화 상태 (매우 어두운 블루-그레이) */
	static ImVec4 Disabled() { return SRGBToLinear(ImVec4(0.15f, 0.16f, 0.20f, 0.7f)); }

	// ==================== Text Colors ====================

	/** @brief 기본 텍스트 색상 (밝은 회색 - 높은 대비) */
	static ImVec4 TextPrimary() { return SRGBToLinear(ImVec4(0.88f, 0.89f, 0.92f, 1.0f)); }

	/** @brief 보조 텍스트 색상 (중간 회색) */
	static ImVec4 TextSecondary() { return SRGBToLinear(ImVec4(0.56f, 0.58f, 0.64f, 1.0f)); }

	/** @brief 비활성화된 텍스트 색상 (어두운 회색) */
	static ImVec4 TextDisabled() { return SRGBToLinear(ImVec4(0.38f, 0.40f, 0.45f, 1.0f)); }

	// ==================== Background Colors ====================
	// Midnight 테마 - 어두운 블루-블랙 톤 (블루 강조, 가독성 개선)

	/** @brief 주 배경색 (미드나잇 블랙) */
	static ImVec4 BackgroundPrimary() { return SRGBToLinear(ImVec4(0.08f, 0.09f, 0.12f, 1.0f)); }

	/** @brief 보조 배경색 (어두운 블루-그레이) */
	static ImVec4 BackgroundSecondary() { return SRGBToLinear(ImVec4(0.10f, 0.11f, 0.15f, 1.0f)); }

	/** @brief 삼차 배경색 (위젯 배경) */
	static ImVec4 BackgroundTertiary() { return SRGBToLinear(ImVec4(0.13f, 0.14f, 0.19f, 1.0f)); }

	/** @brief 호버 시 배경색 */
	static ImVec4 BackgroundHovered() { return SRGBToLinear(ImVec4(0.16f, 0.17f, 0.23f, 1.0f)); }

	/** @brief 활성화 시 배경색 */
	static ImVec4 BackgroundActive() { return SRGBToLinear(ImVec4(0.19f, 0.21f, 0.28f, 1.0f)); }

	// ==================== Border & Separator ====================

	/** @brief 테두리 색상 */
	static ImVec4 Border() { return SRGBToLinear(ImVec4(0.22f, 0.24f, 0.30f, 1.0f)); }

	/** @brief 구분선 색상 */
	static ImVec4 Separator() { return SRGBToLinear(ImVec4(0.18f, 0.20f, 0.26f, 1.0f)); }

	// ==================== Log Level Colors ====================

	/** @brief 로그 - 일반 */
	static ImVec4 LogNormal() { return SRGBToLinear(ImVec4(0.83f, 0.83f, 0.83f, 1.0f)); }

	/** @brief 로그 - 성공 */
	static ImVec4 LogSuccess() { return SRGBToLinear(ImVec4(0.31f, 0.79f, 0.69f, 1.0f)); }

	/** @brief 로그 - 경고 */
	static ImVec4 LogWarning() { return SRGBToLinear(ImVec4(0.86f, 0.86f, 0.67f, 1.0f)); }

	/** @brief 로그 - 에러 */
	static ImVec4 LogError() { return SRGBToLinear(ImVec4(0.96f, 0.53f, 0.44f, 1.0f)); }

	// ==================== Helper Functions ====================

	/**
	 * @brief 버튼의 3가지 상태 색상을 한 번에 푸시 (Normal, Hovered, Active)
	 * @param Normal 기본 상태 색상
	 * @param Hovered 호버 상태 색상
	 * @param Active 클릭 상태 색상
	 */
	static void PushButtonColors(const ImVec4& Normal, const ImVec4& Hovered, const ImVec4& Active)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, Normal);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Hovered);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, Active);
	}

	/** @brief 성공 스타일 버튼 색상 푸시 */
	static void PushSuccessButton()
	{
		PushButtonColors(Success(), SuccessHovered(), SuccessActive());
	}

	/** @brief 경고 스타일 버튼 색상 푸시 */
	static void PushWarningButton()
	{
		PushButtonColors(Warning(), WarningHovered(), WarningActive());
	}

	/** @brief 위험 스타일 버튼 색상 푸시 */
	static void PushDangerButton()
	{
		PushButtonColors(Danger(), DangerHovered(), DangerActive());
	}

	/** @brief 정보 스타일 버튼 색상 푸시 */
	static void PushInfoButton()
	{
		PushButtonColors(Info(), InfoHovered(), InfoActive());
	}

	/** @brief 비활성화 스타일 버튼 색상 푸시 */
	static void PushDisabledButton()
	{
		PushButtonColors(Disabled(), Disabled(), Disabled());
	}

	/**
	 * @brief 전역 ImGui 스타일을 다크 테마로 초기화
	 * ImGuiHelper 초기화 시점에 호출됩니다.
	 */
	static void ApplyDarkTheme()
	{
		ImGuiStyle& Style = ImGui::GetStyle();
		ImVec4* Colors = Style.Colors;

		// Background colors - 검정색 기반
		Colors[ImGuiCol_WindowBg] = BackgroundPrimary();
		Colors[ImGuiCol_ChildBg] = BackgroundSecondary();
		Colors[ImGuiCol_PopupBg] = BackgroundSecondary();
		Colors[ImGuiCol_FrameBg] = BackgroundSecondary();
		Colors[ImGuiCol_FrameBgHovered] = BackgroundHovered();
		Colors[ImGuiCol_FrameBgActive] = BackgroundActive();

		// Title bar
		Colors[ImGuiCol_TitleBg] = BackgroundPrimary();
		Colors[ImGuiCol_TitleBgActive] = BackgroundSecondary();
		Colors[ImGuiCol_TitleBgCollapsed] = BackgroundPrimary();

		// Menu bar
		Colors[ImGuiCol_MenuBarBg] = BackgroundSecondary();

		// Scrollbar
		Colors[ImGuiCol_ScrollbarBg] = BackgroundPrimary();
		Colors[ImGuiCol_ScrollbarGrab] = BackgroundTertiary();
		Colors[ImGuiCol_ScrollbarGrabHovered] = BackgroundHovered();
		Colors[ImGuiCol_ScrollbarGrabActive] = BackgroundActive();

		// Tabs
		Colors[ImGuiCol_Tab] = BackgroundSecondary();
		Colors[ImGuiCol_TabHovered] = BackgroundHovered();
		Colors[ImGuiCol_TabActive] = BackgroundTertiary();
		Colors[ImGuiCol_TabUnfocused] = BackgroundSecondary();
		Colors[ImGuiCol_TabUnfocusedActive] = BackgroundSecondary();

		// Headers (CollapsingHeader, TreeNode 등)
		Colors[ImGuiCol_Header] = BackgroundTertiary();
		Colors[ImGuiCol_HeaderHovered] = BackgroundHovered();
		Colors[ImGuiCol_HeaderActive] = BackgroundActive();

		// Buttons - 기본값
		Colors[ImGuiCol_Button] = BackgroundTertiary();
		Colors[ImGuiCol_ButtonHovered] = BackgroundHovered();
		Colors[ImGuiCol_ButtonActive] = BackgroundActive();

		// Text
		Colors[ImGuiCol_Text] = TextPrimary();
		Colors[ImGuiCol_TextDisabled] = TextDisabled();
		Colors[ImGuiCol_TextSelectedBg] = SRGBToLinear(ImVec4(0.34f, 0.61f, 0.84f, 0.35f)); // Info 색상 기반

		// Borders & Separators
		Colors[ImGuiCol_Border] = Border();
		Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		Colors[ImGuiCol_Separator] = Separator();
		Colors[ImGuiCol_SeparatorHovered] = SRGBToLinear(ImVec4(0.35f, 0.36f, 0.40f, 1.0f));
		Colors[ImGuiCol_SeparatorActive] = SRGBToLinear(ImVec4(0.40f, 0.42f, 0.47f, 1.0f));

		// Check marks (Info 색상 사용)
		Colors[ImGuiCol_CheckMark] = Info();

		// Sliders (Info 색상 사용)
		Colors[ImGuiCol_SliderGrab] = Info();
		Colors[ImGuiCol_SliderGrabActive] = InfoHovered();

		// Resize grip
		Colors[ImGuiCol_ResizeGrip] = SRGBToLinear(ImVec4(0.30f, 0.31f, 0.35f, 0.5f));
		Colors[ImGuiCol_ResizeGripHovered] = SRGBToLinear(ImVec4(0.34f, 0.61f, 0.84f, 0.67f)); // Info 색상
		Colors[ImGuiCol_ResizeGripActive] = SRGBToLinear(ImVec4(0.34f, 0.61f, 0.84f, 0.95f));

		// Plot
		Colors[ImGuiCol_PlotLines] = Info();
		Colors[ImGuiCol_PlotLinesHovered] = InfoHovered();
		Colors[ImGuiCol_PlotHistogram] = Warning();
		Colors[ImGuiCol_PlotHistogramHovered] = WarningHovered();

		// Table
		Colors[ImGuiCol_TableHeaderBg] = BackgroundTertiary();
		Colors[ImGuiCol_TableBorderStrong] = Border();
		Colors[ImGuiCol_TableBorderLight] = Separator();
		Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		Colors[ImGuiCol_TableRowBgAlt] = SRGBToLinear(ImVec4(0.88f, 0.89f, 0.92f, 0.05f));

		// Style adjustments
		Style.WindowRounding = 4.0f;
		Style.FrameRounding = 3.0f;
		Style.GrabRounding = 3.0f;
		Style.TabRounding = 3.0f;
		Style.ScrollbarRounding = 3.0f;
		Style.WindowBorderSize = 1.0f;
		Style.FrameBorderSize = 0.0f;
	}
};
