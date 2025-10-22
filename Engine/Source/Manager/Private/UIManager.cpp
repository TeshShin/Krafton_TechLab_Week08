#include "pch.h"
#include "Manager/Public/UIManager.h"
#include "Manager/Public/TimeManager.h"
#include "Editor/Public/UI/Window/UIWindow.h"
#include "Editor/Public/UI/ImGuiHelper.h"
#include "Editor/Public/UI/Widget/Widget.h"
#include "Editor/Public/UI/Window/MainMenuWindow.h"
#include "Editor/Public/UI/Window/EditorWindow.h"
#include "Editor/Public/UI/Widget/ViewportMenuBarWidget.h"
#include "ImGui/imgui_internal.h"

IMPLEMENT_SINGLETON_CLASS(UUIManager, UObject)

UUIManager::UUIManager()
{
	ImGuiHelper = new UImGuiHelper();
	Initialize();
}

UUIManager::~UUIManager()
{
	if (ImGuiHelper)
	{
		delete ImGuiHelper;
		ImGuiHelper = nullptr;
	}
}

/**
 * @brief UI 매니저 초기화
 */
void UUIManager::Initialize()
{
	if (bIsInitialized)
	{
		return;
	}

	UE_LOG("UIManager: UI System 초기화 진행 중...");

	UIWindows.clear();
	FocusedWindow = nullptr;
	TotalTime = 0.0f;

	bIsInitialized = true;

	UE_LOG("UIManager: UI System 초기화 성공");
}

/**
 * @brief ImGui를 포함한 UI Manager 초기화
 */
void UUIManager::Initialize(HWND InWindowHandle)
{
	Initialize();

	if (ImGuiHelper)
	{
		ImGuiHelper->Initialize(InWindowHandle);
		cout << "UIManager: ImGui Initialized Successfully." << "\n";
	}
}

/**
 * @brief UI 매니저 종료 및 정리
 */
void UUIManager::Shutdown()
{
	if (!bIsInitialized)
	{
		return;
	}

	UE_LOG("UIManager: UI system 종료 중...");

	// ImGui 정리
	if (ImGuiHelper)
	{
		ImGuiHelper->Release();
	}

	// 모든 UI 윈도우 정리
	for (auto* Window : UIWindows)
	{
		if (Window && !Window->IsSingleton())
		{
			Window->Cleanup();
			delete Window;
		}
	}

	UIWindows.clear();
	FocusedWindow = nullptr;
	bIsInitialized = false;

	UE_LOG("UIManager: UI 시스템 종료 완료");
}

/**
 * @brief 모든 UI 윈도우 업데이트
 */
void UUIManager::Update()
{
	if (!bIsInitialized)
	{
		return;
	}

	TotalTime += DT;

	// 모든 UI 윈도우 업데이트
	for (auto* Window : UIWindows)
	{
		if (Window && Window->IsVisible())
		{
			Window->Update();
		}
	}

	// 포커스 상태 업데이트
	UpdateFocusState();
}

/**
 * @brief 모든 UI 윈도우 렌더링
 */
void UUIManager::Render()
{
	if (!bIsInitialized)
	{
		return;
	}

	if (!ImGuiHelper)
	{
		return;
	}

	// ImGui 프레임 시작
	ImGuiHelper->BeginFrame();

	// 뷰포트 자동 조정을 위해 메인 메뉴바를 가장 먼저 렌더링
	if (MainMenuWindow && MainMenuWindow->IsVisible())
	{
		MainMenuWindow->RenderWidget();
	}

	// DockSpace 생성 (MainMenuBar 아래 전체 영역)
	CreateDockSpace();

	// 우선순위에 따라 정렬
	SortUIWindowsByPriority();

	// 나머지 UI 윈도우 렌더링
	for (auto* Window : UIWindows)
	{
		if (Window && Window != MainMenuWindow)
		{
			Window->RenderWindow();
		}
	}

	// EditorWindow의 ViewportMenuBar를 별도로 렌더링 (모든 윈도우 위에)
	UUIWindow* EditorWindowPtr = FindUIWindow("Editor");
	if (EditorWindowPtr)
	{
		UEditorWindow* EditorWnd = dynamic_cast<UEditorWindow*>(EditorWindowPtr);
		if (EditorWnd && EditorWnd->GetViewportMenuBarWidget())
		{
			EditorWnd->GetViewportMenuBarWidget()->RenderWidget();
		}
	}

	// ImGui 프레임 종료
	ImGuiHelper->EndFrame();
}

/**
 * @brief UI 윈도우 등록
 * @param InWindow 등록할 UI 윈도우
 * @return 등록 성공 여부
 */
bool UUIManager::RegisterUIWindow(UUIWindow* InWindow)
{
	if (!InWindow)
	{
		UE_LOG("UIManager: Error: Attempted To Register Null Window!");
		return false;
	}

	// 이미 등록된 윈도우인지 확인
	auto Iter = std::find(UIWindows.begin(), UIWindows.end(), InWindow);
	if (Iter != UIWindows.end())
	{
		UE_LOG("UIManager: Warning: Window Already Registered: %u", InWindow->GetWindowID());
		return false;
	}

	// 윈도우 초기화
	try
	{
		InWindow->Initialize();
	}
	catch (const exception& Exception)
	{
		UE_LOG("UIManager: Error: Window 생성에 실패했습니다 %u: %s", InWindow->GetWindowID(), Exception.what());
		return false;
	}

	UIWindows.push_back(InWindow);

	UE_LOG("UIManager: UI Window 등록: %s", InWindow->GetWindowTitle().ToString().data());
	UE_LOG("UIManager: 전체 등록된 Window 갯수: %zu", UIWindows.size());

	return true;
}

/**
 * @brief UI 윈도우 등록 해제
 * @param InWindow 해제할 UI 윈도우
 * @return 해제 성공 여부
 */
bool UUIManager::UnregisterUIWindow(UUIWindow* InWindow)
{
	if (!InWindow)
	{
		return false;
	}

	auto It = std::find(UIWindows.begin(), UIWindows.end(), InWindow);
	if (It == UIWindows.end())
	{
		UE_LOG("UIManager: Warning: Attempted to unregister non-existent window: %u", InWindow->GetWindowID());
		return false;
	}

	// 포커스된 윈도우였다면 포커스 해제
	if (FocusedWindow == InWindow)
	{
		FocusedWindow = nullptr;
	}

	// 윈도우 정리
	InWindow->Cleanup();

	UIWindows.erase(It);

	UE_LOG("UIManager: UI Window 등록 해제: %u", InWindow->GetWindowID());
	UE_LOG("UIManager: 전체 등록된 Window 갯수: %zu", UIWindows.size());

	return true;
}

/**
 * @brief 이름으로 UI 윈도우 검색
 * @param InWindowName 검색할 윈도우 제목
 * @return 찾은 윈도우 (없으면 nullptr)
 */
UUIWindow* UUIManager::FindUIWindow(const FName& InWindowName) const
{
	for (auto* Window : UIWindows)
	{
		if (Window && Window->GetWindowTitle() == InWindowName)
		{
			return Window;
		}
	}
	return nullptr;
}

UWidget* UUIManager::FindWidget(const FName& InWidgetName) const
{
	for (auto* Window : UIWindows)
	{
		for (auto* Widget : Window->GetWidgets())
		{
			if (Widget->GetName().ToBaseNameString() == InWidgetName.ToString())
			{
				return Widget;
			}
		}
	}
	return nullptr;
}

/**
 * @brief 모든 UI 윈도우 숨기기
 */
void UUIManager::HideAllWindows() const
{
	for (auto* Window : UIWindows)
	{
		if (Window)
		{
			Window->SetWindowState(EUIWindowState::Hidden);
		}
	}
	UE_LOG("UIManager: All Windows Hidden.");
}

/**
 * @brief 모든 UI 윈도우 보이기
 */
void UUIManager::ShowAllWindows() const
{
	for (auto* Window : UIWindows)
	{
		if (Window)
		{
			Window->SetWindowState(EUIWindowState::Visible);
		}
	}
	UE_LOG("UIManager: All Windows Shown.");
}

/**
 * @brief 특정 윈도우에 포커스 설정
 */
void UUIManager::SetFocusedWindow(UUIWindow* InWindow)
{
	if (FocusedWindow != InWindow)
	{
		if (FocusedWindow)
		{
			FocusedWindow->OnFocusLost();
		}

		FocusedWindow = InWindow;

		if (FocusedWindow)
		{
			FocusedWindow->OnFocusGained();
		}
	}
}

/**
 * @brief 디버그 정보 출력
 * 필요한 지점에서 호출해서 로그로 체크하는 용도
 */
void UUIManager::PrintDebugInfo() const
{
	UE_LOG("");
	UE_LOG("=== UI Manager Debug Info ===");
	UE_LOG("Initialized: %s", (bIsInitialized ? "Yes" : "No"));
	UE_LOG("Total Time: %.2fs", TotalTime);
	UE_LOG("Registered Windows: %zu", UIWindows.size());
	UE_LOG("Focused Window: %s", (FocusedWindow ? to_string(FocusedWindow->GetWindowID()).c_str() : "None"));

	UE_LOG("UIManager: All ImGui windows hidden due to minimization.");
	UE_LOG("--- Window List ---");
	for (size_t i = 0; i < UIWindows.size(); ++i)
	{
		auto* Window = UIWindows[i];
		if (Window)
		{
			UE_LOG("[%zu] %u (%s)", i, Window->GetWindowID(), Window->GetWindowTitle().ToString().data());
			UE_LOG("    State: %s", (Window->IsVisible() ? "Visible" : "Hidden"));
			UE_LOG("    Priority: %d", Window->GetPriority());
			UE_LOG("    Focused: %s", (Window->IsFocused() ? "Yes" : "No"));
		}
	}
	UE_LOG("===========================");
	UE_LOG("UIManager: All ImGui windows hidden due to minimization.");
}

/**
 * @brief UI 윈도우들을 우선순위에 따라 정렬
 */
void UUIManager::SortUIWindowsByPriority()
{
	// 우선순위가 낮을수록 먼저 렌더링되고 가려짐
	std::sort(UIWindows.begin(), UIWindows.end(), [](const UUIWindow* A, const UUIWindow* B)
	{
		if (!A)
		{
			return false;
		}
		if (!B)
		{
			return true;
		}

		return A->GetPriority() < B->GetPriority();
	});
}

/**
 * @brief 포커스 상태 업데이트
 */
void UUIManager::UpdateFocusState()
{
	// ImGui에서 현재 포커스된 윈도우 찾기
	UUIWindow* NewFocusedWindow = nullptr;

	for (auto* Window : UIWindows)
	{
		if (Window && Window->IsVisible() && Window->IsFocused())
		{
			NewFocusedWindow = Window;
			break;
		}
	}

	// 포커스 변경시 처리
	if (FocusedWindow != NewFocusedWindow)
	{
		SetFocusedWindow(NewFocusedWindow);
	}
}

/**
 * @brief 윈도우 프로시저 핸들러
 */
LRESULT UUIManager::WndProcHandler(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam)
{
	return UImGuiHelper::WndProcHandler(hwnd, msg, wParam, lParam);
}

void UUIManager::RepositionImGuiWindows() const
{
	// 1. 현재 화면(Viewport)의 작업 영역을 가져옵니다.
	for (auto& window : UIWindows)
	{
		window->SetIsResized(true);
	}
}

/**
 * @brief 메인 윈도우가 최소화될 때 호출되는 함수
 * 모든 ImGui 윈도우의 현재 상태를 저장
 */
void UUIManager::OnWindowMinimized()
{
	UE_LOG("UIManager: UI Minimize 작업 시작");

	if (!bIsInitialized || bIsMinimized)
	{
		return;
	}

	bIsMinimized = true;
	SavedWindowStates.clear();
	UE_LOG("UIManager: %zu개의 윈도우에 대해 상태 저장 시도", UIWindows.size());

	// 모든 UI 윈도우의 현재 상태 저장
	for (auto* Window : UIWindows)
	{
		if (Window)
		{
			FUIWindowSavedState SavedState;
			SavedState.WindowID = Window->GetWindowID();
			SavedState.SavedPosition = Window->GetLastWindowPosition();
			SavedState.SavedSize = Window->GetLastWindowSize();
			SavedState.bWasVisible = Window->IsVisible();

			UE_LOG("UIManager: Saving Window ID=%u, Position=(%.1f,%.1f), Size=(%.1f,%.1f), Visible=%s",
			       SavedState.WindowID,
			       SavedState.SavedPosition.x, SavedState.SavedPosition.y,
			       SavedState.SavedSize.x, SavedState.SavedSize.y,
			       (SavedState.bWasVisible ? "true" : "false"));

			SavedWindowStates.push_back(SavedState);
		}
	}

	UE_LOG("UIManager: 최소화로 인한 %zu개의 윈도우 상태 저장 완료", SavedWindowStates.size());
}

/**
 * @brief 메인 윈도우가 복원될 때 호출되는 함수
 * 저장된 상태로 모든 ImGui 윈도우를 복원
 */
void UUIManager::OnWindowRestored()
{
	UE_LOG("UIManager: UI Restore 작업 시작");

	if (!bIsInitialized || !bIsMinimized)
	{
		return;
	}

	bIsMinimized = false;
	UE_LOG("UIManager: %zu개의 윈도우에 대해 상태 복원 시도", SavedWindowStates.size());

	// 저장된 상태로 모든 UI 윈도우 복원
	for (auto* Window : UIWindows)
	{
		if (Window)
		{
			uint32 CurrentWindowID = Window->GetWindowID();
			UE_LOG("UIManager: Restoring Window ID=%u", CurrentWindowID);

			// 저장된 상태에서 해당 윈도우 찾기
			FUIWindowSavedState* FoundState = nullptr;
			for (auto& SavedState : SavedWindowStates)
			{
				if (SavedState.WindowID == CurrentWindowID)
				{
					FoundState = &SavedState;
					break;
				}
			}

			if (FoundState)
			{
				UE_LOG(
					"UIManager: Restoring Window ID=%u: Position=(%.1f,%.1f) -> (%.1f,%.1f), Size=(%.1f,%.1f) -> (%.1f,%.1f)",
					CurrentWindowID,
					Window->GetLastWindowPosition().x, Window->GetLastWindowPosition().y,
					FoundState->SavedPosition.x, FoundState->SavedPosition.y,
					Window->GetLastWindowSize().x, Window->GetLastWindowSize().y,
					FoundState->SavedSize.x, FoundState->SavedSize.y);

				// 위치와 크기 복원
				Window->SetLastWindowPosition(FoundState->SavedPosition);
				Window->SetLastWindowSize(FoundState->SavedSize);
				UE_LOG("UIManager: %u번 윈도우에 대해 이후 10프레임 동안 복원을 시도합니다", CurrentWindowID);

				// 가시성 복원
				if (FoundState->bWasVisible)
				{
					Window->SetWindowState(EUIWindowState::Visible);
					UE_LOG("UIManager: %u번 윈도우가 Visible 상태로 복원됩니다", CurrentWindowID);
				}
				else
				{
					Window->SetWindowState(EUIWindowState::Hidden);
					UE_LOG("UIManager: %u번 윈도우가 Hidden 상태로 복원됩니다", CurrentWindowID);
				}
			}
			else
			{
				UE_LOG("UIManager: %u번 윈도우에 대한 정보를 찾을 수 없습니다", CurrentWindowID);
			}
		}
	}

	UE_LOG("UIManager: %zu개의 윈도우 상태가 복원되었습니다", SavedWindowStates.size());
	SavedWindowStates.clear();
}

/**
 * @brief 메인 메뉴바 윈도우를 등록하는 함수
 */
void UUIManager::RegisterMainMenuWindow(UMainMenuWindow* InMainMenuWindow)
{
	if (MainMenuWindow)
	{
		UE_LOG("UIManager: 메인 메뉴바 윈도우가 이미 등록되어 있습니다. 기존 윈도우를 교체합니다.");
	}

	MainMenuWindow = InMainMenuWindow;

	if (MainMenuWindow)
	{
		UE_LOG("UIManager: 메인 메뉴바 윈도우가 등록되었습니다");
	}
}

/**
 * @brief 메인 메뉴바의 높이를 반환하는 함수
 */
float UUIManager::GetMainMenuBarHeight() const
{
	if (MainMenuWindow)
	{
		return MainMenuWindow->GetMenuBarHeight();
	}

	return 0.0f;
}

void UUIManager::OnSelectedComponentChanged(UActorComponent* InSelectedComponent) const
{
	for (UUIWindow* UIWindow : UIWindows)
	{
		UIWindow->OnSelectedComponentChanged(InSelectedComponent);
	}
}

/**
 * @brief DockSpace를 생성하여 UI 윈도우들이 docking 가능하도록 함
 */
void UUIManager::CreateDockSpace()
{
	// MainMenuBar 높이를 고려하여 DockSpace 위치 계산
	const float MenuBarHeight = GetMainMenuBarHeight();

	ImGuiViewport* Viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(Viewport->Pos.x, Viewport->Pos.y + MenuBarHeight));
	ImGui::SetNextWindowSize(ImVec2(Viewport->Size.x, Viewport->Size.y - MenuBarHeight));
	ImGui::SetNextWindowViewport(Viewport->ID);

	// DockSpace 윈도우 플래그 설정
	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoDocking;
	WindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
	WindowFlags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	WindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	WindowFlags |= ImGuiWindowFlags_NoBackground;

	// 패딩 제거
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	ImGui::Begin("MainDockSpace", nullptr, WindowFlags);
	ImGui::PopStyleVar(3);

	// DockSpace 생성
	ImGuiID DockSpaceID = ImGui::GetID("MyDockSpace");
	ImGuiDockNodeFlags DockSpaceFlags = ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingInCentralNode;
	ImGui::DockSpace(DockSpaceID, ImVec2(0.0f, 0.0f), DockSpaceFlags);

	// 중앙 노드(Central Node) 정보 추적
	ImGuiDockNode* CentralNode = ImGui::DockBuilderGetCentralNode(DockSpaceID);
	if (CentralNode && CentralNode->IsEmpty())
	{
		// 중앙 노드가 비어있을 때만 (PassthruCentralNode)
		bHasCentralNode = true;
		CentralNodePos = CentralNode->Pos;
		CentralNodeSize = CentralNode->Size;
	}
	else if (CentralNode)
	{
		// 중앙 노드에 창이 도킹되어 있어도 위치/크기는 항상 업데이트
		// (Editor.cpp에서 이 값을 항상 사용하므로)
		bHasCentralNode = false;
		CentralNodePos = CentralNode->Pos;
		CentralNodeSize = CentralNode->Size;
	}
	else
	{
		// CentralNode가 nullptr인 경우, DockSpace의 전체 영역을 사용
		// (main 브랜치의 WorkPos/WorkSize와 동일한 fallback)
		bHasCentralNode = false;
		CentralNodePos = ImVec2(Viewport->Pos.x, Viewport->Pos.y + MenuBarHeight);
		CentralNodeSize = ImVec2(Viewport->Size.x, Viewport->Size.y - MenuBarHeight);
	}

	ImGui::End();
}
