#include "pch.h"
#include "Renderer/Public/RenderPass/FXAAPass.h"
#include "Renderer/Public/Pipeline.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Renderer/Public/DeviceResources.h"

FFXAAPass::FFXAAPass(UPipeline* InPipeline, UDeviceResources* InDeviceResources)
    :FRenderPass(InPipeline), DeviceResources(InDeviceResources)
{
    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/Common/BlitVS.hlsl", {}, &VertexShader, nullptr);
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/PostProcess/FXAAPS.hlsl", &PixelShader);

    SamplerState = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
    FXAAConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<FFXAAConstants>();
}

FFXAAPass::~FFXAAPass()
{
    Release();
}

bool FFXAAPass::CanRender(const FRenderingContext& Context)
{
	return Context.ShowFlags & EEngineShowFlags::SF_FXAA;
}

void FFXAAPass::SetRenderTargets(class UDeviceResources* DeviceResources)
{
	// PP는 Swap Buffer
	DeviceResources->SwapFrameBuffers();

	ID3D11RenderTargetView* RTVs[] = { DeviceResources->GetDestinationRTV() };
	Pipeline->SetRenderTargets(1, RTVs, nullptr);
	SceneSRV = DeviceResources->GetSourceSRV();
}

void FFXAAPass::Execute(FRenderingContext& Context)
{
	FXAAParams.InvResolution = FVector2(1.0f / Context.RTSize.X, 1.0f / Context.RTSize.Y);

	// FXAA 품질 설정값을 명시적으로 업데이트
	FXAAParams.FXAASpanMax = 8.0f;
	FXAAParams.FXAAReduceMul = 1.0f / 8.0f;
	FXAAParams.FXAAReduceMin = 1.0f / 128.0f;

	FRenderResourceFactory::UpdateConstantBufferData(FXAAConstantBuffer, FXAAParams);

    FPipelineInfo PipelineInfo = {};
    PipelineInfo.VertexShader = VertexShader;
    PipelineInfo.PixelShader = PixelShader;
    PipelineInfo.DepthStencilState = nullptr;
    PipelineInfo.BlendState = nullptr;
    PipelineInfo.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    Pipeline->UpdatePipeline(PipelineInfo);

    Pipeline->SetConstantBuffer(0, EShaderType::EST_Pixel, FXAAConstantBuffer);
    Pipeline->SetSRV(0, EShaderType::EST_Pixel, SceneSRV);
    Pipeline->SetSamplerState(0, EShaderType::EST_Pixel, SamplerState);

	Pipeline->Draw(3, 0);
    Pipeline->SetSRV(0, EShaderType::EST_Pixel, nullptr);
}

void FFXAAPass::Release()
{
	SafeRelease(VertexShader);
	SafeRelease(PixelShader);
	SafeRelease(SamplerState);
    SafeRelease(FXAAConstantBuffer);
}
