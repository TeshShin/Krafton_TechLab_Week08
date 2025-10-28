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
	FShadowMapManager::GetInstance().Initialize(16, 2048, 16, 2048, 4096);
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
	FPipelineInfo PipelineInfo = { InputLayout, VS,
		FRenderResourceFactory::GetRasterizerState({ ECullMode::Back, EFillMode::Solid }), DS, PS };
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
	FMatrix CameraVPInv = CamInv.Projection * CamInv.View;
    TArray<ID3D11RenderTargetView*> PointRTVs;

    for (ULightComponentBase* Light : Context.Lights)
    {
        Light->SetShadowMapIdx(-1);
        if (Light->IsVisible() && Light->DoesCastShadows())
        {
            ShadowMapManager.AllocateShadowMap(Light);
            if (Light->GetShadowMapIdx() == -1) { continue; }

        	const TArray<FMatrix>& ViewMatrices = Light->GetLightViewMatrices(CameraVPInv);
        	FShadowLightInfo LightInfo;
        	LightInfo.LightPosition = Light->GetWorldLocation();
        	LightInfo.LightType = static_cast<uint32>(Light->GetLightType());
        	LightInfo.LightView = ViewMatrices[0];
        	LightInfo.LightProjection = Light->GetLightProjectionMatrix(CameraVPInv);
        	FRenderResourceFactory::UpdateConstantBufferData(CBLightInfo, LightInfo);

            const uint32 Resolution = ShadowMapManager.GetResolution(Light);
            ShadowViewport.Width = static_cast<float>(Resolution);
            ShadowViewport.Height = static_cast<float>(Resolution);
            DeviceContext->RSSetViewports(1, &ShadowViewport);

            if (Light->GetLightType() == ELightComponentType::LightType_Point)
            {
            	UPointLightComponent* PointLight = Cast<UPointLightComponent>(Light);
                ShadowMapManager.GetPointShadowRTVs(Light, PointRTVs);
                ID3D11DepthStencilView* SharedDSV = ShadowMapManager.GetPointShadowDepthDSV();

				LightInfo.LightRadius = PointLight->GetAttenuationRadius();
                for (uint32 Idx = 0; Idx < PointRTVs.size(); Idx++)
                {
                	DeviceContext->ClearDepthStencilView(SharedDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

                	LightInfo.LightView = ViewMatrices[Idx];
                	FRenderResourceFactory::UpdateConstantBufferData(CBLightInfo, LightInfo);

                    Pipeline->SetRenderTargets(1, &PointRTVs[Idx], SharedDSV);
                	RenderAllStaticMeshes(Context);
                }
            }
            else
            {
            	ID3D11DepthStencilView* DSV = nullptr;
            	ID3D11RenderTargetView* RTV[] = { nullptr };
            	if (Light->GetLightType() == ELightComponentType::LightType_Spot)
            	{
            		DSV = ShadowMapManager.GetSpotLightDSV(Light->GetShadowMapIdx());
            		RTV[0] = ShadowMapManager.GetSpotMomentsRTV(Light->GetShadowMapIdx());
            	}
            	else if (Light->GetLightType() == ELightComponentType::LightType_Directional)
            	{
            		DSV = ShadowMapManager.GetDirectionalLightDSV();
            		RTV[0] = ShadowMapManager.GetDirectionalMomentRTV();
            	}

            	Pipeline->SetRenderTargets(1, RTV, DSV);
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
