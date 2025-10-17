#include "pch.h"
#include "Scene/Public/Component/PointLightComponent.h"
#include "Editor/Public/Camera.h"
#include "Renderer/Public/RenderPass/PointLightPass.h"
#include "Renderer/Public/Renderer.h"
#include "Renderer/Public/RenderResourceFactory.h"

FPointLightPass::FPointLightPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS, ID3D11BlendState* InBS)
        : FRenderPass(InPipeline, nullptr, nullptr), DS(InDS), BS(InBS)
{
    FNormalVertex NormalVertices[3] = {};
    NormalVertices[0].Position = { -1.0f, -1.0f, 0.0f };
    NormalVertices[1].Position = { 3.0f, -1.0f, 0.0f };
    NormalVertices[2].Position = { -1.0f, 3.0f, 0.0f };
    
    TArray<D3D11_INPUT_ELEMENT_DESC> PointLightLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
    };
    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/PointLightShader.hlsl", PointLightLayout, &VS, &InputLayout);
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/PointLightShader.hlsl", &PS);
    
    VertexBuffer = FRenderResourceFactory::CreateVertexBuffer(NormalVertices, sizeof(NormalVertices));
    ConstantBufferPerFrame = FRenderResourceFactory::CreateConstantBuffer<FPointLightPerFrame>();
    ConstantBufferPointLightData = FRenderResourceFactory::CreateConstantBuffer<FPointLightData>();
    PointLightSampler = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
}

void FPointLightPass::Execute(FRenderingContext& Context)
{
    if (Context.ViewMode != EViewModeIndex::VMI_Lit)
    {
        return;
    }
    
	const auto& Renderer = URenderer::GetInstance();
    const auto& DeviceResources = Renderer.GetDeviceResources();
    ID3D11RenderTargetView* RTV = nullptr;
    if (Renderer.GetFXAA())
    {
        RTV = DeviceResources->GetSceneColorRenderTargetView();	
    }
    else
    {
        RTV = DeviceResources->GetRenderTargetView();	
    }
    ID3D11RenderTargetView* RTVs[1] = { RTV };
    Pipeline->SetRenderTargets(1, RTVs, nullptr);
    auto RS = FRenderResourceFactory::GetRasterizerState( { ECullMode::None, EFillMode::Solid }); 

    FPipelineInfo PipelineInfo = { InputLayout, VS, RS, DS, PS, BS };
    Pipeline->UpdatePipeline(PipelineInfo);
    Pipeline->SetVertexBuffer(VertexBuffer, sizeof(FNormalVertex));

    if (Renderer.GetFXAA())
    {
        Pipeline->SetTexture(0, false, DeviceResources->GetSceneColorShaderResourceView());
    }
    else
    {
        Pipeline->SetTexture(0, false, DeviceResources->GetSceneColorSRV());
    }
    Pipeline->SetTexture(1, false, DeviceResources->GetNormalSRV());
    Pipeline->SetTexture(2, false, DeviceResources->GetDepthSRV());
    Pipeline->SetSamplerState(0, false, PointLightSampler);

    for (auto PointLight : Context.PointLights)
    {
        // if (!PointLight || !PointLight->IsVisible()) { continue; }
        if (!PointLight) { continue; }

        auto ViewProjConstantsInverse = Context.CurrentCamera->GetFViewProjConstantsInverse();
        
        FPointLightPerFrame PointLightPerFrame = {};
        PointLightPerFrame.InvView = ViewProjConstantsInverse.View;
        PointLightPerFrame.InvProjection = ViewProjConstantsInverse.Projection;
        PointLightPerFrame.CameraLocation = Context.CurrentCamera->GetLocation();

        const auto& DeviceResources = URenderer::GetInstance().GetDeviceResources();
        const D3D11_VIEWPORT& ViewportInfo = Context.Viewport;
        PointLightPerFrame.Viewport = FVector4(ViewportInfo.TopLeftX, ViewportInfo.TopLeftY, ViewportInfo.Width, ViewportInfo.Height);
        PointLightPerFrame.RenderTargetSize = FVector2(Context.RenderTargetSize.X, Context.RenderTargetSize.Y);

        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPerFrame, PointLightPerFrame);
        Pipeline->SetConstantBuffer(0, false, ConstantBufferPerFrame);
        
        FPointLightData PointLightData = {};
        PointLightData.LightLocation = PointLight->GetWorldLocation();
        PointLightData.LightIntensity = PointLight->GetIntensity();
        PointLightData.LightColor = PointLight->GetLightColor();
        PointLightData.LightRadius = PointLight->GetSourceRadius();
        PointLightData.LightFalloffExtent = PointLight->GetLightFalloffExtent();

        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPointLightData, PointLightData);
        Pipeline->SetConstantBuffer(1, false, ConstantBufferPointLightData);

        Pipeline->Draw(3, 0);
    }
    ID3D11DepthStencilView* DSV = DeviceResources->GetDepthStencilView();
    Pipeline->SetRenderTargets(1, RTVs, DSV);
}

void FPointLightPass::Release()
{
    SafeRelease(VS);
    SafeRelease(PS);
    SafeRelease(InputLayout);
    
    SafeRelease(VertexBuffer);
    SafeRelease(ConstantBufferPerFrame);
    SafeRelease(ConstantBufferPointLightData);
    SafeRelease(PointLightSampler);
}