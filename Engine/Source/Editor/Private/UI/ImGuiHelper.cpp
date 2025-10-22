#include "pch.h"
#include "Editor/Public/UI/ImGuiHelper.h"
#include "Editor/Public/UI/ImGuiStyleHelper.h"
#include "Renderer/Public/Renderer.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "imGui/imgui_impl_win32.h"
#include "Manager/Public/PathManager.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam);

UImGuiHelper::UImGuiHelper() = default;

UImGuiHelper::~UImGuiHelper() = default;

/**
 * @brief ImGui 초기화 함수
 */
void UImGuiHelper::Initialize(HWND InWindowHandle)
{
	if (bIsInitialized)
	{
		return;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(InWindowHandle);

	// Apply Custom Dark Theme
	FImGuiStyleHelper::ApplyDarkTheme();

	ImGuiIO& IO = ImGui::GetIO();

	// Docking 활성화
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// imgui.ini 파일 경로 설정 (docking 정보 저장)
	// Config 디렉토리에 저장
	static std::string IniFilePath = (UPathManager::GetInstance().GetConfigPath() / "imgui.ini").string();
	IO.IniFilename = IniFilePath.c_str();

	path FontFilePath = UPathManager::GetInstance().GetFontPath() / "Pretendard-Regular.otf";
	IO.Fonts->AddFontFromFileTTF((char*)FontFilePath.u8string().c_str(), 16.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());

	auto& Renderer = URenderer::GetInstance();
	ImGui_ImplDX11_Init(Renderer.GetDevice(), Renderer.GetDeviceContext());

	bIsInitialized = true;
}

/**
 * @brief ImGui 자원 해제 함수
 */
void UImGuiHelper::Release()
{
	if (!bIsInitialized)
	{
		return;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	bIsInitialized = false;
}

/**
 * @brief ImGui 새 프레임 시작
 */
void UImGuiHelper::BeginFrame() const
{
	if (!bIsInitialized)
	{
		return;
	}

	// Get New Frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

/**
 * @brief ImGui 렌더링 종료 및 출력
 */
void UImGuiHelper::EndFrame() const
{
	if (!bIsInitialized)
	{
		return;
	}

	// Render ImGui
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

/**
 * @brief WndProc Handler 래핑 함수
 * @return ImGui 자체 함수 반환
 */
LRESULT UImGuiHelper::WndProcHandler(HWND hWnd, uint32 msg, WPARAM wParam, LPARAM lParam)
{
	return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}

