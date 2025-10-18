#include "pch.h"
#include "Renderer/Public/RenderPass/BillboardPass.h"
#include "Asset/Public/Texture.h"
#include "Editor/Public/Camera.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Scene/Public/Component/BillBoardComponent.h"

FBillboardPass::FBillboardPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferModel, ID3D11DepthStencilState* InDS, ID3D11BlendState* InBS)
        : FRenderPass(InPipeline, InConstantBufferModel), DS(InDS), BS(InBS)
{
	TArray<D3D11_INPUT_ELEMENT_DESC> LayoutDesc = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/BillboardShader.hlsl", LayoutDesc, &VS, &InputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/BillboardShader.hlsl", &PS);
}

bool FBillboardPass::CanRender(const FRenderingContext& Context)
{
	return Context.ShowFlags & EEngineShowFlags::SF_Billboard;
}

void FBillboardPass::SetRenderTargets(class UDeviceResources* DeviceResources)
{
	ID3D11RenderTargetView* RTVs[] = { DeviceResources->GetDestinationRTV() };
	ID3D11DepthStencilView* DSV = DeviceResources->GetDepthBufferDSV();
	Pipeline->SetRenderTargets(1, RTVs, DSV);
}

void FBillboardPass::Execute(FRenderingContext& Context)
{
    FRenderState RenderState = UBillBoardComponent::GetClassDefaultRenderState();
    if (Context.ViewMode == EViewModeIndex::VMI_Wireframe)
    {
        RenderState.CullMode = ECullMode::None;
    	RenderState.FillMode = EFillMode::WireFrame;
    }
    FPipelineInfo PipelineInfo = { InputLayout, VS, FRenderResourceFactory::GetRasterizerState(RenderState), DS, PS, BS };
    Pipeline->UpdatePipeline(PipelineInfo);

	//Pipeline->SetTexture(0, false, nullptr);
    for (UBillBoardComponent* BillBoardComp : Context.BillBoards)
    {
        BillBoardComp->FaceCamera(Context.CurrentCamera->GetForward());

        FMatrix WorldMatrix;
        if (BillBoardComp->IsScreenSizeScaled())
        {
            FVector FixedWorldScale = BillBoardComp->GetRelativeScale3D() * BillBoardComp->GetScreenSize();
            FVector BillboardLocation = BillBoardComp->GetWorldLocation();
            FQuaternion BillboardRotation = BillBoardComp->GetWorldRotationAsQuaternion();

            WorldMatrix = FMatrix::GetModelMatrix(BillboardLocation, BillboardRotation, FixedWorldScale);
        }
        else { WorldMatrix = BillBoardComp->GetWorldTransformMatrix(); }

        Pipeline->SetVertexBuffer(BillBoardComp->GetVertexBuffer(), sizeof(FNormalVertex));
        Pipeline->SetIndexBuffer(BillBoardComp->GetIndexBuffer(), 0);

        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, WorldMatrix);
        Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

    	if (Context.ViewMode != EViewModeIndex::VMI_Wireframe)
    	{
    		Pipeline->SetTexture(0, false, BillBoardComp->GetSprite()->GetTextureSRV());
    		Pipeline->SetSamplerState(0, false, BillBoardComp->GetSprite()->GetTextureSampler());
    	}

        Pipeline->DrawIndexed(BillBoardComp->GetNumIndices(), 0, 0);
    }

}

void FBillboardPass::Release()
{
    SafeRelease(VS);
    SafeRelease(PS);
    SafeRelease(InputLayout);
}
