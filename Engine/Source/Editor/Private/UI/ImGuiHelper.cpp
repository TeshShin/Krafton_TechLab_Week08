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

	// imgui.ini 파일 경로 설정 (유니코드 경로 지원)
	// Config 디렉토리에 저장
	ImGuiIniFilePath = UPathManager::GetInstance().GetConfigPath() / L"imgui.ini";

	// ImGui 자동 저장 비활성화 (유니코드 경로 지원을 위해 수동 처리)
	IO.IniFilename = nullptr;

	// 기존 ini 파일 로드
	LoadImGuiIni();

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

	// ImGui 설정 저장 (유니코드 경로 지원)
	SaveImGuiIni();

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

/**
 * @brief ImGui ini 파일 로드 (유니코드 경로 지원)
 */
void UImGuiHelper::LoadImGuiIni()
{
	try
	{
		if (!std::filesystem::exists(ImGuiIniFilePath))
		{
			return;
		}

		std::ifstream File(ImGuiIniFilePath, std::ios::binary | std::ios::ate);
		if (!File.is_open())
		{
			UE_LOG("ImGuiHelper: Failed to open imgui.ini for reading");
			return;
		}

		std::streamsize Size = File.tellg();
		File.seekg(0, std::ios::beg);

		std::vector<char> Buffer(Size + 1);
		if (File.read(Buffer.data(), Size))
		{
			Buffer[Size] = '\0';
			ImGui::LoadIniSettingsFromMemory(Buffer.data(), Size);
		}
		else
		{
			UE_LOG("ImGuiHelper: Failed to read imgui.ini");
		}
	}
	catch (const std::exception& e)
	{
		UE_LOG("ImGuiHelper: Exception while loading imgui.ini: %s", e.what());
	}
}

/**
 * @brief ImGui ini 파일 저장 (유니코드 경로 지원)
 */
void UImGuiHelper::SaveImGuiIni()
{
	try
	{
		path ConfigDir = ImGuiIniFilePath.parent_path();
		if (!std::filesystem::exists(ConfigDir))
		{
			std::filesystem::create_directories(ConfigDir);
		}

		size_t IniSize = 0;
		const char* IniData = ImGui::SaveIniSettingsToMemory(&IniSize);

		if (IniData && IniSize > 0)
		{
			std::ofstream File(ImGuiIniFilePath, std::ios::binary);
			if (File.is_open())
			{
				File.write(IniData, IniSize);
				File.flush();
				File.close();
			}
			else
			{
				UE_LOG("ImGuiHelper: Failed to save imgui.ini");
			}
		}
	}
	catch (const std::exception& e)
	{
		UE_LOG("ImGuiHelper: Exception while saving imgui.ini: %s", e.what());
	}
}

