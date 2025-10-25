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

	CBLightViewProj = FRenderResourceFactory::CreateConstantBuffer<FMatrix>();
	FShadowMapManager::GetInstance().Initialize(32, 2048, 32, 2048);
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
		DS, nullptr };
	Pipeline->UpdatePipeline(PipelineInfo);
	Pipeline->SetConstantBuffer(0, EShaderType::EST_Vertex, CBLightViewProj);

	FShadowMapManager& ShadowMapManager = FShadowMapManager::GetInstance();
	ShadowMapManager.ClearShadowMaps();

	D3D11_VIEWPORT ShadowViewport = {};
	ShadowViewport.TopLeftX = 0.0f;
	ShadowViewport.TopLeftY = 0.0f;
	ShadowViewport.MinDepth = 0.0f;
	ShadowViewport.MaxDepth = 1.0f;
	TArray<ID3D11DepthStencilView*> ShadowMaps;

	for (ULightComponentBase* Light : Context.Lights)
	{
		Light->SetShadowMapIdx(-1);
		if (Light->DoesCastShadows())
		{
			ShadowMapManager.AllocateShadowMap(Light);
			if (Light->GetShadowMapIdx() == -1) { continue; }

			const uint32 Resolution = ShadowMapManager.GetResolution(Light);
			ShadowViewport.Width = static_cast<float>(Resolution);
			ShadowViewport.Height = static_cast<float>(Resolution);
			DeviceContext->RSSetViewports(1, &ShadowViewport);

			ShadowMapManager.GetDSVs(Light, ShadowMaps);
			const TArray<FMatrix>& VPs = Light->GetLightViewProjectionMatrices();
			for (uint32 Idx = 0; Idx < ShadowMaps.size(); Idx++)
			{
				Pipeline->SetRenderTargets(0, nullptr, ShadowMaps[Idx]);
				FRenderResourceFactory::UpdateConstantBufferData(CBLightViewProj, VPs[Idx]);

				RenderAllStaticMeshes(Context);
			}
		}
	}

	DeviceContext->RSSetViewports(1, &Context.Viewport);
}

void FShadowPass::Release()
{
	SafeRelease(InputLayout);
	SafeRelease(VS);
	SafeRelease(CBLightViewProj);
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
