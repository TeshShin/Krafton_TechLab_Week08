#include "pch.h"
#include "Renderer/Public/RenderPass/SceneDepthPass.h"
#include "Editor/Public/Camera.h"
#include "Renderer/Public/Renderer.h"
#include "Renderer/Public/RenderResourceFactory.h"

FSceneDepthPass::FSceneDepthPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS)
    : FRenderPass(InPipeline, nullptr), DS(InDS)
{
    TArray<D3D11_INPUT_ELEMENT_DESC> LayoutDesc = {};

    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/SceneDepthShader.hlsl", LayoutDesc, &VertexShader, nullptr);
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/SceneDepthShader.hlsl", &PixelShader);

    SamplerState = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
    ConstantBufferPerFrame = FRenderResourceFactory::CreateConstantBuffer<FSceneDepthConstants>();
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

    FSceneDepthConstants SceneDepthConstants;
    SceneDepthConstants.RenderTarget = Context.RTSize;
    SceneDepthConstants.IsOrthographic = Context.CurrentCamera->GetCameraType() == ECameraType::ECT_Orthographic;
    FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPerFrame, SceneDepthConstants);
    Pipeline->SetConstantBuffer(0, false, ConstantBufferPerFrame);
    Pipeline->SetSRV(0, false, DepthSRV);
    Pipeline->SetSamplerState(0, false, SamplerState);

    Pipeline->Draw(3, 0);
    Pipeline->SetSRV(0, false, nullptr);
}

void FSceneDepthPass::Release()
{
    SafeRelease(PixelShader);
    SafeRelease(VertexShader);
    SafeRelease(SamplerState);
    SafeRelease(ConstantBufferPerFrame);
}
