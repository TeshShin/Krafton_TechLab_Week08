#pragma once
#include "Editor/Public/UI/Window/UIWindow.h"

class UViewportMenuBarWidget;

/**
 * @brief 다중 뷰포트의 속성을 제어할 수 있는 UI를 담당하는 Window
 * ImGui 윈도우로 뷰포트를 표시하며, docking 가능
 */
class UViewportClientWindow : public UUIWindow
{
	DECLARE_CLASS(UViewportClientWindow, UUIWindow)
public:
	UViewportClientWindow();
	virtual ~UViewportClientWindow() override = default;

	void Initialize() override;
	void RenderWindow();

	// Getter
	bool IsViewportHovered() const { return bIsViewportHovered; }
	bool IsViewportFocused() const { return bIsViewportFocused; }
	const FVector& GetViewportMousePosition() const { return ViewportMousePosition; }
	const ImVec2& GetViewportImagePos() const { return ViewportImagePos; }
	const ImVec2& GetViewportImageSize() const { return ViewportImageSize; }

private:
	void SetupConfig();
	void RenderViewportImage();
	void UpdateViewportMousePosition();

	UViewportMenuBarWidget* ViewportMenuBarWidget = nullptr;

	// 뷰포트 상태
	bool bIsViewportHovered = false;
	bool bIsViewportFocused = false;

	// 뷰포트 이미지 정보 (마우스 좌표 변환용)
	ImVec2 ViewportImagePos = ImVec2(0, 0);
	ImVec2 ViewportImageSize = ImVec2(0, 0);
	FVector ViewportMousePosition = FVector(0, 0, 0);
};

