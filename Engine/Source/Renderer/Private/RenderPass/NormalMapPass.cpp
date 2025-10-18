#include "pch.h"
#include "Renderer/Public/RenderPass/NormalMapPass.h"
#include "Renderer/Public/Renderer.h"
#include "Renderer/Public/RenderResourceFactory.h"

FNormalMapPass::FNormalMapPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11DepthStencilState* InDS)
    : FRenderPass(InPipeline, InConstantBufferViewProj, nullptr), DS(InDS)
{
    // Fullscreen pass: no input layout required
    TArray<D3D11_INPUT_ELEMENT_DESC> LayoutDesc = {};

    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/NormalMapShader.hlsl", LayoutDesc, nullptr, &VertexShader, nullptr);
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/NormalMapShader.hlsl", nullptr, &PixelShader);

    SamplerState = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
    ConstantBufferPerFrame = FRenderResourceFactory::CreateConstantBuffer<FNormalMapConstants>();
}

void FNormalMapPass::Execute(FRenderingContext& Context)
{
    if (Context.ViewMode != EViewModeIndex::VMI_NormalMap)
    {
        return;
    }

    const auto& Renderer = URenderer::GetInstance();
    const auto& DeviceResources = Renderer.GetDeviceResources();

    ID3D11RenderTargetView* RTV = Renderer.GetFXAA() ? DeviceResources->GetSceneColorRenderTargetView()
                                                     : DeviceResources->GetRenderTargetView();
    ID3D11RenderTargetView* RTVs[] = { RTV };
    Pipeline->SetRenderTargets(1, RTVs, nullptr);

    auto RS = FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid });
    FPipelineInfo PipelineInfo = { nullptr, VertexShader, RS, DS, PixelShader, nullptr };
    Pipeline->UpdatePipeline(PipelineInfo);

    // Update constants and bind resources
    FNormalMapConstants NormalMapConstants = {};
    NormalMapConstants.RenderTarget = FVector2(Context.RenderTargetSize.X, Context.RenderTargetSize.Y);
    FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPerFrame, NormalMapConstants);

    Pipeline->SetConstantBuffer(0, false, ConstantBufferPerFrame);
    Pipeline->SetTexture(0, false, DeviceResources->GetNormalSRV());
    Pipeline->SetSamplerState(0, false, SamplerState);

    // Fullscreen triangle
    Pipeline->Draw(3, 0);

    // Restore depth target
    ID3D11DepthStencilView* DSV = DeviceResources->GetDepthStencilView();
    Pipeline->SetRenderTargets(1, RTVs, DSV);
}

void FNormalMapPass::Release()
{
    SafeRelease(PixelShader);
    SafeRelease(VertexShader);
    SafeRelease(SamplerState);
    SafeRelease(ConstantBufferPerFrame);
}

