#include "pch.h"
#include "Editor/Public/UI/Window/EditorWindow.h"
#include "Editor/Public/UI/Widget/SplitterDebugWidget.h"
#include "Editor/Public/UI/Widget/ViewportMenuBarWidget.h"
#include "Manager/Public/UIManager.h"
#include "Renderer/Public/Renderer.h"

IMPLEMENT_CLASS(UEditorWindow, UUIWindow)

/**
 * @brief Editor Window Constructor
 */
UEditorWindow::UEditorWindow()
{
	FUIWindowConfig Config;
	Config.WindowTitle = "Editor";
	Config.DefaultSize = ImVec2(0, 0);
	Config.DefaultPosition = ImVec2(0, 0);
	Config.DockDirection = EUIDockDirection::None; // 특정 위치에 고정하지 않음
	Config.Priority = 10000; // 가장 마지막에 그려지도록 우선순위를 높게 설정
	Config.bResizable = false;
	Config.bMovable = false;
	Config.bCollapsible = false;

	// 배경이 없는 투명 창으로 만들기 위한 플래그 설정
	// NoInputs: 3D 씬 마우스 입력을 위해 차단 (스플릿터는 직접 마우스 위치 확인)
	Config.WindowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoInputs;

	SetConfig(Config);

	// 위젯 생성 및 초기화
	if (SplitterDebugWidget = NewObject<USplitterDebugWidget>())
	{
		SplitterDebugWidget->Initialize();
		AddWidget(SplitterDebugWidget);
		UE_LOG("EditorWindow: SplitterDebugWidget이 생성되고 초기화되었습니다");
	}
	else
	{
		UE_LOG("EditorWindow: Error: SplitterDebugWidget 생성에 실패했습니다!");
		return;
	}

	// ViewportMenuBarWidget 생성
	if (ViewportMenuBarWidget = NewObject<UViewportMenuBarWidget>())
	{
		ViewportMenuBarWidget->Initialize();
		AddWidget(ViewportMenuBarWidget);
		UE_LOG("EditorWindow: ViewportMenuBarWidget이 생성되고 초기화되었습니다");
	}
	else
	{
		UE_LOG("EditorWindow: Error: ViewportMenuBarWidget 생성에 실패했습니다!");
		return;
	}

	UE_LOG("EditorWindow: 에디터 윈도우가 초기화되었습니다");
}

/**
 * @brief Initializer
 */
void UEditorWindow::Initialize()
{
	UE_LOG("EditorWindow: Window가 성공적으로 생성되었습니다.");

	SetWindowState(EUIWindowState::Visible);
}

void UEditorWindow::RenderWindow()
{
	if (!IsVisible())
	{
		return;
	}

	// ViewportMenuBarWidget 업데이트
	if (ViewportMenuBarWidget)
	{
		FViewport* Viewport = URenderer::GetInstance().GetViewportClient();
		if (Viewport)
		{
			ViewportMenuBarWidget->SetViewportClient(Viewport);
		}
	}

	// 중앙 노드 영역과 일치하도록 윈도우 위치/크기 설정
	UUIManager& UIManager = UUIManager::GetInstance();

	if (UIManager.HasCentralNode())
	{
		ImVec2 CentralPos = UIManager.GetCentralNodePos();
		ImVec2 CentralSize = UIManager.GetCentralNodeSize();

		ImGui::SetNextWindowPos(CentralPos);
		ImGui::SetNextWindowSize(CentralSize);
	}

	const FUIWindowConfig& Config = GetConfig();

	if (ImGui::Begin(Config.WindowTitle.ToString().data(), nullptr, Config.WindowFlags))
	{
		// 스플릿터 위젯 렌더링
		if (SplitterDebugWidget)
		{
			SplitterDebugWidget->RenderWidget();
		}

		UpdateWindowInfo();
	}

	ImGui::End();
}
