#include "pch.h"
#include "Renderer/Public/RenderPass/FogPass.h"
#include "Scene/Public/Component/HeightFogComponent.h"
#include "Editor/Public/Camera.h"
#include "Renderer/Public/RenderResourceFactory.h"

FFogPass::FFogPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS, ID3D11BlendState* InBlendState)
        : FRenderPass(InPipeline, nullptr), DS(InDS), BS(InBlendState)
{
    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/HeightFogShader.hlsl", {}, &VS, nullptr);
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/HeightFogShader.hlsl", &PS);
    ConstantBufferFog = FRenderResourceFactory::CreateConstantBuffer<FFogConstants>();
    ConstantBufferCameraInverse = FRenderResourceFactory::CreateConstantBuffer<FCameraInverseConstants>();
    ConstantBufferViewportInfo = FRenderResourceFactory::CreateConstantBuffer<FViewportConstants>();
	Sampler = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP);
}

bool FFogPass::CanRender(const FRenderingContext& Context)
{
    return Context.ShowFlags & EEngineShowFlags::SF_Fog;
}

void FFogPass::SetRenderTargets(class UDeviceResources* DeviceResources)
{
	//DeviceResources->SwapFrameBuffers();

	ID3D11RenderTargetView* RTVs[] = { DeviceResources->GetDestinationRTV() };
	DepthSRV = DeviceResources->GetDepthBufferSRV();
	Pipeline->SetRenderTargets(1, RTVs, nullptr);
}

void FFogPass::Execute(FRenderingContext& Context)
{
	TIME_PROFILE(FogPass)

    FPipelineInfo PipelineInfo = { nullptr, VS, FRenderResourceFactory::GetRasterizerState({ ECullMode::Back, EFillMode::Solid }),DS, PS, BS };
    Pipeline->UpdatePipeline(PipelineInfo);

	Pipeline->SetTexture(0, false, DepthSRV);
	Pipeline->SetSamplerState(0, false, Sampler);

	// Update CameraInverse Constant Buffer (Slot 1)
	FCameraInverseConstants CameraInverseConstants;
	FCameraConstants ViewProjConstants = Context.CurrentCamera->GetFViewProjConstantsInverse();
	CameraInverseConstants.ProjectionInverse =  ViewProjConstants.Projection;
	CameraInverseConstants.ViewInverse =  ViewProjConstants.View;
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferCameraInverse, CameraInverseConstants);
	Pipeline->SetConstantBuffer(3, false, ConstantBufferCameraInverse);

	// Update ViewportInfo Constant Buffer (Slot 2)
	FViewportConstants ViewportConstants;
	ViewportConstants.RenderTargetSize = Context.RTSize;
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferViewportInfo, ViewportConstants);
	Pipeline->SetConstantBuffer(2, false, ConstantBufferViewportInfo);

    // --- Draw Fog --- //
    for (UHeightFogComponent* Fog : Context.Fogs)
    {
        // Update Fog Constant Buffer (Slot 0)
        FFogConstants FogConstant;
        FVector Color3 = Fog->GetFogInscatteringColor();
        FogConstant.FogColor = FVector4(Color3.X, Color3.Y, Color3.Z, 1.0f);
        FogConstant.FogDensity = Fog->GetFogDensity();
        FogConstant.FogHeightFalloff = Fog->GetFogHeightFalloff();
        FogConstant.StartDistance = Fog->GetStartDistance();
        FogConstant.FogCutoffDistance = Fog->GetFogCutoffDistance();
        FogConstant.FogMaxOpacity = Fog->GetFogMaxOpacity();
        FogConstant.FogZ = Fog->GetWorldLocation().Z;
        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferFog, FogConstant);
        Pipeline->SetConstantBuffer(0, false, ConstantBufferFog);

        Pipeline->Draw(3,0);
    }
    Pipeline->SetTexture(0, false, nullptr);
}

void FFogPass::Release()
{
    SafeRelease(VS);
    SafeRelease(PS);
    SafeRelease(ConstantBufferFog);
    SafeRelease(ConstantBufferCameraInverse);
    SafeRelease(ConstantBufferViewportInfo);
	SafeRelease(Sampler);
}
