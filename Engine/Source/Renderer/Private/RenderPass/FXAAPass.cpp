#include "pch.h"
#include "Renderer/Public/RenderPass/FXAAPass.h"
#include "Renderer/Public/Pipeline.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Renderer/Public/DeviceResources.h"

struct FFullscreenVertex
{
    FVector2 Position;
    FVector2 UV;
};

FFXAAPass::FFXAAPass(UPipeline* InPipeline, UDeviceResources* InDeviceResources)
    :FRenderPass(InPipeline, nullptr), DeviceResources(InDeviceResources)
{
    InitializeFullscreenQuad();
    TArray<D3D11_INPUT_ELEMENT_DESC> FXAALayout =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/FXAAShader.hlsl", FXAALayout, &VertexShader, &InputLayout);
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/FXAAShader.hlsl", &PixelShader);

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
	FXAAParams.RenderTargetSize = Context.RTSize;

	// FXAA 품질 설정값을 명시적으로 업데이트
	FXAAParams.FXAASpanMax = 8.0f;
	FXAAParams.FXAAReduceMul = 1.0f / 8.0f;
	FXAAParams.FXAAReduceMin = 1.0f / 128.0f;

	FRenderResourceFactory::UpdateConstantBufferData(FXAAConstantBuffer, FXAAParams);

    FPipelineInfo PipelineInfo = {};
    PipelineInfo.InputLayout = InputLayout;
    PipelineInfo.VertexShader = VertexShader;
    PipelineInfo.PixelShader = PixelShader;
    PipelineInfo.DepthStencilState = nullptr;
    PipelineInfo.BlendState = nullptr;
    PipelineInfo.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    Pipeline->UpdatePipeline(PipelineInfo);

    Pipeline->SetVertexBuffer(FullscreenVB, FullscreenStride);
    Pipeline->SetIndexBuffer(FullscreenIB, 0);

    Pipeline->SetConstantBuffer(0, false, FXAAConstantBuffer);
    Pipeline->SetTexture(0, false, SceneSRV);
    Pipeline->SetSamplerState(0, false, SamplerState);

    Pipeline->DrawIndexed(FullscreenIndexCount, 0, 0);
    Pipeline->SetTexture(0, false, nullptr);
}

void FFXAAPass::Release()
{
    SafeRelease(FullscreenVB);
    SafeRelease(FullscreenIB);
    SafeRelease(FXAAConstantBuffer);
}

void FFXAAPass::InitializeFullscreenQuad()
{
    static const FFullscreenVertex Vertices[] =
    {
        {{-1.f,  1.f}, {0.f, 0.f}},
        {{ 1.f,  1.f}, {1.f, 0.f}},
        {{ 1.f, -1.f}, {1.f, 1.f}},
        {{-1.f, -1.f}, {0.f, 1.f}},
    };

    static const uint32 Indices[] = { 0, 1, 2, 0, 2, 3 };

    FullscreenStride = sizeof(FFullscreenVertex);
    FullscreenIndexCount = static_cast<UINT>(sizeof(Indices) / sizeof(Indices[0]));

    D3D11_BUFFER_DESC VBDesc = {};
    VBDesc.ByteWidth = sizeof(Vertices);
    VBDesc.Usage = D3D11_USAGE_IMMUTABLE;
    VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA VBData = {};
    VBData.pSysMem = Vertices;

    DeviceResources->GetDevice()->CreateBuffer(&VBDesc, &VBData, &FullscreenVB);

    D3D11_BUFFER_DESC IBDesc = {};
    IBDesc.ByteWidth = sizeof(Indices);
    IBDesc.Usage = D3D11_USAGE_IMMUTABLE;
    IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA IBData = {};
    IBData.pSysMem = Indices;

    DeviceResources->GetDevice()->CreateBuffer(&IBDesc, &IBData, &FullscreenIB);
}
