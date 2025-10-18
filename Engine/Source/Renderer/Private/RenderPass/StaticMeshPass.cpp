#include "pch.h"
#include "Renderer/Public/RenderPass/StaticMeshPass.h"
#include "Scene/Public/Component/StaticMeshComponent.h"
#include "Scene/Public/Component/LightComponentBase.h"
#include "Renderer/Public/Pipeline.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Renderer/Public/LightData.h"
#include "Asset/Public/Texture.h"

FStaticMeshPass::FStaticMeshPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferModel, ID3D11DepthStencilState* InDS)
	: FRenderPass(InPipeline, InConstantBufferModel), DS(InDS)
{
	TArray<D3D11_INPUT_ELEMENT_DESC> TextureLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/TextureVS.hlsl", TextureLayout, &VS, &InputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/TexturePS.hlsl", &PS);

	ConstantBufferMaterial = FRenderResourceFactory::CreateConstantBuffer<FMaterialConstants>();
	ConstantBufferLight = FRenderResourceFactory::CreateConstantBuffer<FLightConstants>();

	// Unified Dynamic Light Buffer (Point, Spot, Rect)
	UnifiedLightCapacity = 128;  // Initial capacity for all dynamic lights
	UnifiedLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FUnifiedDynamicLight>(UnifiedLightCapacity);
	UnifiedLightSRV = FRenderResourceFactory::CreateBufferSRV(UnifiedLightStructuredBuffer, UnifiedLightCapacity);
}

bool FStaticMeshPass::CanRender(const FRenderingContext& Context)
{
	return Context.ShowFlags & EEngineShowFlags::SF_StaticMesh;
}

void FStaticMeshPass::SetRenderTargets(class UDeviceResources* DeviceResources)
{
	ID3D11RenderTargetView* RTVs[] = { DeviceResources->GetDestinationRTV(), DeviceResources->GetNormalBufferRTV() };
	ID3D11DepthStencilView* DSV = DeviceResources->GetDepthBufferDSV();
	Pipeline->SetRenderTargets(2, RTVs, DSV);
}

void FStaticMeshPass::Execute(FRenderingContext& Context)
{
	// Update lights and get count
	uint32 LightCount = UpdateLightsFromContext(Context);

	FRenderState RenderState = UStaticMeshComponent::GetClassDefaultRenderState();
	if (Context.ViewMode == EViewModeIndex::VMI_Wireframe)
	{
		RenderState.CullMode = ECullMode::None; RenderState.FillMode = EFillMode::WireFrame;
	}
	ID3D11RasterizerState* RS = FRenderResourceFactory::GetRasterizerState(RenderState);
	FPipelineInfo PipelineInfo = { InputLayout, VS, RS, DS, PS, nullptr };
	Pipeline->UpdatePipeline(PipelineInfo);

	// [UNIFIED FORWARD RENDERING] Only Ambient light uses ConstantBuffer now
	// All dynamic lights (Directional, Point, Spot) use unified StructuredBuffer
	FLightConstants LightConstants = {};

	// Initialize with default ambient (very dark, almost black)
	LightConstants.GlobalAmbient.Color = FVector(1.0f, 1.0f, 1.0f);
	LightConstants.GlobalAmbient.Intensity = 0.0f;
	LightConstants.UnifiedLightCount = LightCount;

	for (ULightComponentBase* Light : Context.Lights)
	{
		if (Light->GetLightType() == ELightComponentType::LightType_Ambient)
		{
			LightConstants.GlobalAmbient.Color = Light->GetLightColor();
			LightConstants.GlobalAmbient.Intensity = Light->GetIntensity();
			break; // Only one ambient light is supported
		}
	}

	Pipeline->SetConstantBuffer(10, false, ConstantBufferLight);
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferLight, LightConstants);
	Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

	//Pipeline->SetSamplerState(0, false, URenderer::GetInstance().GetDefaultSampler());

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

		if (CurrentMeshAsset != MeshAsset)
		{
			Pipeline->SetVertexBuffer(MeshComp->GetVertexBuffer(), sizeof(FNormalVertex));
			Pipeline->SetIndexBuffer(MeshComp->GetIndexBuffer(), 0);
			CurrentMeshAsset = MeshAsset;
		}

		FModelConstants ModelConstants = {};
		ModelConstants.World = MeshComp->GetWorldTransformMatrix();
		ModelConstants.WorldInverseTranspose = MeshComp->GetWorldTransformMatrixInverse().Transpose();

		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, ModelConstants);
		Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

		if (MeshAsset->MaterialInfo.empty() || MeshComp->GetStaticMesh()->GetNumMaterials() == 0)
		{
			Pipeline->DrawIndexed(MeshAsset->Indices.size(), 0, 0);
			continue;
		}

		if (MeshComp->IsScrollEnabled())
		{
			MeshComp->SetElapsedTime(MeshComp->GetElapsedTime() + UTimeManager::GetInstance().GetDeltaTime());
		}

		for (const FMeshSection& Section : MeshAsset->Sections)
		{
			UMaterial* Material = MeshComp->GetMaterial(Section.MaterialSlot);
			if (CurrentMaterial != Material) {
				FMaterialConstants MaterialConstants = {};
				FVector AmbientColor = Material->GetAmbientColor(); MaterialConstants.Ka = FVector4(AmbientColor.X, AmbientColor.Y, AmbientColor.Z, 1.0f);
				FVector DiffuseColor = Material->GetDiffuseColor(); MaterialConstants.Kd = FVector4(DiffuseColor.X, DiffuseColor.Y, DiffuseColor.Z, 1.0f);
				FVector SpecularColor = Material->GetSpecularColor(); MaterialConstants.Ks = FVector4(SpecularColor.X, SpecularColor.Y, SpecularColor.Z, 1.0f);
				MaterialConstants.Ns = Material->GetSpecularExponent();
				MaterialConstants.Ni = Material->GetRefractionIndex();
				MaterialConstants.D = Material->GetDissolveFactor();
				MaterialConstants.MaterialFlags = 0;
				if (Material->GetDiffuseTexture())  { MaterialConstants.MaterialFlags |= HAS_DIFFUSE_MAP; }
				if (Material->GetAmbientTexture())  { MaterialConstants.MaterialFlags |= HAS_AMBIENT_MAP; }
				if (Material->GetSpecularTexture()) { MaterialConstants.MaterialFlags |= HAS_SPECULAR_MAP; }
				if (Material->GetShininessTexture()) { MaterialConstants.MaterialFlags |= HAS_SHININESS_MAP; }
				if (Material->GetAlphaTexture())    { MaterialConstants.MaterialFlags |= HAS_ALPHA_MAP; }
				if (Material->GetBumpTexture())     { MaterialConstants.MaterialFlags |= HAS_BUMP_MAP; }
				MaterialConstants.Time = MeshComp->GetElapsedTime();

				FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferMaterial, MaterialConstants);
				Pipeline->SetConstantBuffer(2, false, ConstantBufferMaterial);

				if (UTexture* DiffuseTexture = Material->GetDiffuseTexture())
				{
					Pipeline->SetTexture(0, false, DiffuseTexture->GetTextureSRV());
					Pipeline->SetSamplerState(0, false, DiffuseTexture->GetTextureSampler());
				}
				if (UTexture* AmbientTexture = Material->GetAmbientTexture())
				{
					Pipeline->SetTexture(1, false, AmbientTexture->GetTextureSRV());
				}
				if (UTexture* SpecularTexture = Material->GetSpecularTexture())
				{
					Pipeline->SetTexture(2, false, SpecularTexture->GetTextureSRV());
				}
				if (UTexture* NormalTexture = Material->GetShininessTexture())
				{
					Pipeline->SetTexture(3, false, NormalTexture->GetTextureSRV());
				}
				if (UTexture* AlphaTexture = Material->GetAlphaTexture())
				{
					Pipeline->SetTexture(4, false, AlphaTexture->GetTextureSRV());
				}
				if (UTexture* BumpTexture = Material->GetBumpTexture())
				{
					Pipeline->SetTexture(5, false, BumpTexture->GetTextureSRV());
				}

				CurrentMaterial = Material;
			}
			Pipeline->DrawIndexed(Section.IndexCount, Section.StartIndex, 0);
		}
	}
	Pipeline->SetConstantBuffer(2, false, nullptr);
}

uint32 FStaticMeshPass::UpdateLightsFromContext(FRenderingContext& Context)
{
	// Step 1: Collect all dynamic lights into unified buffer
	TArray<FUnifiedDynamicLight> UnifiedLights;

	for (ULightComponentBase* Light : Context.Lights)
	{
		if (!Light || !Light->IsVisible()) continue;

		// Skip Ambient light - it's handled separately via ConstantBuffer
		if (Light->GetLightType() == ELightComponentType::LightType_Ambient)
			continue;

		// Each component provides its own unified light data
		FUnifiedDynamicLight UnifiedLight = Light->GetUnifiedLightData();
		UnifiedLights.push_back(UnifiedLight);
	}

	// Store actual light count before padding
	uint32 ActualLightCount = static_cast<uint32>(UnifiedLights.size());

	// Step 2: Reallocate buffer if capacity exceeded
	// Always maintain minimum capacity of 1 to support empty updates
	if (UnifiedLights.size() > UnifiedLightCapacity)
	{
		UnifiedLightCapacity = static_cast<uint32>(UnifiedLights.size() * 2);
		FRenderResourceFactory::ReallocateStructuredBuffer<FUnifiedDynamicLight>(
			UnifiedLightStructuredBuffer, UnifiedLightSRV, UnifiedLightCapacity);
	}

	// Step 3: Upload to GPU (always update, even if empty, to clear stale data)
	// When empty, upload one dummy light with Intensity=0 to maintain buffer validity
	if (UnifiedLights.empty())
	{
		UnifiedLights.push_back(FUnifiedDynamicLight());  // All fields zero, Intensity=0
	}

	FRenderResourceFactory::UpdateStructuredBufferData(
		UnifiedLightStructuredBuffer, UnifiedLights);

	// Step 4: Bind SRV to Pixel Shader (t6)
	Pipeline->SetTexture(6, false, UnifiedLightSRV);

	// Return actual light count (not including dummy)
	return ActualLightCount;
}


void FStaticMeshPass::Release()
{
	SafeRelease(ConstantBufferMaterial);
	SafeRelease(ConstantBufferLight);
	SafeRelease(UnifiedLightStructuredBuffer);
	SafeRelease(UnifiedLightSRV);
}
