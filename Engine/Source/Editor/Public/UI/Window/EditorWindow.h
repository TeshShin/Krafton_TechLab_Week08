#pragma once
#include "Editor/Public/UI/Window/UIWindow.h"

class USplitterDebugWidget;
class UViewportMenuBarWidget;

/**
 * @brief Editor의 전반적인 UI를 담당하는 Window
 * 스플리터 UI 렌더링, 기즈모 조작 등 Editor 관련 위젯을 포함합니다.
 */
class UEditorWindow : public UUIWindow
{
	DECLARE_CLASS(UEditorWindow, UUIWindow);
public:
	UEditorWindow();
	virtual ~UEditorWindow() override = default;

	void Initialize() override;
	void RenderWindow() override;

	UViewportMenuBarWidget* GetViewportMenuBarWidget() const { return ViewportMenuBarWidget; }

private:
	USplitterDebugWidget* SplitterDebugWidget = nullptr;
	UViewportMenuBarWidget* ViewportMenuBarWidget = nullptr;
};
