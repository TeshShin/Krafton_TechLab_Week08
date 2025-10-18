#include "pch.h"
#include "Renderer/Public/RenderPass/NormalMapPass.h"
#include "Renderer/Public/Renderer.h"
#include "Renderer/Public/RenderResourceFactory.h"

FNormalMapPass::FNormalMapPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS)
    : FRenderPass(InPipeline,  nullptr), DS(InDS)
{
    // Fullscreen pass: no input layout required
    TArray<D3D11_INPUT_ELEMENT_DESC> LayoutDesc = {};

    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/NormalMapShader.hlsl", LayoutDesc, nullptr, &VertexShader, nullptr);
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/NormalMapShader.hlsl", nullptr, &PixelShader);

    SamplerState = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
    ConstantBufferPerFrame = FRenderResourceFactory::CreateConstantBuffer<FNormalMapConstants>();
}

bool FNormalMapPass::CanRender(const FRenderingContext& Context)
{
	return Context.ViewMode == EViewModeIndex::VMI_NormalMap;
}

void FNormalMapPass::SetRenderTargets(class UDeviceResources* DeviceResources)
{
	DeviceResources->SwapFrameBuffers();

	ID3D11RenderTargetView* RTVs[] = { DeviceResources->GetBackBufferRTV() };
	Pipeline->SetRenderTargets(1, RTVs, nullptr);
	NormalSRV = DeviceResources->GetNormalBufferSRV();
}

void FNormalMapPass::Execute(FRenderingContext& Context)
{
    auto RS = FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid });
    FPipelineInfo PipelineInfo = { nullptr, VertexShader, RS, DS, PixelShader, nullptr };
    Pipeline->UpdatePipeline(PipelineInfo);

    // Update constants and bind resources
    FNormalMapConstants NormalMapConstants = {};
    NormalMapConstants.RenderTarget = Context.RTSize;
    FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPerFrame, NormalMapConstants);

    Pipeline->SetConstantBuffer(0, false, ConstantBufferPerFrame);
    Pipeline->SetSRV(0, false, NormalSRV);
    Pipeline->SetSamplerState(0, false, SamplerState);

    // Fullscreen triangle
    Pipeline->Draw(3, 0);
    Pipeline->SetSRV(0, false, nullptr);
}

void FNormalMapPass::Release()
{
    SafeRelease(PixelShader);
    SafeRelease(VertexShader);
    SafeRelease(SamplerState);
    SafeRelease(ConstantBufferPerFrame);
}
