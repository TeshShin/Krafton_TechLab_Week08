#include "pch.h"
#include "Renderer/Public/Renderer.h"
#include "Scene/Public/Component/StaticMeshComponent.h"
#include "Scene/Public/Component/DecalComponent.h"
#include "Scene/Public/Component/HeightFogComponent.h"
#include "Scene/Public/Component/PrimitiveComponent.h"
#include "Scene/Public/Component/BillBoardComponent.h"
#include "Scene/Public/Component/UUIDTextComponent.h"
#include "Scene/Public/Component/TextComponent.h"
#include "Editor/Public/Camera.h"
#include "Editor/Public/Editor.h"
#include "Editor/Public/Viewport.h"
#include "Editor/Public/ViewportClient.h"
#include "Editor/Public/UI/StatOverlay.h"
#include "Scene/Public/Level/Level.h"
#include "Manager/Public/UIManager.h"
#include "Manager/Public/InputManager.h"
#include "Manager/Public/TimeManager.h"
#include "Renderer/Public/RenderPass/RenderPass.h"
#include "Renderer/Public/RenderPass/BillboardPass.h"
#include "Renderer/Public/RenderPass/DecalPass.h"
#include "Renderer/Public/RenderPass/FXAAPass.h"
#include "Renderer/Public/RenderPass/FogPass.h"
#include "Renderer/Public/RenderPass/StaticMeshPass.h"
#include "Renderer/Public/RenderPass/StaticMeshUnlitPass.h"
#include "Renderer/Public/RenderPass/TextPass.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Renderer/Public/RenderPass/NormalMapPass.h"
#include "Renderer/Public/RenderPass/DefaultViewPass.h"
#include "Renderer/Public/RenderPass/SceneDepthPass.h"
#include "Renderer/Public/RenderPass/RenderingContext.h"
#include "Renderer/Public/ShaderManager.h"
#include "Editor/Public/Line/BatchLineManager.h"
#include "Renderer/Public/RenderPass/EditorDepthPass.h"
#include "Renderer/Public/RenderPass/EditorOverlayPass.h"

IMPLEMENT_SINGLETON_CLASS(URenderer, UObject)

URenderer::URenderer() = default;

URenderer::~URenderer() = default;


void URenderer::Init(HWND InWindowHandle)
{
	DeviceResources = new UDeviceResources(InWindowHandle);
	Pipeline = new UPipeline(GetDeviceContext());
	ViewportClient = new FViewport();

	// 렌더링 상태 및 리소스 생성
	CreateDepthStencilState();
	CreateBlendState();
	CreateConstantBuffers();
	CreateRenderPasses();
	ViewportClient->InitializeLayout(DeviceResources->GetViewportInfo());
	UBatchLineManager::GetInstance().Init();
}

void URenderer::Release()
{
	UBatchLineManager::GetInstance().Release();

	ReleaseConstantBuffers();
	ReleaseDepthStencilState();
	ReleaseBlendState();
	ReleaseRenderPasses();
	FRenderResourceFactory::ReleaseRasterizerState();

	SafeDelete(ViewportClient);
	SafeDelete(Pipeline);
	SafeDelete(DeviceResources);
}

void URenderer::CreateDepthStencilState()
{
	// 3D Default Depth Stencil (Depth WRITE/READ, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DefaultDescription = {};
	DefaultDescription.DepthEnable = TRUE;
	DefaultDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DefaultDescription.DepthFunc = D3D11_COMPARISON_LESS;
	DefaultDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DefaultDescription, &DefaultDS);

	// Read Only Depth Stencil (Depth READ, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DecalDescription = {};
	DecalDescription.DepthEnable = TRUE;
	DecalDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DecalDescription.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	DecalDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DecalDescription, &ReadOnlyDS);

	// Disabled Depth Stencil (Depth X, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DisabledDescription = {};
	DisabledDescription.DepthEnable = FALSE;
	DisabledDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DisabledDescription, &DisabledDS);
}

void URenderer::CreateBlendState()
{
	// Alpha Blending
	D3D11_BLEND_DESC BlendDesc = {};
	BlendDesc.RenderTarget[0].BlendEnable = TRUE;
	BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	GetDevice()->CreateBlendState(&BlendDesc, &AlphaBlendState);

	// Additive Blending
	D3D11_BLEND_DESC AdditiveBlendDesc = {};
	AdditiveBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	AdditiveBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	AdditiveBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	AdditiveBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	AdditiveBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	AdditiveBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	AdditiveBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	AdditiveBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	GetDevice()->CreateBlendState(&AdditiveBlendDesc, &AdditiveBlendState);
}

void URenderer::ReleaseDepthStencilState()
{
	SafeRelease(DefaultDS);
	SafeRelease(ReadOnlyDS);
	SafeRelease(DisabledDS);
	if (GetDeviceContext())
	{
		GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
	}
}

void URenderer::ReleaseBlendState()
{
	SafeRelease(AlphaBlendState);
	SafeRelease(AdditiveBlendState);
}

void URenderer::ReleaseRenderPasses()
{
	for (auto& RenderPass : LevelPasses)
	{
		RenderPass->Release();
		SafeDelete(RenderPass);
	}
	for (auto& RenderPass : PostProcessPasses)
	{
		RenderPass->Release();
		SafeDelete(RenderPass);
	}
	for (auto& RenderPassPair : ViewModePasses)
	{
		RenderPassPair.second->Release();
		SafeDelete(RenderPassPair.second);
	}
	EditorDepthPass->Release();
	SafeDelete(EditorDepthPass);
	EditorOverlayPass->Release();
	SafeDelete(EditorOverlayPass);

}

void URenderer::Render()
{
	// Manual shader reload via F4 key (for immediate testing)
	static bool bWasF4Pressed = false;
	bool bIsF4Pressed = UInputManager::GetInstance().IsKeyDown(EKeyInput::F4);

	if (bIsF4Pressed && !bWasF4Pressed)  // Detect key press (not hold)
	{
		UE_LOG("===== Shader Hot-Reload Triggered (F4 - Manual) =====");
		int32 ReloadedCount = FShaderManager::Get().ReloadAllShaders();
		UE_LOG("Shader Hot-Reload: %d variants recompiled", ReloadedCount);
	}
	bWasF4Pressed = bIsF4Pressed;

	// Auto shader reload (timestamp-based, periodic check)
	CheckShaderHotReload();

	RenderBegin();

	for (FViewportClient& Viewport : ViewportClient->GetViewports())
	{
		if (Viewport.GetViewportInfo().Width < 1.0f || Viewport.GetViewportInfo().Height < 1.0f) { continue; }
		Viewport.Apply(GetDeviceContext());
		Viewport.Camera.Update(Viewport.GetViewportInfo());

		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferViewProj,
		                                                 Viewport.Camera.GetFViewProjConstants());
		Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);
		Pipeline->SetConstantBuffer(1, false, ConstantBufferViewProj);

		FRenderingContext RenderingContext(&Viewport.Camera, GEditor->GetEditorModule()->GetViewMode(),
		                                   GEditor->GetEditorModule()->GetShowFlags(), Viewport.ViewportInfo,
		                                   {
			                                   DeviceResources->GetViewportInfo().Width,
			                                   DeviceResources->GetViewportInfo().Height
		                                   });

		{
			TIME_PROFILE(RenderLevel)
			RenderLevel(RenderingContext);
		}
		{
			TIME_PROFILE(RenderPostProcess)
			RenderPostProcess(RenderingContext);
		}
		{
			TIME_PROFILE(RenderViewMode)
			RenderByViewMode(RenderingContext);
		}
		{
			TIME_PROFILE(RenderEditorDepth)
			RenderEditorDepth(RenderingContext);
		}
		{
			TIME_PROFILE(RenderEditorOverlay)
			RenderEditorOverlay(RenderingContext);
		}
	}

	{
		TIME_PROFILE(UUIManager)
		UUIManager::GetInstance().Render();
	}
	{
		TIME_PROFILE(UStatOverlay)
		UStatOverlay::GetInstance().Render();
	}

	RenderEnd();
}

void URenderer::RenderBegin() const
{
	// Editor::UpdateLayout()에서 설정한 viewport 레이아웃 유지
	// (스플릿터에 따라 4개의 뷰포트가 이미 설정되어 있음)

	// Match BackgroundPrimary color from ImGuiStyleHelper (Midnight theme)
	// sRGB to Linear conversion applied: {0.08, 0.09, 0.12} sRGB → {0.0099, 0.0132, 0.0228} Linear
	// Linear buffer에서 의도한 sRGB 색상으로 올바르게 표시되도록 변환됨
	constexpr float ClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	constexpr float NormalClearColor[] = {0.5f, 0.5f, 0.5f, 1.0f};

	GetDeviceContext()->ClearRenderTargetView(GetBackBufferRTV(), ClearColor);
	GetDeviceContext()->ClearRenderTargetView(GetDestinationRTV(), ClearColor);
	GetDeviceContext()->ClearRenderTargetView(GetSourceRTV(), ClearColor);
	GetDeviceContext()->ClearRenderTargetView(GetNormalBufferRTV(), NormalClearColor);
	GetDeviceContext()->ClearDepthStencilView(GetDepthBufferDSV(), D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void URenderer::RenderLevel(struct FRenderingContext& RenderingContext)
{
	const ULevel* CurrentLevel = GWorld->GetLevel();
	if (!CurrentLevel) { return; }

	// 1. Sort visible primitive components
	TArray<UPrimitiveComponent*> FinalVisiblePrims = RenderingContext.CurrentCamera->GetViewVolumeCuller().
	                                                                  GetRenderableObjects();
	RenderingContext.AllPrimitives = FinalVisiblePrims;
	for (const auto& Prim : FinalVisiblePrims)
	{
		// Filter by hierarchical visibility before adding to RenderingContext
		if (!Prim->IsVisibleInHierarchy()) { continue; }

		if (auto StaticMesh = Cast<UStaticMeshComponent>(Prim))
		{
			RenderingContext.StaticMeshes.push_back(StaticMesh);
		}
		else if (auto BillBoard = Cast<UBillBoardComponent>(Prim))
		{
			RenderingContext.BillBoards.push_back(BillBoard);
		}
		else if (auto Text = Cast<UTextComponent>(Prim))
		{
			if (!Text->IsExactly(UUUIDTextComponent::StaticClass())) { RenderingContext.Texts.push_back(Text); }
			else { RenderingContext.UUIDs.push_back(Cast<UUUIDTextComponent>(Text)); }
		}
		else if (auto Decal = Cast<UDecalComponent>(Prim))
		{
			RenderingContext.Decals.push_back(Decal);
		}
	}

	RenderingContext.Lights = CurrentLevel->GetLights();

	// 2. Collect HeightFogComponents from all actors in the level
	for (const auto& Actor : CurrentLevel->GetLevelActors())
	{
		for (const auto& Component : Actor->GetOwnedComponents())
		{
			if (auto Fog = Cast<UHeightFogComponent>(Component))
			{
				RenderingContext.Fogs.push_back(Fog);
			}
		}
	}

	for (auto LevelPass : LevelPasses)
	{
		if (LevelPass->CanRender(RenderingContext))
		{
			LevelPass->SetRenderTargets(DeviceResources);
			LevelPass->Execute(RenderingContext);
		}
	}
}

void URenderer::RenderPostProcess(struct FRenderingContext& RenderingContext)
{
	for (auto PostProcessPass : PostProcessPasses)
	{
		if (PostProcessPass->CanRender(RenderingContext))
		{
			PostProcessPass->SetRenderTargets(DeviceResources);
			PostProcessPass->Execute(RenderingContext);
		}
	}
}

void URenderer::RenderByViewMode(struct FRenderingContext& RenderingContext)
{
	FRenderPass* ViewModePass = ViewModePasses[RenderingContext.ViewMode];
	if (ViewModePass->CanRender(RenderingContext))
	{
		ViewModePass->SetRenderTargets(DeviceResources);
		ViewModePass->Execute(RenderingContext);
	}
}

void URenderer::RenderEditorDepth(struct FRenderingContext& RenderingContext)
{
	UBatchLineManager::GetInstance().Update();
	if (EditorDepthPass->CanRender(RenderingContext))
	{
		EditorDepthPass->SetRenderTargets(DeviceResources);
		EditorDepthPass->Execute(RenderingContext);
	}
}

void URenderer::RenderEditorOverlay(struct FRenderingContext& RenderingContext)
{
	if (EditorOverlayPass->CanRender(RenderingContext))
	{
		EditorOverlayPass->SetRenderTargets(DeviceResources);
		EditorOverlayPass->Execute(RenderingContext);
	}
}

void URenderer::RenderEnd() const
{
	TIME_PROFILE(DrawCall)
	GetSwapChain()->Present(0, 0);
	TIME_PROFILE_END(DrawCall)
}

void URenderer::CheckShaderHotReload()
{
	// Accumulate time
	ShaderCheckAccumulator += UTimeManager::GetInstance().GetDeltaTime();

	// Check periodically (not every frame for performance)
	if (ShaderCheckAccumulator >= ShaderCheckInterval)
	{
		ShaderCheckAccumulator = 0.0f;

		// Check all tracked shader files for modifications
		int32 ModifiedFileCount = FShaderManager::Get().CheckAndReloadModifiedShaders();

		// Log only if files were actually modified
		if (ModifiedFileCount > 0)
		{
			UE_LOG("===== Shader Auto-Reload: %d file(s) detected and recompiled =====", ModifiedFileCount);
		}
	}
}

void URenderer::OnResize(uint32 InWidth, uint32 InHeight) const
{
	if (!DeviceResources || !GetDeviceContext() || !GetSwapChain()) return;

	Pipeline->SetRenderTargets(0, nullptr, nullptr);
	DeviceResources->ReleaseBuffers();

	if (FAILED(GetSwapChain()->ResizeBuffers(2, InWidth, InHeight, DXGI_FORMAT_UNKNOWN, 0)))
	{
		UE_LOG("OnResize Failed");
		return;
	}

	DeviceResources->UpdateViewport();
	DeviceResources->CreateBuffers();
}


void URenderer::CreateConstantBuffers()
{
	ConstantBufferModels = FRenderResourceFactory::CreateConstantBuffer<FModelConstants>();
	ConstantBufferViewProj = FRenderResourceFactory::CreateConstantBuffer<FCameraConstants>();
}

void URenderer::CreateRenderPasses()
{
	auto StaticMeshPass = new FStaticMeshPass(Pipeline, ConstantBufferModels, DefaultDS);
	LevelPasses.push_back(StaticMeshPass);

	auto StaticMeshUnlitPass = new FStaticMeshUnlitPass(Pipeline, ConstantBufferModels, DefaultDS);
	LevelPasses.push_back(StaticMeshUnlitPass);

	auto TextPass = new FTextPass(Pipeline, ConstantBufferModels, DefaultDS, AlphaBlendState);
	LevelPasses.push_back(TextPass);

	auto BillboardPass = new FBillboardPass(Pipeline, ConstantBufferModels, DefaultDS, AlphaBlendState);
	LevelPasses.push_back(BillboardPass);

	auto DecalPass = new FDecalPass(Pipeline, ReadOnlyDS, AlphaBlendState);
	LevelPasses.push_back(DecalPass);

	auto FogPass = new FFogPass(Pipeline, DefaultDS, AlphaBlendState);
	PostProcessPasses.push_back(FogPass);

	auto FXAAPass = new FFXAAPass(Pipeline, DeviceResources);
	PostProcessPasses.push_back(FXAAPass);

	ViewModePasses[EViewModeIndex::VMI_Lit_Phong] = new FDefaultViewPass(Pipeline, DisabledDS);
	ViewModePasses[EViewModeIndex::VMI_Lit_Lambert] = new FDefaultViewPass(Pipeline, DisabledDS);
	ViewModePasses[EViewModeIndex::VMI_Lit_Gouraud] = new FDefaultViewPass(Pipeline, DisabledDS);
	ViewModePasses[EViewModeIndex::VMI_Unlit] = new FDefaultViewPass(Pipeline, DisabledDS);
	ViewModePasses[EViewModeIndex::VMI_Wireframe] = new FDefaultViewPass(Pipeline, DisabledDS);
	ViewModePasses[EViewModeIndex::VMI_SceneDepth] = new FSceneDepthPass(Pipeline, DisabledDS);
	ViewModePasses[EViewModeIndex::VMI_NormalMap] = new FNormalMapPass(Pipeline, DefaultDS);
	EditorDepthPass = new FEditorDepthPass(Pipeline, ConstantBufferModels, DefaultDS);
	EditorOverlayPass = new FEditorOverlayPass(Pipeline, ConstantBufferModels, DisabledDS);
}

void URenderer::ReleaseConstantBuffers()
{
	SafeRelease(ConstantBufferModels);
	SafeRelease(ConstantBufferViewProj);
}
