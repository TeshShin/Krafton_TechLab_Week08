#include "pch.h"
#include "Renderer/Public/RenderPass/ShadowPass.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Renderer/Public/ShadowMapManager.h"
#include "Renderer/Public/RenderPass/DecalPass.h"
#include "Scene/Public/Component/LightComponentBase.h"
#include "Scene/Public/Component/StaticMeshComponent.h"

FShadowPass::FShadowPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS) : FRenderPass(InPipeline), DS(InDS)
{
	TArray<D3D11_INPUT_ELEMENT_DESC> Layout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/Lighting/ShadowMapVS.hlsl", Layout, &VS, &InputLayout);

    CBLightViewProj = FRenderResourceFactory::CreateConstantBuffer<FLightMatrix>();
    // Pixel shader to output VSM moments
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/Lighting/ShadowMapPS.hlsl", &PS);
	FShadowMapManager::GetInstance().Initalize(32, 2048);
}

bool FShadowPass::CanRender(const FRenderingContext& Context)
{
	return Context.ViewMode != EViewModeIndex::VMI_Unlit;
}

void FShadowPass::SetRenderTargets(class UDeviceResources* DeviceResources)
{ 
	// Light 별로 Shadow Map DSV 설정
}

void FShadowPass::Execute(FRenderingContext& Context)
{
	ID3D11DeviceContext* DeviceContext = URenderer::GetInstance().GetDeviceContext();
    FPipelineInfo PipelineInfo = { InputLayout, VS, FRenderResourceFactory::GetRasterizerState({ ECullMode::Back, EFillMode::Solid }),
		DS, PS };
    Pipeline->UpdatePipeline(PipelineInfo);
    Pipeline->SetConstantBuffer(0, EShaderType::EST_Vertex, CBLightViewProj);
    // Bind model matrix cbuffer at b12 (used by ShadowMapVS)
    Pipeline->SetConstantBuffer(12, EShaderType::EST_Vertex, Context.ModelCB);

	FShadowMapManager& ShadowMapManager = FShadowMapManager::GetInstance();
	ShadowMapManager.ClearShadowMaps();

	D3D11_VIEWPORT ShadowViewport = {};
	ShadowViewport.Width = static_cast<float>(ShadowMapManager.GetResolution());
	ShadowViewport.Height = static_cast<float>(ShadowMapManager.GetResolution());
	ShadowViewport.TopLeftX = 0.0f;
	ShadowViewport.TopLeftY = 0.0f;
	ShadowViewport.MinDepth = 0.0f;
	ShadowViewport.MaxDepth = 1.0f;
	DeviceContext->RSSetViewports(1, &ShadowViewport);

	for (ULightComponentBase* Light : Context.Lights)
	{
		Light->SetShadowMapIdx(-1);
		if (Light->DoesCastShadows())
		{
			ShadowMapManager.AllocateShadowMap(Light);

			if (Light->GetShadowMapIdx() == -1) { continue; }

			ID3D11DepthStencilView* ShadowMapDSV = ShadowMapManager.GetDSV(Light->GetShadowMapIdx());
			ID3D11RenderTargetView* MomentsRTV = ShadowMapManager.GetMomentsRTV(Light->GetShadowMapIdx());
			// Clear targets to known state per-light (avoid stale contents)
			const float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			URenderer::GetInstance().GetDeviceContext()->ClearDepthStencilView(ShadowMapDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
			URenderer::GetInstance().GetDeviceContext()->ClearRenderTargetView(MomentsRTV, ClearColor);

			Pipeline->SetRenderTargets(1, &MomentsRTV, ShadowMapDSV);

			FLightMatrix LightConstants = {};
			LightConstants.LightView = Light->GetLightViewMatrix();
			LightConstants.LightProjection = Light->GetLightProjectionMatrix();
			FRenderResourceFactory::UpdateConstantBufferData(CBLightViewProj, LightConstants);

			 
			RenderAllStaticMeshes(Context);

		}
	}

	DeviceContext->RSSetViewports(1, &Context.Viewport);
}

void FShadowPass::Release()
{
    SafeRelease(InputLayout);
    SafeRelease(VS);
    SafeRelease(CBLightViewProj);
    SafeRelease(PS);
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
           // 섀도우 셰이더의 InputLayout이 Position만 받더라도,
           // 실제 버퍼는 FNormalVertex 크기로 생성되었으므로 Stride는 원본과 동일해야 합니다.
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
