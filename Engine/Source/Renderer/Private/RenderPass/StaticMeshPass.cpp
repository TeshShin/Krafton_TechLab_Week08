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
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Tangent), D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	D3D_SHADER_MACRO TexturePhongDefines[] =
	{
		"LIGHTING_MODEL_PHONG", "1",
		nullptr, nullptr
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/TextureVS.hlsl", TextureLayout, &VSPhong, &InputLayout, TexturePhongDefines);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/TexturePS.hlsl", &PSPhong, TexturePhongDefines);

	D3D_SHADER_MACRO TextureGouraudDefines[] =
	{
		"LIGHTING_MODEL_GOURAUD", "1",
		nullptr, nullptr
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/TextureVS.hlsl", TextureLayout, &VSGouraud, &InputLayout, TextureGouraudDefines);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/TexturePS.hlsl", &PSGouraud, TextureGouraudDefines);

	D3D_SHADER_MACRO TextureLambertDefines[] =
	{
		"LIGHTING_MODEL_LAMBERT", "1",
		nullptr, nullptr
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/TextureVS.hlsl", TextureLayout, &VSLambert, &InputLayout, TextureLambertDefines);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/TexturePS.hlsl", &PSLambert, TextureLambertDefines);

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
	// Collect lights from context
	TArray<FUnifiedDynamicLight> UnifiedLights = CollectLightsFromContext(Context);

	// Ensure buffer capacity is sufficient
	if (UnifiedLights.size() > UnifiedLightCapacity)
	{
		UnifiedLightCapacity = static_cast<uint32>(UnifiedLights.size() * 2);
		FRenderResourceFactory::ReallocateStructuredBuffer<FUnifiedDynamicLight>(
			UnifiedLightStructuredBuffer, UnifiedLightSRV, UnifiedLightCapacity);
	}

	// Upload Unified Lights to StructuredBuffer
	FRenderResourceFactory::UpdateStructuredBufferData(
		UnifiedLightStructuredBuffer, UnifiedLights);

	// Bind Unified Light SRV to the pipeline
	if(Context.ViewMode == EViewModeIndex::VMI_Lit_Gouraud)
	{
		Pipeline->SetSRV(6, true, UnifiedLightSRV);
	}
	else
	{
		Pipeline->SetSRV(6, false, UnifiedLightSRV);
	}

	FRenderState RenderState = UStaticMeshComponent::GetClassDefaultRenderState();
	if (Context.ViewMode == EViewModeIndex::VMI_Wireframe)
	{
		RenderState.CullMode = ECullMode::None;
		RenderState.FillMode = EFillMode::WireFrame;
	}
	ID3D11RasterizerState* RS = FRenderResourceFactory::GetRasterizerState(RenderState);

	ID3D11VertexShader* VSToUse = nullptr;
	ID3D11PixelShader* PSToUse = nullptr;
	switch (Context.ViewMode)
	{
	case EViewModeIndex::VMI_Lit_Lambert:
		VSToUse = VSLambert;
		PSToUse = PSLambert;
		break;
	case EViewModeIndex::VMI_Lit_Gouraud:
		VSToUse = VSGouraud;
		PSToUse = PSGouraud;
		break;
	case EViewModeIndex::VMI_Lit_Phong:
		VSToUse = VSPhong;
		PSToUse = PSPhong;
		break;
	default: // Normal View 모드에서는 Phong shading에서 normal mapping한 결과가 필요
		VSToUse = VSPhong;
		PSToUse = PSPhong;
		break;
	}

	FPipelineInfo PipelineInfo = { InputLayout, VSToUse, RS, DS, PSToUse, nullptr };
	Pipeline->UpdatePipeline(PipelineInfo);

	// [UNIFIED FORWARD RENDERING] All lights (Directional, Point, Spot, Ambient) use StructuredBuffer
	FLightConstants LightConstants = {};

	// GlobalAmbient is deprecated - all lights now go through unified StructuredBuffer
	LightConstants.UnifiedLightCount = UnifiedLights.size();

	//for (ULightComponentBase* Light : Context.Lights)
	//{
	//	if (Light->GetLightType() == ELightComponentType::LightType_Ambient)
	//	{
	//		LightConstants.GlobalAmbient.Color = Light->GetLightColor();
	//		LightConstants.GlobalAmbient.Intensity = Light->GetIntensity();
	//		break; // Only one ambient light is supported
	//	}
	//}

	if (Context.ViewMode == EViewModeIndex::VMI_Lit_Gouraud)
	{
		Pipeline->SetConstantBuffer(10, true, ConstantBufferLight);
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferLight, LightConstants);
	}
	else
	{
		Pipeline->SetConstantBuffer(10, false, ConstantBufferLight);
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferLight, LightConstants);
	}

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

				// Gouraud 모드면, Vertex Shader에도 Material 바인딩
				Pipeline->SetConstantBuffer(2, false, ConstantBufferMaterial);
				if(Context.ViewMode == EViewModeIndex::VMI_Lit_Gouraud)
				{
					Pipeline->SetConstantBuffer(2, true, ConstantBufferMaterial);
				}

				if (UTexture* DiffuseTexture = Material->GetDiffuseTexture())
				{
					Pipeline->SetSRV(0, false, DiffuseTexture->GetTextureSRV());
					Pipeline->SetSamplerState(0, false, DiffuseTexture->GetTextureSampler());
				}
				if (UTexture* AmbientTexture = Material->GetAmbientTexture())
				{
					Pipeline->SetSRV(1, false, AmbientTexture->GetTextureSRV());
				}
				if (UTexture* SpecularTexture = Material->GetSpecularTexture())
				{
					Pipeline->SetSRV(2, false, SpecularTexture->GetTextureSRV());
				}
				if (UTexture* NormalTexture = Material->GetShininessTexture())
				{
					Pipeline->SetSRV(3, false, NormalTexture->GetTextureSRV());
				}
				if (UTexture* AlphaTexture = Material->GetAlphaTexture())
				{
					Pipeline->SetSRV(4, false, AlphaTexture->GetTextureSRV());
				}
				if (UTexture* BumpTexture = Material->GetBumpTexture())
				{
					Pipeline->SetSRV(5, false, BumpTexture->GetTextureSRV());
				}

				CurrentMaterial = Material;
			}
			Pipeline->DrawIndexed(Section.IndexCount, Section.StartIndex, 0);
		}
	}
	Pipeline->SetConstantBuffer(2, false, nullptr);
}

TArray<FUnifiedDynamicLight> FStaticMeshPass::CollectLightsFromContext(FRenderingContext& Context)
{
	// Collect all dynamic lights into unified buffer
	TArray<FUnifiedDynamicLight> UnifiedLights;

	for (ULightComponentBase* Light : Context.Lights)
	{
		if (!Light || !Light->IsVisible()) continue;

		// [UNIFIED FORWARD RENDERING] All light types (including Ambient) go through StructuredBuffer
		FUnifiedDynamicLight UnifiedLight = Light->GetUnifiedLightData();
		UnifiedLights.push_back(UnifiedLight);
	}

	// When empty, upload one dummy light with Intensity=0 to maintain buffer validity
	if (UnifiedLights.empty())
	{
		UnifiedLights.push_back(FUnifiedDynamicLight());  // All fields zero, Intensity=0
	}

	return UnifiedLights;
}


void FStaticMeshPass::Release()
{
	SafeRelease(ConstantBufferMaterial);
	SafeRelease(ConstantBufferLight);
	SafeRelease(UnifiedLightStructuredBuffer);
	SafeRelease(UnifiedLightSRV);
	SafeRelease(VSPhong);
	SafeRelease(PSPhong);
	SafeRelease(VSLambert);
	SafeRelease(PSLambert);
	SafeRelease(VSGouraud);
	SafeRelease(PSGouraud);
	SafeRelease(InputLayout);
}
