#include "pch.h"
#include "Renderer/Public/RenderPass/ShadowPass.h"

#include "Editor/Public/Camera.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Renderer/Public/ShadowMapManager.h"
#include "Renderer/Public/RenderPass/DecalPass.h"
#include "Scene/Public/Component/LightComponentBase.h"
#include "Scene/Public/Component/PointLightComponent.h"
#include "Scene/Public/Component/StaticMeshComponent.h"

FShadowPass::FShadowPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS) : FRenderPass(InPipeline), DS(InDS)
{
	TArray<D3D11_INPUT_ELEMENT_DESC> Layout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/Lighting/ShadowMapShader.hlsl", Layout, &VS, &InputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/Lighting/ShadowMapShader.hlsl", &PS);

	CBLightInfo = FRenderResourceFactory::CreateConstantBuffer<FShadowLightInfo>();
	FShadowMapManager::GetInstance().Initialize(EShadowFilterType::SFT_None, 16, 1024, 16, 1024, 2048);
}

bool FShadowPass::CanRender(const FRenderingContext& Context)
{
	return (Context.ViewMode != EViewModeIndex::VMI_Unlit) && (Context.ShowFlags & EEngineShowFlags::SF_Shadow);
}

void FShadowPass::SetRenderTargets(class UDeviceResources* DeviceResources)
{
	// Light 별로 Shadow Map DSV 설정
}

void FShadowPass::Execute(FRenderingContext& Context)
{
	ID3D11DeviceContext* DeviceContext = URenderer::GetInstance().GetDeviceContext();
	// ECullMode::Back -> Front 로 교체, Bias 채워진 RenderState 사용
	FRenderState ShadowRenderState;
	ShadowRenderState.CullMode = ECullMode::Front;           // Front-face culling으로 셀프 셰도우 감소
	ShadowRenderState.FillMode = EFillMode::Solid;

	// 해상도 기반 Bias 추천값 (스팟 섀도우 기준)
	const uint32 SpotRes = FShadowMapManager::GetInstance().GetSpotResolution();
	ShadowRenderState.DepthBias = 0;                         // D32_FLOAT에선 보통 0
	// TODO - 추후에 사용하게 된다면 수치 조정할 것
	ShadowRenderState.SlopeScaledDepthBias = 1.5f;           // 1.0~2.5 범위에서 조정
	ShadowRenderState.DepthBiasClamp = 2.0f / SpotRes;       // 텍셀 단위 클램프 (~2 texels)

	FPipelineInfo PipelineInfo =
	{
		InputLayout, VS,
		FRenderResourceFactory::GetRasterizerState(ShadowRenderState),
		DS, PS
	};
	Pipeline->UpdatePipeline(PipelineInfo);
	Pipeline->SetConstantBuffer(0, EShaderType::EST_Vertex, CBLightInfo);
	Pipeline->SetConstantBuffer(0, EShaderType::EST_Pixel, CBLightInfo);
    Pipeline->SetConstantBuffer(12, EShaderType::EST_Vertex, Context.ModelCB);

	FShadowMapManager& ShadowMapManager = FShadowMapManager::GetInstance();
	ShadowMapManager.ClearShadowMaps();

	D3D11_VIEWPORT ShadowViewport = {};
	ShadowViewport.TopLeftX = 0.0f;
	ShadowViewport.TopLeftY = 0.0f;
	ShadowViewport.MinDepth = 0.0f;
	ShadowViewport.MaxDepth = 1.0f;

	FCameraConstants CamInv = Context.CurrentCamera->GetCameraConstantsInverse();

	for (ULightComponentBase* Light : Context.Lights)
	{
	    Light->SetShadowMapIdx(-1);
	    if (!Light->IsVisibleInHierarchy() || !Light->DoesCastShadows()) { continue; }

	    ShadowMapManager.AllocateShadowMap(Light);
	    if (Light->GetShadowMapIdx() == -1) { continue; }

	    const TArray<FMatrix>& ViewMatrices = Light->GetLightViewMatrices(CamInv);
		const TArray<FMatrix>& DsvProjMats = Light->GetCSMDsvProjections(CamInv);

	    const uint32 Resolution = ShadowMapManager.GetResolution(Light);
	    ShadowViewport.Width = static_cast<float>(Resolution);
	    ShadowViewport.Height = static_cast<float>(Resolution);
	    DeviceContext->RSSetViewports(1, &ShadowViewport);

	    FShadowLightInfo LightInfo;
	    LightInfo.LightPosition = Light->GetWorldLocation();
	    LightInfo.LightType = static_cast<uint32>(Light->GetLightType());
	    LightInfo.LightProjection = Light->GetLightProjectionMatrix(CamInv);

	    if (Light->GetLightType() == ELightComponentType::LightType_Point)
	    {
	        LightInfo.LightRadius = Cast<UPointLightComponent>(Light)->GetAttenuationRadius();
	    }

	    const uint32 NumPasses = ShadowMapManager.GetShadowPassCount(Light);

	    for (uint32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
	    {
	        ID3D11DepthStencilView* DSV = nullptr;
	        ID3D11RenderTargetView* RTV[] = { nullptr };
	        ShadowMapManager.GetShadowPassViews(Light, PassIndex, &RTV[0], &DSV);

	        if (PassIndex >= 1)
	        {
	            DeviceContext->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
	        }

	    	if (Light->GetShadowProjectionMode() == EShadowProjectionMode::CSM)
	    	{
	    		LightInfo.LightProjection = DsvProjMats[PassIndex];
	    	}

	        LightInfo.LightView = ViewMatrices[PassIndex];
	        FRenderResourceFactory::UpdateConstantBufferData(CBLightInfo, LightInfo);

	    	PipelineInfo.PixelShader = RTV[0] == nullptr ? nullptr : PS;
	    	Pipeline->UpdatePipeline(PipelineInfo);

	        Pipeline->SetRenderTargets(1, RTV, DSV);
	        RenderAllStaticMeshes(Context);
	    }
	}

    DeviceContext->RSSetViewports(1, &Context.Viewport);
}

void FShadowPass::Release()
{
	SafeRelease(InputLayout);
	SafeRelease(VS);
	SafeRelease(PS);
	SafeRelease(CBLightInfo);
}

void FShadowPass::RenderAllStaticMeshes(const FRenderingContext& Context) const
{
    FStaticMesh* CurrentMeshAsset = nullptr;

    for (UStaticMeshComponent* MeshComp : Context.StaticMeshes)
    {
        if (!MeshComp->GetStaticMesh()) { continue; }
        FStaticMesh* MeshAsset = MeshComp->GetStaticMesh()->GetStaticMeshAsset();
        if (!MeshAsset) { continue; }

        if (CurrentMeshAsset != MeshAsset)
        {
           Pipeline->SetVertexBuffer(MeshComp->GetVertexBuffer(), sizeof(FNormalVertex));
           Pipeline->SetIndexBuffer(MeshComp->GetIndexBuffer(), 0);
           CurrentMeshAsset = MeshAsset;
        }

        FModelConstants ModelConstants = {};
        ModelConstants.World = MeshComp->GetWorldTransformMatrix();
    	ModelConstants.WorldInverseTranspose = MeshComp->GetWorldTransformMatrixInverse().Transpose();
        FRenderResourceFactory::UpdateConstantBufferData(Context.ModelCB, ModelConstants);

        // 재질 정보가 없는 단순 메시 그리기
        if (MeshAsset->MaterialInfo.empty() || MeshComp->GetStaticMesh()->GetNumMaterials() == 0)
        {
           Pipeline->DrawIndexed(MeshAsset->Indices.size(), 0, 0);
           continue;
        }

        for (const FMeshSection& Section : MeshAsset->Sections)
        {
           // DrawIndexed만 호출
           Pipeline->DrawIndexed(Section.IndexCount, Section.StartIndex, 0);
        }
    }
}
