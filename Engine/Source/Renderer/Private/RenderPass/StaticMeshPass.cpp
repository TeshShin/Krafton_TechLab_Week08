#include "pch.h"
#include "Renderer/Public/RenderPass/StaticMeshPass.h"
#include "Scene/Public/Component/StaticMeshComponent.h"
#include "Scene/Public/Component/LightComponentBase.h"
#include "Renderer/Public/Pipeline.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Renderer/Public/LightData.h"
#include "Asset/Public/Texture.h"
#include "Editor/Public/Camera.h"

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

	LightTilesCS = FRenderResourceFactory::CreateComputeShader(L"Asset/Shader/LightTilesComputeShader.hlsl");

    FP_CameraCB = FRenderResourceFactory::CreateConstantBuffer<FForwardPlusCameraConstants>();
    FP_ParamsCB = FRenderResourceFactory::CreateConstantBuffer<FForwardPlusConstants>();

    // Debug heat overlay shaders (fullscreen VS + heat PS)
    TArray<D3D11_INPUT_ELEMENT_DESC> FullscreenLayout = {};
    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/ClusterHeatShader.hlsl", FullscreenLayout, &HeatVS, nullptr);
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/ClusterHeatShader.hlsl", &HeatPS);

    // Create disabled depth-stencil state so overlay always draws on top
    D3D11_DEPTH_STENCIL_DESC DisabledDesc = {};
    DisabledDesc.DepthEnable = FALSE;
    DisabledDesc.StencilEnable = FALSE;
    URenderer::GetInstance().GetDevice()->CreateDepthStencilState(&DisabledDesc, &DS_Disabled);
}

bool FStaticMeshPass::CanRender(const FRenderingContext& Context)
{
	return (Context.ShowFlags & EEngineShowFlags::SF_StaticMesh) && (Context.ViewMode != EViewModeIndex::VMI_Unlit);
}

void FStaticMeshPass::SetRenderTargets(class UDeviceResources* DeviceResources)
{
	ID3D11RenderTargetView* RTVs[] = { DeviceResources->GetDestinationRTV(), DeviceResources->GetNormalBufferRTV() };
	ID3D11DepthStencilView* DSV = DeviceResources->GetDepthBufferDSV();
	Pipeline->SetRenderTargets(2, RTVs, DSV);
}

void FStaticMeshPass::CreateClusterBuffers(FRenderingContext& Context, uint32 NumLights)
{
    // Use per-viewport size (multi-viewport aware)
    uint32 Width = static_cast<uint32>(Context.Viewport.Width);
    uint32 Height = static_cast<uint32>(Context.Viewport.Height);

	uint32 TileSize = 32;

	// Ceil-div to cover the whole screen with tiles
	uint32 NumTilesX = (Width  + TileSize - 1) / TileSize;
	uint32 NumTilesY = (Height + TileSize - 1) / TileSize;
	uint32 NumZSlices = 24;
	uint32 TotalClusters = NumTilesX * NumTilesY * NumZSlices;
	uint32 MaxLightsPerCluster = 64;

	// (Re)create buffers only when dimensions or capacities change
	bool bDimsChanged = (CachedNumTilesX != NumTilesX) || (CachedNumTilesY != NumTilesY) || (CachedNumZSlices != NumZSlices);
	bool bMaxChanged  = (CachedMaxLightsPerCluster != MaxLightsPerCluster);
	if (bDimsChanged || bMaxChanged || !ClusterCountBuffer || !ClusterIndexBuffer)
	{
		SafeRelease(ClusterCountSRV);
		SafeRelease(ClusterCountUAV);
		SafeRelease(ClusterCountBuffer);
		SafeRelease(ClusterIndexSRV);
		SafeRelease(ClusterIndexUAV);
		SafeRelease(ClusterIndexBuffer);

		// Create Count buffer (TotalClusters uints)
		const uint32 CountElements = TotalClusters;
		ClusterCountBuffer = FRenderResourceFactory::CreateStructuredBufferWithUAV(
			sizeof(uint32),
			CountElements,
			&ClusterCountSRV,
			&ClusterCountUAV);

		// Create Index buffer (TotalClusters * MaxLightsPerCluster uints)
		const uint32 IndexElements = TotalClusters * MaxLightsPerCluster;
		ClusterIndexBuffer = FRenderResourceFactory::CreateStructuredBufferWithUAV(
			sizeof(uint32),
			IndexElements,
			&ClusterIndexSRV,
			&ClusterIndexUAV);

	        // Cache
	        CachedNumTilesX = NumTilesX;
	        CachedNumTilesY = NumTilesY;
	        CachedNumZSlices = NumZSlices;
	        CachedMaxLightsPerCluster = MaxLightsPerCluster;
    }

	auto* CurrentCamera = Context.CurrentCamera;

	FForwardPlusCameraConstants FPcam = {};
	FPcam.View        = CurrentCamera->GetFViewProjConstants().View;                // row_major
	FPcam.Proj        = CurrentCamera->GetFViewProjConstants().Projection;          // row_major
	FPcam.InvProj     = CurrentCamera->GetFViewProjConstantsInverse().Projection;
    FPcam.ScreenSize  = {Width, Height};
    FPcam.ViewportOrigin = { static_cast<uint32>(Context.Viewport.TopLeftX), static_cast<uint32>(Context.Viewport.TopLeftY) };
	FPcam.NumTilesX   = NumTilesX;
	FPcam.NumTilesY   = NumTilesY;
	FPcam.NumZSlices  = NumZSlices;
	FPcam.NearZ       = CurrentCamera->GetNearZ();
	FPcam.FarZ        = CurrentCamera->GetFarZ();

	FForwardPlusConstants FPparams = {};
	FPparams.NumLights            = static_cast<uint32>(NumLights);
	FPparams.MaxLightsPerCluster  = MaxLightsPerCluster;
	FPparams.TotalClusters        = TotalClusters;

	FRenderResourceFactory::UpdateConstantBufferData(FP_CameraCB, FPcam);
	FRenderResourceFactory::UpdateConstantBufferData(FP_ParamsCB, FPparams);

	ID3D11DeviceContext* ctx = URenderer::GetInstance().GetDeviceContext();

	// CS constant buffers (slots b0,b1 to match LightTilesComputeShader.hlsl)
	ID3D11Buffer* csCBs[2] = { FP_CameraCB, FP_ParamsCB };
	ctx->CSSetConstantBuffers(0, 2, csCBs);

	// CS SRV (t0 = unified dynamic lights; you already filled/grew this)
	ID3D11ShaderResourceView* csSRVs[1] = { UnifiedLightSRV };
	ctx->CSSetShaderResources(0, 1, csSRVs);

	// CS UAVs (u0 = counts, u1 = indices)
	ID3D11UnorderedAccessView* csUAVs[2] = { ClusterCountUAV, ClusterIndexUAV };
	UINT initialCounts[2] = { 0, 0 }; // ignored for structured UAVs
	ctx->CSSetUnorderedAccessViews(0, 2, csUAVs, initialCounts);

	// Set compute shader
	ctx->CSSetShader(LightTilesCS, nullptr, 0);

	// IMPORTANT: clear counts either here or inside CS.
	// Since your count buffer is a *structured* UAV, prefer zeroing at the top of the CS kernel:
	//   ClusterCount[cid] = 0;
	// If you already did that, no ClearUAVUint call is needed here.

	// Dispatch one thread per cluster (per your CS)
	ctx->Dispatch(NumTilesX, NumTilesY, NumZSlices);

	// Unbind CS UAVs/SRVs to avoid hazards when we rebinding as PS SRVs later
	ID3D11UnorderedAccessView* nullUAVs[2] = { nullptr, nullptr };
	ctx->CSSetUnorderedAccessViews(0, 2, nullUAVs, initialCounts);

	ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
	ctx->CSSetShaderResources(0, 1, nullSRVs);

	// (Optional) leave CS set or clear it:
	ctx->CSSetShader(nullptr, nullptr, 0);
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

    // After uploading current lights, build clusters using the compute shader
    CreateClusterBuffers(Context, UnifiedLights.size());

	// Bind Unified Light SRV to the pipeline
	if(Context.ViewMode == EViewModeIndex::VMI_Lit_Gouraud)
	{
		Pipeline->SetSRV(6, true, UnifiedLightSRV);
	}
	else
	{
		// PS also needs the unified light buffer at t6 for clustering
		Pipeline->SetSRV(6, false /*PS*/, UnifiedLightSRV);
		// Bind Forward+ SRVs for PS
		Pipeline->SetSRV(7, false /*PS*/, ClusterCountSRV);
		Pipeline->SetSRV(8, false /*PS*/, ClusterIndexSRV);

		// Bind Forward+ CBs for PS (slots b11, b12)
		Pipeline->SetConstantBuffer(11, false /*PS*/, FP_CameraCB);
		Pipeline->SetConstantBuffer(12, false /*PS*/, FP_ParamsCB);
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

	// --- Debug: draw cluster heat overlay ---
	bool bOverlayEnabled = (Context.ShowFlags & EEngineShowFlags::SF_ClusterHeat) != 0;
	if (bOverlayEnabled && HeatVS && HeatPS && ClusterCountSRV)
	{
		// Reuse already-bound FP cbuffers (b11,b12) and FP_ClusterCount (t7)
		// Setup fullscreen pipeline with disabled depth
		auto RS = FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid });        // Use alpha blending for a proper overlay regardless of previous passes
		ID3D11BlendState* BS = URenderer::GetInstance().GetAlphaBlendState();
		FPipelineInfo HeatPipe = { nullptr, HeatVS, RS, DS_Disabled ? DS_Disabled : DS, HeatPS, BS };
		Pipeline->UpdatePipeline(HeatPipe);
		// Ensure SRV t7 and CBs b11,b12 are bound to PS
		Pipeline->SetSRV(7, false /*PS*/, ClusterCountSRV);
		Pipeline->SetConstantBuffer(11, false /*PS*/, FP_CameraCB);
		Pipeline->SetConstantBuffer(12, false /*PS*/, FP_ParamsCB);
		Pipeline->Draw(3, 0);
	}
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

	// Forward+ resources
	SafeRelease(ClusterCountSRV);
	SafeRelease(ClusterCountUAV);
	SafeRelease(ClusterCountBuffer);
	SafeRelease(ClusterIndexSRV);
	SafeRelease(ClusterIndexUAV);
	SafeRelease(ClusterIndexBuffer);
	SafeRelease(FP_CameraCB);
	SafeRelease(FP_ParamsCB);
	SafeRelease(LightTilesCS);
	SafeRelease(VSPhong);
	SafeRelease(PSPhong);
	SafeRelease(VSLambert);
	SafeRelease(PSLambert);
	SafeRelease(VSGouraud);
	SafeRelease(PSGouraud);
	SafeRelease(InputLayout);
    SafeRelease(HeatVS);
    SafeRelease(HeatPS);
    SafeRelease(DS_Disabled);
}
