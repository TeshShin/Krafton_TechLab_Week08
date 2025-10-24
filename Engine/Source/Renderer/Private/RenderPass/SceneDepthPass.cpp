#include "pch.h"
#include "Renderer/Public/RenderPass/SceneDepthPass.h"
#include "Editor/Public/Camera.h"
#include "Renderer/Public/Renderer.h"
#include "Renderer/Public/RenderResourceFactory.h"

FSceneDepthPass::FSceneDepthPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS)
    : FRenderPass(InPipeline), DS(InDS)
{
    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/Common/BlitVS.hlsl", {}, &VertexShader, nullptr);
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/ViewMode/SceneDepthPS.hlsl", &PixelShader);

    SamplerState = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
}

bool FSceneDepthPass::CanRender(const FRenderingContext& Context)
{
	return Context.ViewMode == EViewModeIndex::VMI_SceneDepth;
}

void FSceneDepthPass::SetRenderTargets(class UDeviceResources* DeviceResources)
{
	DeviceResources->SwapFrameBuffers();

	ID3D11RenderTargetView* RTVs[] = { DeviceResources->GetBackBufferRTV() };
	DepthSRV = DeviceResources->GetDepthBufferSRV();
	Pipeline->SetRenderTargets(1, RTVs, nullptr);
}

void FSceneDepthPass::Execute(FRenderingContext& Context)
{
    if (Context.ViewMode != EViewModeIndex::VMI_SceneDepth) { return; }

    auto RS = FRenderResourceFactory::GetRasterizerState( { ECullMode::None, EFillMode::Solid });
    FPipelineInfo PipelineInfo = { nullptr, VertexShader, RS, DS, PixelShader, nullptr };
    Pipeline->UpdatePipeline(PipelineInfo);

    Pipeline->SetSRV(0, EShaderType::EST_Pixel, DepthSRV);
    Pipeline->SetSamplerState(0, EShaderType::EST_Pixel, SamplerState);

    Pipeline->Draw(3, 0);
    Pipeline->SetSRV(0, EShaderType::EST_Pixel, nullptr);
}

void FSceneDepthPass::Release()
{
    SafeRelease(PixelShader);
    SafeRelease(VertexShader);
    SafeRelease(SamplerState);
}
