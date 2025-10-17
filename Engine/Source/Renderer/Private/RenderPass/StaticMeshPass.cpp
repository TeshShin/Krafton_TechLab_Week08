#include "pch.h"
#include "Renderer/Public/RenderPass/StaticMeshPass.h"
#include "Scene/Public/Component/StaticMeshComponent.h"
#include "Scene/Public/Component/PointLightComponent.h"
#include "Scene/Public/Component/SpotLightComponent.h"
#include "Renderer/Public/Pipeline.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Renderer/Public/LightData.h"
#include "Asset/Public/Texture.h"
#include "Scene/Public/Component/LightComponentBase.h"

FStaticMeshPass::FStaticMeshPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera, ID3D11Buffer* InConstantBufferModel,
                                 ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS)
	: FRenderPass(InPipeline, InConstantBufferCamera, InConstantBufferModel), VS(InVS), PS(InPS), InputLayout(InLayout), DS(InDS)
{
	ConstantBufferMaterial = FRenderResourceFactory::CreateConstantBuffer<FMaterialConstants>();
	ConstantBufferLight = FRenderResourceFactory::CreateConstantBuffer<FLightConstants>();

	// Unified Dynamic Light Buffer (Point, Spot, Rect)
	UnifiedLightCapacity = 128;  // Initial capacity for all dynamic lights
	UnifiedLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FUnifiedDynamicLight>(
		UnifiedLightCapacity);
	UnifiedLightSRV = FRenderResourceFactory::CreateBufferSRV(
		UnifiedLightStructuredBuffer, UnifiedLightCapacity);
}

void FStaticMeshPass::Execute(FRenderingContext& Context)
{
	UpdateLightsFromContext(Context);

	FRenderState RenderState = UStaticMeshComponent::GetClassDefaultRenderState();
	if (Context.ViewMode == EViewModeIndex::VMI_Wireframe)
	{
		RenderState.CullMode = ECullMode::None; RenderState.FillMode = EFillMode::WireFrame;
	}
	ID3D11RasterizerState* RS = FRenderResourceFactory::GetRasterizerState(RenderState);
	FPipelineInfo PipelineInfo = { InputLayout, VS, RS, DS, PS, nullptr };
	Pipeline->UpdatePipeline(PipelineInfo);

	FLightConstants LightConstants;
	int32 DirectionalLightCount = 0;

	for (ULightComponentBase* Light : Context.Lights)
	{
		switch (Light->GetLightType())
		{
		case ELightComponentType::LightType_Ambient:
			{
				LightConstants.GlobalAmbient.Color = Light->GetLightColor();
				LightConstants.GlobalAmbient.Intensity = Light->GetIntensity();
				break;
			}
		case ELightComponentType::LightType_Directional:
			{
				if (DirectionalLightCount >= MAX_DIRECTIONAL_LIGHTS) { continue; }
				LightConstants.DirectionalLights[DirectionalLightCount].Color = Light->GetLightColor();
				LightConstants.DirectionalLights[DirectionalLightCount].Intensity = Light->GetIntensity();
				LightConstants.DirectionalLights[DirectionalLightCount].Direction = Light->GetWorldForwardVector();
				DirectionalLightCount++;
				break;
			}
		default: ;
		}
	}
	LightConstants.NumDirectionalLights = DirectionalLightCount;
	Pipeline->SetConstantBuffer(10, false, ConstantBufferLight);
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferLight, LightConstants);
	// Set a default sampler to slot 0 to ensure one is always bound
	Pipeline->SetSamplerState(0, false, URenderer::GetInstance().GetDefaultSampler());

	Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);
	Pipeline->SetConstantBuffer(1, true, ConstantBufferCamera);
	Pipeline->SetConstantBuffer(1, false, ConstantBufferCamera);

	if (!(Context.ShowFlags & EEngineShowFlags::SF_StaticMesh)) { return; }
	TArray<UStaticMeshComponent*>& MeshComponents = Context.StaticMeshes;
	sort(MeshComponents.begin(), MeshComponents.end(),
		[](UStaticMeshComponent* A, UStaticMeshComponent* B) {
			int32 MeshA = A->GetStaticMesh() ? A->GetStaticMesh()->GetAssetPathFileName().GetComparisonIndex() : 0;
			int32 MeshB = B->GetStaticMesh() ? B->GetStaticMesh()->GetAssetPathFileName().GetComparisonIndex() : 0;
			return MeshA < MeshB;
		});

	FStaticMesh* CurrentMeshAsset = nullptr;
	UMaterial* CurrentMaterial = nullptr;

	// --- RTVs Setup ---

	/**
	 * @todo Find a better way to reduce depdency upon Renderer class.
	 * @note How about introducing methods like BeginPass(), EndPass() to set up and release pass specific state?
	 */
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
	ID3D11RenderTargetView* RTVs[2] = { RTV, DeviceResources->GetNormalRenderTargetView() };
	ID3D11DepthStencilView* DSV = DeviceResources->GetDepthStencilView();
	Pipeline->SetRenderTargets(2, RTVs, DSV);

	// --- RTVs Setup End ---

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


	// --- RTVs Reset ---

	/**
	 * @todo Find a better way to reduce depdency upon Renderer class.
	 * @note How about introducing methods like BeginPass(), EndPass() to set up and release pass specific state?
	 */
	Pipeline->SetRenderTargets(1, RTVs, DSV);

	// --- RTVs Reset End ---
}

void FStaticMeshPass::UpdateLightsFromContext(FRenderingContext& Context)
{
	// Step 1: Collect all dynamic lights into unified buffer
	TArray<FUnifiedDynamicLight> UnifiedLights;

	for (ULightComponentBase* Light : Context.Lights)
	{
		if (!Light || !Light->IsVisible()) continue;

		FUnifiedDynamicLight UnifiedLight = {};

		switch (Light->GetLightType())
		{
		case ELightComponentType::LightType_Point:
		{
			UPointLightComponent* PointLight = Cast<UPointLightComponent>(Light);
			if (!PointLight) continue;

			UnifiedLight.Position = PointLight->GetWorldLocation();
			UnifiedLight.Intensity = PointLight->GetIntensity();
			UnifiedLight.Color = PointLight->GetLightColor();
			UnifiedLight.SourceRadius = max(PointLight->GetSourceRadius(), 1000.0f);  // Temp: ensure minimum radius
			UnifiedLight.FalloffExponent = PointLight->GetLightFalloffExtent();
			UnifiedLight.LightType = static_cast<uint32>(EDynamicLightType::Point);

			UnifiedLights.push_back(UnifiedLight);
			break;
		}
		//case ELightComponentType::LightType_Spot:
		//{
		//	USpotLightComponent* SpotLight = Cast<USpotLightComponent>(Light);
		//	if (!SpotLight) continue;

		//	UnifiedLight.Position = SpotLight->GetWorldLocation();
		//	UnifiedLight.Intensity = SpotLight->GetIntensity();
		//	UnifiedLight.Color = SpotLight->GetLightColor();
		//	UnifiedLight.SourceRadius = 1000.0f;
		//	UnifiedLight.Direction = SpotLight->GetWorldForwardVector();
		//	UnifiedLight.FalloffExponent = SpotLight->GetLight
		//	//UnifiedLight.Param0 = SpotLight->GetInnerConeAngle();
		//	//UnifiedLight.Param1 = SpotLight->GetOuterConeAngle();
		//	UnifiedLight.LightType = static_cast<uint32>(EDynamicLightType::Spot);

		//	UnifiedLights.push_back(UnifiedLight);
		//	break;
		//}
		case ELightComponentType::LightType_Ambient:
		case ELightComponentType::LightType_Directional:
			// These are handled via ConstantBuffer, skip here
			break;
		default:
			break;
		}
	}

	// Step 2: Reallocate buffer if capacity exceeded
	if (UnifiedLights.size() > UnifiedLightCapacity)
	{
		UnifiedLightCapacity = static_cast<uint32>(UnifiedLights.size() * 2);
		FRenderResourceFactory::ReallocateStructuredBuffer<FUnifiedDynamicLight>(
			UnifiedLightStructuredBuffer, UnifiedLightSRV, UnifiedLightCapacity);
	}

	// Step 3: Upload to GPU
	if (!UnifiedLights.empty())
	{
		FRenderResourceFactory::UpdateStructuredBufferData(
			UnifiedLightStructuredBuffer, UnifiedLights);
	}

	// Step 4: Bind SRV to Pixel Shader (t6)
	Pipeline->SetTexture(6, false, UnifiedLightSRV);
}


void FStaticMeshPass::Release()
{
	SafeRelease(ConstantBufferMaterial);
	SafeRelease(ConstantBufferLight);
	SafeRelease(UnifiedLightStructuredBuffer);
	SafeRelease(UnifiedLightSRV);
}
