#include "pch.h"
#include "Renderer/Public/RenderPass/DefaultViewPass.h"
#include "Renderer/Public/RenderResourceFactory.h"

FDefaultViewPass::FDefaultViewPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS)
	: FRenderPass(InPipeline), DS(InDS)
{

	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/Common/BlitVS.hlsl", {}, &VertexShader, nullptr);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/ViewMode/BlitPS.hlsl", &PixelShader);

	SamplerState = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
}

bool FDefaultViewPass::CanRender(const FRenderingContext& Context)
{
	return true;
}

void FDefaultViewPass::SetRenderTargets(class UDeviceResources* DeviceResources)
{
	DeviceResources->SwapFrameBuffers();

	ID3D11RenderTargetView* RTVs[] = { DeviceResources->GetBackBufferRTV() };
	SceneSRV = DeviceResources->GetSourceSRV();
	Pipeline->SetRenderTargets(1, RTVs, nullptr);
}

void FDefaultViewPass::Execute(FRenderingContext& Context)
{
	auto RS = FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid });
	FPipelineInfo PipelineInfo = { nullptr, VertexShader, RS, DS, PixelShader, nullptr };
	Pipeline->UpdatePipeline(PipelineInfo);

	Pipeline->SetSRV(0, EShaderType::EST_Pixel, SceneSRV);
	Pipeline->SetSamplerState(0, EShaderType::EST_Pixel, SamplerState);

	// Fullscreen triangle
	Pipeline->Draw(3, 0);
	Pipeline->SetSRV(0, EShaderType::EST_Pixel, nullptr);
}

void FDefaultViewPass::Release()
{
	SafeRelease(SamplerState);
}
