#include "pch.h"
#include "Renderer/Public/RenderPass/StaticMeshUnlitPass.h"
#include "Scene/Public/Component/StaticMeshComponent.h"
#include "Renderer/Public/Pipeline.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Asset/Public/Texture.h"

FStaticMeshUnlitPass::FStaticMeshUnlitPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferModel, ID3D11DepthStencilState* InDS)
	: FRenderPass(InPipeline, InConstantBufferModel), DS(InDS)
{
	TArray<D3D11_INPUT_ELEMENT_DESC> TextureLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Tangent), D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	// Create Unlit shaders
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/TextureUnlitVS.hlsl", TextureLayout, &VSUnlit, &InputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/TextureUnlitPS.hlsl", &PSUnlit);

	// Create material constant buffer (only for Kd, Ka, D, MaterialFlags)
	ConstantBufferMaterial = FRenderResourceFactory::CreateConstantBuffer<FMaterialConstants>();
}

bool FStaticMeshUnlitPass::CanRender(const FRenderingContext& Context)
{
	return (Context.ShowFlags & EEngineShowFlags::SF_StaticMesh) && (Context.ViewMode == EViewModeIndex::VMI_Unlit);
}

void FStaticMeshUnlitPass::SetRenderTargets(class UDeviceResources* DeviceResources)
{
	ID3D11RenderTargetView* RTVs[] = { DeviceResources->GetDestinationRTV(), DeviceResources->GetNormalBufferRTV() };
	ID3D11DepthStencilView* DSV = DeviceResources->GetDepthBufferDSV();
	Pipeline->SetRenderTargets(2, RTVs, DSV);
}

void FStaticMeshUnlitPass::Execute(FRenderingContext& Context)
{
	// No light collection needed for unlit rendering

	// Setup render state
	FRenderState RenderState = UStaticMeshComponent::GetClassDefaultRenderState();
	if (Context.ViewMode == EViewModeIndex::VMI_Wireframe)
	{
		RenderState.CullMode = ECullMode::None;
		RenderState.FillMode = EFillMode::WireFrame;
	}
	ID3D11RasterizerState* RS = FRenderResourceFactory::GetRasterizerState(RenderState);

	// Setup pipeline with unlit shaders
	FPipelineInfo PipelineInfo = { InputLayout, VSUnlit, RS, DS, PSUnlit, nullptr };
	Pipeline->UpdatePipeline(PipelineInfo);

	Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

	// Sort mesh components by mesh asset for batching
	TArray<UStaticMeshComponent*>& MeshComponents = Context.StaticMeshes;
	sort(MeshComponents.begin(), MeshComponents.end(),
		[](UStaticMeshComponent* A, UStaticMeshComponent* B) {
			int32 MeshA = A->GetStaticMesh() ? A->GetStaticMesh()->GetAssetPathFileName().GetComparisonIndex() : 0;
			int32 MeshB = B->GetStaticMesh() ? B->GetStaticMesh()->GetAssetPathFileName().GetComparisonIndex() : 0;
			return MeshA < MeshB;
		});

	FStaticMesh* CurrentMeshAsset = nullptr;
	UMaterial* CurrentMaterial = nullptr;

	for (UStaticMeshComponent* MeshComp : MeshComponents)
	{
		if (!MeshComp->GetStaticMesh()) { continue; }
		FStaticMesh* MeshAsset = MeshComp->GetStaticMesh()->GetStaticMeshAsset();
		if (!MeshAsset) { continue; }

		// Bind vertex and index buffers if mesh changed
		if (CurrentMeshAsset != MeshAsset)
		{
			Pipeline->SetVertexBuffer(MeshComp->GetVertexBuffer(), sizeof(FNormalVertex));
			Pipeline->SetIndexBuffer(MeshComp->GetIndexBuffer(), 0);
			CurrentMeshAsset = MeshAsset;
		}

		// Update model transform
		FModelConstants ModelConstants = {};
		ModelConstants.World = MeshComp->GetWorldTransformMatrix();
		ModelConstants.WorldInverseTranspose = MeshComp->GetWorldTransformMatrixInverse().Transpose();

		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, ModelConstants);
		Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

		// Handle meshes without materials
		if (MeshAsset->MaterialInfo.empty() || MeshComp->GetStaticMesh()->GetNumMaterials() == 0)
		{
			Pipeline->DrawIndexed(MeshAsset->Indices.size(), 0, 0);
			continue;
		}

		// Update elapsed time for scrolling textures
		if (MeshComp->IsScrollEnabled())
		{
			MeshComp->SetElapsedTime(MeshComp->GetElapsedTime() + UTimeManager::GetInstance().GetDeltaTime());
		}

		// Render each section with its material
		for (const FMeshSection& Section : MeshAsset->Sections)
		{
			UMaterial* Material = MeshComp->GetMaterial(Section.MaterialSlot);
			if (CurrentMaterial != Material)
			{
				// Setup material constants (only diffuse and alpha relevant for unlit)
				FMaterialConstants MaterialConstants = {};
				FVector DiffuseColor = Material->GetDiffuseColor();
				MaterialConstants.Kd = FVector4(DiffuseColor.X, DiffuseColor.Y, DiffuseColor.Z, 1.0f);
				MaterialConstants.D = Material->GetDissolveFactor();

				// Material flags - only diffuse and alpha maps
				MaterialConstants.MaterialFlags = 0;
				if (Material->GetDiffuseTexture())  { MaterialConstants.MaterialFlags |= HAS_DIFFUSE_MAP; }
				if (Material->GetAlphaTexture())    { MaterialConstants.MaterialFlags |= HAS_ALPHA_MAP; }
				MaterialConstants.Time = MeshComp->GetElapsedTime();

				FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferMaterial, MaterialConstants);
				Pipeline->SetConstantBuffer(2, false, ConstantBufferMaterial);

				// Bind textures (only diffuse and alpha)
				if (UTexture* DiffuseTexture = Material->GetDiffuseTexture())
				{
					Pipeline->SetSRV(0, false, DiffuseTexture->GetTextureSRV());
					Pipeline->SetSamplerState(0, false, DiffuseTexture->GetTextureSampler());
				}
				if (UTexture* AlphaTexture = Material->GetAlphaTexture())
				{
					Pipeline->SetSRV(4, false, AlphaTexture->GetTextureSRV());
				}

				CurrentMaterial = Material;
			}
			Pipeline->DrawIndexed(Section.IndexCount, Section.StartIndex, 0);
		}
	}
	Pipeline->SetConstantBuffer(2, false, nullptr);
}

void FStaticMeshUnlitPass::Release()
{
	SafeRelease(ConstantBufferMaterial);
	SafeRelease(VSUnlit);
	SafeRelease(PSUnlit);
	SafeRelease(InputLayout);
}
