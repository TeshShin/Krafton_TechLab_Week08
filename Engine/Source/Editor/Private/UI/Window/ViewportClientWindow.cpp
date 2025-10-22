#include "pch.h"
#include "Editor/Public/UI/Window/ViewportClientWindow.h"
#include "Editor/Public/UI/Widget/ViewportMenuBarWidget.h"
#include "Renderer/Public/Renderer.h"
#include "Manager/Public/InputManager.h"

IMPLEMENT_CLASS(UViewportClientWindow, UUIWindow)

UViewportClientWindow::UViewportClientWindow()
{
	SetupConfig();

	// 위젯 생성 및 초기화
	if (UViewportMenuBarWidget* MenuBarWidget = NewObject<UViewportMenuBarWidget>())
	{
		ViewportMenuBarWidget = MenuBarWidget;
		if (FViewport* ViewportClient = URenderer::GetInstance().GetViewportClient())
		{
			ViewportMenuBarWidget->SetViewportClient(ViewportClient);
			ViewportMenuBarWidget->Initialize();
			AddWidget(ViewportMenuBarWidget);
			UE_LOG("ViewportClientWindow: ViewportMenuBarWidget이 생성되고 초기화되었습니다");
		}
	}
	else
	{
		UE_LOG("ViewportClientWindow: Error: ViewportMenuBarWidget 생성에 실패했습니다!");
		return;
	}

	UE_LOG("ViewportClientWindow: 메인 메뉴 윈도우가 초기화되었습니다");
}

void UViewportClientWindow::Initialize()
{
	UE_LOG("ViewportClientWindow: Window가 성공적으로 생성되었습니다.");

	SetWindowState(EUIWindowState::Visible);
}

void UViewportClientWindow::RenderWindow()
{
	if (!IsVisible())
	{
		return;
	}

	// 도킹 설정 적용 (중요!)
	ApplyDockingSettings();

	const FUIWindowConfig& Config = GetConfig();
	float MenuBarOffset = GetMenuBarOffset();

	// ImGui 윈도우 시작
	ImVec2 AdjustedDefaultPosition = Config.DefaultPosition;
	AdjustedDefaultPosition.y = max(AdjustedDefaultPosition.y, MenuBarOffset);

	ImGui::SetNextWindowSize(Config.DefaultSize, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(AdjustedDefaultPosition, ImGuiCond_FirstUseEver);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

	bool bIsOpen = true;
	if (ImGui::Begin(Config.WindowTitle.ToString().data(), &bIsOpen, Config.WindowFlags))
	{
		// 뷰포트 이미지 렌더링
		RenderViewportImage();

		// 마우스 좌표 업데이트
		UpdateViewportMousePosition();

		// 윈도우 정보 업데이트
		UpdateWindowInfo();
	}

	ImGui::End();
	ImGui::PopStyleVar();

	if (!bIsOpen && OnWindowClose())
	{
		SetWindowState(EUIWindowState::Hidden);
	}
}

void UViewportClientWindow::RenderViewportImage()
{
	// FrameBuffer 텍스처를 ImGui Image로 표시 (3D 씬 렌더링 결과)
	auto* DeviceResources = URenderer::GetInstance().GetDeviceResources();
	ID3D11ShaderResourceView* ViewportTexture = DeviceResources->GetSourceSRV();

	if (ViewportTexture)
	{
		// 사용 가능한 영역 계산
		ImVec2 AvailableSize = ImGui::GetContentRegionAvail();

		if (AvailableSize.x > 0 && AvailableSize.y > 0)
		{
			// 텍스처의 실제 크기 가져오기
			ID3D11Resource* Resource = nullptr;
			ViewportTexture->GetResource(&Resource);

			ID3D11Texture2D* Texture = nullptr;
			if (Resource)
			{
				Resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&Texture));
			}

			ImVec2 ImageSize = AvailableSize;
			if (Texture)
			{
				D3D11_TEXTURE2D_DESC Desc;
				Texture->GetDesc(&Desc);

				// 텍스처를 윈도우 크기에 맞춰 표시 (종횡비 유지)
				float AspectRatio = static_cast<float>(Desc.Width) / static_cast<float>(Desc.Height);
				float WindowAspect = AvailableSize.x / AvailableSize.y;

				if (AspectRatio > WindowAspect)
				{
					ImageSize.y = AvailableSize.x / AspectRatio;
				}
				else
				{
					ImageSize.x = AvailableSize.y * AspectRatio;
				}

				Texture->Release();
			}

			if (Resource)
			{
				Resource->Release();
			}

			// 중앙 정렬
			ImVec2 CursorPos = ImGui::GetCursorPos();
			CursorPos.x += (AvailableSize.x - ImageSize.x) * 0.5f;
			CursorPos.y += (AvailableSize.y - ImageSize.y) * 0.5f;
			ImGui::SetCursorPos(CursorPos);

			// 뷰포트 이미지 위치 저장 (마우스 좌표 변환용)
			ViewportImagePos = ImGui::GetCursorScreenPos();
			ViewportImageSize = ImageSize;

			// 뷰포트 텍스처 표시
			ImGui::Image(reinterpret_cast<ImTextureID>(ViewportTexture), ImageSize);

			// 뷰포트가 호버되었는지 체크
			bIsViewportHovered = ImGui::IsItemHovered();
			bIsViewportFocused = ImGui::IsWindowFocused();
		}
	}
	else
	{
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "Viewport Texture Not Available");
	}
}

void UViewportClientWindow::UpdateViewportMousePosition()
{
	if (!bIsViewportHovered)
	{
		return;
	}

	// ImGui 마우스 위치 가져오기
	ImVec2 MousePosGlobal = ImGui::GetMousePos();

	// 뷰포트 이미지 로컬 좌표로 변환
	ImVec2 MousePosLocal;
	MousePosLocal.x = MousePosGlobal.x - ViewportImagePos.x;
	MousePosLocal.y = MousePosGlobal.y - ViewportImagePos.y;

	// 범위 체크
	if (MousePosLocal.x >= 0 && MousePosLocal.x <= ViewportImageSize.x &&
		MousePosLocal.y >= 0 && MousePosLocal.y <= ViewportImageSize.y)
	{
		// 텍스처의 실제 크기로 스케일 (ViewportImageSize는 화면상 크기)
		auto* DeviceResources = URenderer::GetInstance().GetDeviceResources();
		uint32 TextureWidth = DeviceResources->GetWidth();
		uint32 TextureHeight = DeviceResources->GetHeight();

		if (ViewportImageSize.x > 0 && ViewportImageSize.y > 0)
		{
			float ScaleX = static_cast<float>(TextureWidth) / ViewportImageSize.x;
			float ScaleY = static_cast<float>(TextureHeight) / ViewportImageSize.y;

			ViewportMousePosition = FVector(
				MousePosLocal.x * ScaleX,
				MousePosLocal.y * ScaleY,
				0.0f
			);
		}
	}
}

void UViewportClientWindow::SetupConfig()
{
	const D3D11_VIEWPORT& ViewportInfo = URenderer::GetInstance().GetDeviceResources()->GetViewportInfo();
	FUIWindowConfig Config;
	Config.WindowTitle = "Viewport";
	Config.DefaultSize = ImVec2(800, 600);
	Config.DefaultPosition = ImVec2(50, 100);
	Config.DockDirection = EUIDockDirection::None;
	Config.Priority = 100; // EditorWindow(0)보다 높게 설정하여 나중에 렌더링
	Config.bResizable = true;
	Config.bMovable = true;
	Config.bCollapsible = true;

	// 뷰포트 윈도우 플래그 설정
	Config.WindowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

	SetConfig(Config);

	UE_LOG("ViewportClientWindow: Config 설정 완료 - Priority: %d", Config.Priority);
}
