#include "pch.h"
#include "Renderer/Public/RenderPass/DefaultViewPass.h"
#include "Renderer/Public/RenderResourceFactory.h"

FDefaultViewPass::FDefaultViewPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS)
	: FRenderPass(InPipeline,  nullptr), DS(InDS)
{
	// Fullscreen pass: no input layout required
	TArray<D3D11_INPUT_ELEMENT_DESC> LayoutDesc = {};

	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/BlitShader.hlsl", LayoutDesc, &VertexShader, nullptr);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/BlitShader.hlsl", &PixelShader);

	SamplerState = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
	ConstantBufferPerFrame = FRenderResourceFactory::CreateConstantBuffer<FVector2>();
}

bool FDefaultViewPass::CanRender(const FRenderingContext& Context)
{
	// ViewportClientWindow가 활성화되어 있으면 BackBuffer로 복사하지 않음
	// (ViewportClientWindow가 FrameBuffer를 직접 표시)
	return false;
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

	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPerFrame, Context.RTSize);

	Pipeline->SetConstantBuffer(0, false, ConstantBufferPerFrame);
	Pipeline->SetSRV(0, false, SceneSRV);
	Pipeline->SetSamplerState(0, false, SamplerState);

	// Fullscreen triangle
	Pipeline->Draw(3, 0);
	Pipeline->SetSRV(0, false, nullptr);
}

void FDefaultViewPass::Release()
{
	SafeRelease(SamplerState);
	SafeRelease(ConstantBufferPerFrame);
}
