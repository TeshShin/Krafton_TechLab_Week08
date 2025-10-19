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
#include "Editor/Public/BatchLineManager.h"

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
	CreateDefaultShader();

	// 상수 버퍼 생성
	CreateConstantBuffers();

	ViewportClient->InitializeLayout(DeviceResources->GetViewportInfo());

	FStaticMeshPass* StaticMeshPass = new FStaticMeshPass(Pipeline, ConstantBufferModels, DefaultDS);
	LevelPasses.push_back(StaticMeshPass);

	FStaticMeshUnlitPass* StaticMeshUnlitPass = new FStaticMeshUnlitPass(Pipeline, ConstantBufferModels, DefaultDS);
	LevelPasses.push_back(StaticMeshUnlitPass);

	FTextPass* TextPass = new FTextPass(Pipeline, ConstantBufferModels, DefaultDS, AlphaBlendState);
	LevelPasses.push_back(TextPass);

	FBillboardPass* BillboardPass = new FBillboardPass(Pipeline, ConstantBufferModels, DefaultDS, AlphaBlendState);
	LevelPasses.push_back(BillboardPass);

	FDecalPass* DecalPass = new FDecalPass(Pipeline, ReadOnlyDS, AlphaBlendState);
	LevelPasses.push_back(DecalPass);

	FFogPass* FogPass = new FFogPass(Pipeline, DefaultDS, AlphaBlendState);
	PostProcessPasses.push_back(FogPass);

	FFXAAPass* FXAAPass = new FFXAAPass(Pipeline, DeviceResources);
	PostProcessPasses.push_back(FXAAPass);

	ViewModePasses[EViewModeIndex::VMI_Lit_Phong] = new FDefaultViewPass(Pipeline, DisabledDS);
	ViewModePasses[EViewModeIndex::VMI_Lit_Lambert] = new FDefaultViewPass(Pipeline, DisabledDS);
	ViewModePasses[EViewModeIndex::VMI_Lit_Gouraud] = new FDefaultViewPass(Pipeline, DisabledDS);
	ViewModePasses[EViewModeIndex::VMI_Unlit] = new FDefaultViewPass(Pipeline, DisabledDS);
	ViewModePasses[EViewModeIndex::VMI_Wireframe] = new FDefaultViewPass(Pipeline, DisabledDS);
	ViewModePasses[EViewModeIndex::VMI_SceneDepth] = new FSceneDepthPass(Pipeline, DisabledDS);
	ViewModePasses[EViewModeIndex::VMI_NormalMap] = new FNormalMapPass(Pipeline, DefaultDS);

	UBatchLineManager::GetInstance().Init();
}

void URenderer::Release()
{
	UBatchLineManager::GetInstance().Release();

	ReleaseConstantBuffers();
	ReleaseDefaultShader();
	ReleaseDepthStencilState();
	ReleaseBlendState();
	FRenderResourceFactory::ReleaseRasterizerState();

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

void URenderer::CreateDefaultShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> DefaultLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/SampleShader.hlsl", DefaultLayout, &DefaultVertexShader, &DefaultInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/SampleShader.hlsl", &DefaultPixelShader);
}

void URenderer::ReleaseDefaultShader()
{
	SafeRelease(DefaultInputLayout);
	SafeRelease(DefaultPixelShader);
	SafeRelease(DefaultVertexShader);
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

void URenderer::Render()
{
    RenderBegin();

    for (FViewportClient& Viewport : ViewportClient->GetViewports())
    {
	    if (Viewport.GetViewportInfo().Width < 1.0f || Viewport.GetViewportInfo().Height < 1.0f) { continue; }
    	Viewport.Apply(GetDeviceContext());
    	Viewport.Camera.Update(Viewport.GetViewportInfo());

    	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferViewProj, Viewport.Camera.GetFViewProjConstants());
    	Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);
    	Pipeline->SetConstantBuffer(1, false, ConstantBufferViewProj);

    	FRenderingContext RenderingContext(&Viewport.Camera, GEditor->GetEditorModule()->GetViewMode(),
			GEditor->GetEditorModule()->GetShowFlags(), Viewport.ViewportInfo,
    	{ DeviceResources->GetViewportInfo().Width, DeviceResources->GetViewportInfo().Height });

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
        	// @TODO Editor한테 Render를 요청하는 것이 아닌 Renderer 안에서 하도록 처리
        	ID3D11RenderTargetView* RenderTargetView[] = {GetBackBufferRTV()};
        	Pipeline->SetRenderTargets(1, RenderTargetView, GetDepthBufferDSV());
        	GEditor->GetEditorModule()->RenderEditor();
        	GEditor->GetEditorModule()->RenderGizmo(RenderingContext.CurrentCamera);

			UBatchLineManager::GetInstance().Update();
			UBatchLineManager::GetInstance().Render();

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
	constexpr float ClearColor[4] = {0.025f, 0.025f, 0.025f, 1.0f};
	constexpr float NormalClearColor[] = { 0.5f, 0.5f, 0.5f, 1.0f };

	GetDeviceContext()->ClearRenderTargetView(GetBackBufferRTV(), ClearColor);
	GetDeviceContext()->ClearRenderTargetView(GetDestinationRTV(), ClearColor);
	GetDeviceContext()->ClearRenderTargetView(GetSourceRTV(), ClearColor);
	GetDeviceContext()->ClearRenderTargetView(GetNormalBufferRTV(), NormalClearColor);
	GetDeviceContext()->ClearDepthStencilView(GetDepthBufferDSV(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    DeviceResources->UpdateViewport();
}

void URenderer::RenderLevel(struct FRenderingContext& RenderingContext)
{
	const ULevel* CurrentLevel = GWorld->GetLevel();
	if (!CurrentLevel) { return; }

	// 1. Sort visible primitive components
	TArray<UPrimitiveComponent*> FinalVisiblePrims = RenderingContext.CurrentCamera->GetViewVolumeCuller().GetRenderableObjects();
	RenderingContext.AllPrimitives = FinalVisiblePrims;
	for (const auto& Prim : FinalVisiblePrims)
	{
		// Filter by visibility before adding to RenderingContext
		if (!Prim->IsVisible()) { continue; }

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

	for (auto LevelPass: LevelPasses)
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
	for (auto PostProcessPass: PostProcessPasses)
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
}

void URenderer::RenderEditorOverlay(struct FRenderingContext& RenderingContext)
{
}

void URenderer::RenderEditorPrimitive(const FEditorPrimitive& InPrimitive, const FRenderState& InRenderState, uint32 InStride, uint32 InIndexBufferStride)
{
    // Use the global stride if InStride is 0
    const uint32 FinalStride = (InStride == 0) ? sizeof(FNormalVertex) : InStride;

    // Allow for custom shaders, fallback to default
    FPipelineInfo PipelineInfo = {
        InPrimitive.InputLayout ? InPrimitive.InputLayout : DefaultInputLayout,
        InPrimitive.VertexShader ? InPrimitive.VertexShader : DefaultVertexShader,
		FRenderResourceFactory::GetRasterizerState(InRenderState),
        InPrimitive.bShouldAlwaysVisible ? DisabledDS : DefaultDS,
        InPrimitive.PixelShader ? InPrimitive.PixelShader : DefaultPixelShader,
        nullptr,
        InPrimitive.Topology
    };
    Pipeline->UpdatePipeline(PipelineInfo);

    // Update constant buffers
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModels,
		FMatrix::GetModelMatrix(InPrimitive.Location, InPrimitive.Rotation, InPrimitive.Scale));
	Pipeline->SetConstantBuffer(0, true, ConstantBufferModels);
	Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);

	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferColor, InPrimitive.Color);
	Pipeline->SetConstantBuffer(2, false, ConstantBufferColor);
	Pipeline->SetConstantBuffer(2, true, ConstantBufferColor);

    Pipeline->SetVertexBuffer(InPrimitive.VertexBuffer, FinalStride);

    // The core logic: check for an index buffer
    if (InPrimitive.IndexBuffer && InPrimitive.NumIndices > 0)
    {
        Pipeline->SetIndexBuffer(InPrimitive.IndexBuffer, InIndexBufferStride);
        Pipeline->DrawIndexed(InPrimitive.NumIndices, 0, 0);
    }
    else
    {
        Pipeline->Draw(InPrimitive.NumVertices, 0);
    }
}

void URenderer::RenderEnd() const
{
	TIME_PROFILE(DrawCall)
	GetSwapChain()->Present(0, 0);
	TIME_PROFILE_END(DrawCall)
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
	ConstantBufferColor = FRenderResourceFactory::CreateConstantBuffer<FVector4>();
	ConstantBufferViewProj = FRenderResourceFactory::CreateConstantBuffer<FCameraConstants>();
}


void URenderer::ReleaseConstantBuffers()
{
	SafeRelease(ConstantBufferModels);
	SafeRelease(ConstantBufferColor);
	SafeRelease(ConstantBufferViewProj);
}
