#include "pch.h"
#include "Renderer/Public/RenderPass/StaticMeshPass.h"
#include "Scene/Public/Component/StaticMeshComponent.h"
#include "Scene/Public/Component/LightComponentBase.h"
#include "Renderer/Public/Pipeline.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Renderer/Public/LightData.h"
#include "Asset/Public/Texture.h"
#include "Editor/Public/Camera.h"
#include "Renderer/Public/ShadowMapManager.h"

FStaticMeshPass::FStaticMeshPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS, ID3D11DepthStencilState* InDisabledDS)
	: FRenderPass(InPipeline), DS(InDS), DisabledDS(InDisabledDS)
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
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/Material/TextureVS.hlsl", TextureLayout, &VSPhong, &InputLayout, TexturePhongDefines);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/Material/TexturePS.hlsl", &PSPhong, TexturePhongDefines);

	D3D_SHADER_MACRO TextureGouraudDefines[] =
	{
		"LIGHTING_MODEL_GOURAUD", "1",
		nullptr, nullptr
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/Material/TextureVS.hlsl", TextureLayout, &VSGouraud, &InputLayout, TextureGouraudDefines);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/Material/TexturePS.hlsl", &PSGouraud, TextureGouraudDefines);

	D3D_SHADER_MACRO TextureLambertDefines[] =
	{
		"LIGHTING_MODEL_LAMBERT", "1",
		nullptr, nullptr
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/Material/TextureVS.hlsl", TextureLayout, &VSLambert, &InputLayout, TextureLambertDefines);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/Material/TexturePS.hlsl", &PSLambert, TextureLambertDefines);

	ConstantBufferMaterial = FRenderResourceFactory::CreateConstantBuffer<FMaterialConstants>();
	ConstantBufferLight = FRenderResourceFactory::CreateConstantBuffer<FLightConstants>();

	// Unified Dynamic Light Buffer (Point, Spot, Rect)
	UnifiedLightCapacity = 128;  // Initial capacity for all dynamic lights
	UnifiedLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FUnifiedDynamicLight>(UnifiedLightCapacity);
	UnifiedLightSRV = FRenderResourceFactory::CreateBufferSRV(UnifiedLightStructuredBuffer, UnifiedLightCapacity);

	SpotLightMatricesCapacity = FShadowMapManager::GetInstance().GetMaxSpotShadows();
	SpotLightMatricesStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FLightViewProj>(SpotLightMatricesCapacity);
	SpotLightMatricesSRV = FRenderResourceFactory::CreateBufferSRV(SpotLightMatricesStructuredBuffer, SpotLightMatricesCapacity);

	CBDirectionalShadowMatrix = FRenderResourceFactory::CreateConstantBuffer<FLightViewProj>();

	FRenderResourceFactory::CreateComputeShader(L"Asset/Shader/Lighting/LightTilesCS.hlsl", &LightTilesCS);

    FP_CameraCB = FRenderResourceFactory::CreateConstantBuffer<FForwardPlusCameraConstants>();
    FP_ParamsCB = FRenderResourceFactory::CreateConstantBuffer<FForwardPlusConstants>();

    // Debug heat overlay shaders (fullscreen VS + heat PS)
    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/Common/BlitVS.hlsl", {}, &HeatVS, nullptr);
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/Lighting/ClusterHeatPS.hlsl", &HeatPS);
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

	// Ceil-div to cover the whole screen with tiles
	uint32 NumTilesX = (Width  + TileSize - 1) / TileSize;
	uint32 NumTilesY = (Height + TileSize - 1) / TileSize;
	uint32 TotalClusters = NumTilesX * NumTilesY * NumZSlices;

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

		SafeRelease(LocalLightCountForHeatmapSRV);
		SafeRelease(LocalLightCountForHeatmapUAV);
		SafeRelease(LocalLightCountForHeatmapBuffer);

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

		// Create LocalLightCountForHeatmap buffer (TotalClusters uints)
		LocalLightCountForHeatmapBuffer = FRenderResourceFactory::CreateStructuredBufferWithUAV(
			sizeof(uint32),
			CountElements,
			&LocalLightCountForHeatmapSRV,
			&LocalLightCountForHeatmapUAV);

        // Cache
        CachedNumTilesX = NumTilesX;
        CachedNumTilesY = NumTilesY;
        CachedNumZSlices = NumZSlices;
        CachedMaxLightsPerCluster = MaxLightsPerCluster;
    }

	auto* CurrentCamera = Context.CurrentCamera;

	FForwardPlusCameraConstants FPcam = {};
	FPcam.View        = CurrentCamera->GetCameraConstants().View;                // row_major
	FPcam.Proj        = CurrentCamera->GetCameraConstants().Projection;          // row_major
	FPcam.InvProj     = CurrentCamera->GetCameraConstantsInverse().Projection;
    FPcam.ScreenSize  = {Width, Height};
    FPcam.ViewportOrigin = { static_cast<uint32>(Context.Viewport.TopLeftX), static_cast<uint32>(Context.Viewport.TopLeftY) };
	FPcam.NumTilesX   = NumTilesX;
	FPcam.NumTilesY   = NumTilesY;
	FPcam.NumZSlices  = NumZSlices;
	FPcam.NearZ       = CurrentCamera->GetNearZ();
	FPcam.FarZ        = CurrentCamera->GetFarZ();

	FForwardPlusConstants FPparams = {};
	FPparams.NumLights            = NumLights;
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

	// Clear UAV buffers to prevent garbage values from previous frames
	// This is critical for orthographic cameras where some threads may early-return without initializing
	UINT clearValue[4] = { 0, 0, 0, 0 };
	ctx->ClearUnorderedAccessViewUint(ClusterCountUAV, clearValue);
	ctx->ClearUnorderedAccessViewUint(LocalLightCountForHeatmapUAV, clearValue);
	// Note: ClusterIndexUAV doesn't need clearing since it's only read at indices < ClusterCount

	// CS UAVs (u0 = counts, u1 = indices, u2 = local light counts for heatmap)
	ID3D11UnorderedAccessView* csUAVs[3] = { ClusterCountUAV, ClusterIndexUAV, LocalLightCountForHeatmapUAV };
	UINT initialCounts[3] = { 0, 0, 0 }; // ignored for structured UAVs
	ctx->CSSetUnorderedAccessViews(0, 3, csUAVs, initialCounts);

	// Set compute shader
	ctx->CSSetShader(LightTilesCS, nullptr, 0);

    // Dispatch with thread groups covering clusters.
    // Must stay in sync with [numthreads] in LightTilesComputeShader.hlsl
    const uint32 GroupSizeX = 8;
    const uint32 GroupSizeY = 8;
    const uint32 GroupSizeZ = 1;

    const uint32 GroupsX = (NumTilesX + GroupSizeX - 1) / GroupSizeX;
    const uint32 GroupsY = (NumTilesY + GroupSizeY - 1) / GroupSizeY;
    const uint32 GroupsZ = (NumZSlices + GroupSizeZ - 1) / GroupSizeZ;

    ctx->Dispatch(GroupsX, GroupsY, GroupsZ);

	// Unbind shader and resources to prevent hazards
	ID3D11UnorderedAccessView* nullUAVs[3] = { nullptr, nullptr, nullptr };
	ctx->CSSetUnorderedAccessViews(0, 3, nullUAVs, initialCounts);

	ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
	ctx->CSSetShaderResources(0, 1, nullSRVs);
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
    FRenderResourceFactory::UpdateStructuredBufferData(
        SpotLightMatricesStructuredBuffer, SpotLightMatrices);
	Pipeline->SetConstantBuffer(4, EShaderType::EST_Pixel, CBDirectionalShadowMatrix);

    // After uploading current lights, build clusters using the compute shader
    CreateClusterBuffers(Context, UnifiedLights.size());

	// Bind Unified Light SRV to the pipeline
	if(Context.ViewMode == EViewModeIndex::VMI_Lit_Gouraud)
	{
		Pipeline->SetSRV(6, EShaderType::EST_Vertex, UnifiedLightSRV);
	}
	else
	{
		// PS also needs the unified light buffer at t6 for clustering
		Pipeline->SetSRV(6, EShaderType::EST_Pixel /*PS*/, UnifiedLightSRV);
		Pipeline->SetSRV(12, EShaderType::EST_Pixel /*PS*/, SpotLightMatricesSRV);
		// Bind Forward+ SRVs for PS
		Pipeline->SetSRV(10, EShaderType::EST_Pixel /*PS*/, ClusterCountSRV);
		Pipeline->SetSRV(11, EShaderType::EST_Pixel /*PS*/, ClusterIndexSRV);

		// Bind Forward+ CBs for PS (slots b11, b12)
		Pipeline->SetConstantBuffer(2, EShaderType::EST_Pixel /*PS*/, FP_CameraCB);
		Pipeline->SetConstantBuffer(3, EShaderType::EST_Pixel /*PS*/, FP_ParamsCB);
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

	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferLight, LightConstants);
	Pipeline->SetConstantBuffer(0, EShaderType::EST_Vertex, ConstantBufferLight);
	Pipeline->SetConstantBuffer(0, EShaderType::EST_Pixel, ConstantBufferLight);

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

		FRenderResourceFactory::UpdateConstantBufferData(Context.ModelCB, ModelConstants);

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

				Pipeline->SetConstantBuffer(1, EShaderType::EST_Vertex, ConstantBufferMaterial);
				Pipeline->SetConstantBuffer(1, EShaderType::EST_Pixel, ConstantBufferMaterial);

				if (UTexture* DiffuseTexture = Material->GetDiffuseTexture())
				{
					Pipeline->SetSRV(0, EShaderType::EST_Pixel, DiffuseTexture->GetTextureSRV());
					Pipeline->SetSamplerState(0, EShaderType::EST_Pixel, DiffuseTexture->GetTextureSampler());
				}
				if (UTexture* AmbientTexture = Material->GetAmbientTexture())
				{
					Pipeline->SetSRV(1, EShaderType::EST_Pixel, AmbientTexture->GetTextureSRV());
				}
				if (UTexture* SpecularTexture = Material->GetSpecularTexture())
				{
					Pipeline->SetSRV(2, EShaderType::EST_Pixel, SpecularTexture->GetTextureSRV());
				}
				if (UTexture* NormalTexture = Material->GetShininessTexture())
				{
					Pipeline->SetSRV(3, EShaderType::EST_Pixel, NormalTexture->GetTextureSRV());
				}
				if (UTexture* AlphaTexture = Material->GetAlphaTexture())
				{
					Pipeline->SetSRV(4, EShaderType::EST_Pixel, AlphaTexture->GetTextureSRV());
				}
				if (UTexture* BumpTexture = Material->GetBumpTexture())
				{
					Pipeline->SetSRV(5, EShaderType::EST_Pixel, BumpTexture->GetTextureSRV());
				}

				CurrentMaterial = Material;
			}
			Pipeline->DrawIndexed(Section.IndexCount, Section.StartIndex, 0);
		}
	}

	// --- Debug: draw cluster heat overlay ---
	bool bOverlayEnabled = (Context.ShowFlags & EEngineShowFlags::SF_ClusterHeat) != 0;
	if (bOverlayEnabled && HeatVS && HeatPS && ClusterCountSRV)
	{
		// Reuse already-bound FP cbuffers (b11,b12) and FP_ClusterCount (t7)
		// Setup fullscreen pipeline with disabled depth
		auto RS = FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid });        // Use alpha blending for a proper overlay regardless of previous passes
		ID3D11BlendState* BS = URenderer::GetInstance().GetAlphaBlendState();
		FPipelineInfo HeatPipe = { nullptr, HeatVS, RS, DisabledDS, HeatPS, BS };
		Pipeline->UpdatePipeline(HeatPipe);
		Pipeline->SetSRV(7, EShaderType::EST_Pixel, LocalLightCountForHeatmapSRV);
		Pipeline->SetConstantBuffer(0, EShaderType::EST_Pixel, FP_CameraCB);
		Pipeline->SetConstantBuffer(1, EShaderType::EST_Pixel, FP_ParamsCB);
		Pipeline->Draw(3, 0);
	}

	Pipeline->SetSRV(6, EShaderType::EST_Pixel, nullptr);
	Pipeline->SetSRV(7, EShaderType::EST_Pixel, nullptr);
	Pipeline->SetSRV(10, EShaderType::EST_Pixel, nullptr);
	Pipeline->SetSRV(11, EShaderType::EST_Pixel, nullptr);
	Pipeline->SetSRV(12, EShaderType::EST_Pixel, nullptr);
	Pipeline->SetSRV(13, EShaderType::EST_Pixel, nullptr);
	Pipeline->SetSRV(14, EShaderType::EST_Pixel, nullptr);
	Pipeline->SetSRV(15, EShaderType::EST_Pixel, nullptr);
	Pipeline->SetSRV(16, EShaderType::EST_Pixel, nullptr);
	Pipeline->SetSRV(17, EShaderType::EST_Pixel, nullptr);
	Pipeline->SetSRV(18, EShaderType::EST_Pixel, nullptr);
}

TArray<FUnifiedDynamicLight> FStaticMeshPass::CollectLightsFromContext(FRenderingContext& Context)
{
	Pipeline->SetSRV(13, EShaderType::EST_Pixel, FShadowMapManager::GetInstance().GetSpotLightSRV());
	Pipeline->SetSRV(14, EShaderType::EST_Pixel, FShadowMapManager::GetInstance().GetSpotMomentsSRV());
	Pipeline->SetSRV(15, EShaderType::EST_Pixel, FShadowMapManager::GetInstance().GetPointLightSRV()); // PCF 용으로 바꿔야함
	Pipeline->SetSRV(16, EShaderType::EST_Pixel, FShadowMapManager::GetInstance().GetPointLightSRV());
	Pipeline->SetSRV(17, EShaderType::EST_Pixel, FShadowMapManager::GetInstance().GetDirectionalLightSRV());
	Pipeline->SetSRV(18, EShaderType::EST_Pixel, FShadowMapManager::GetInstance().GetDirectionalMomentSRV());

	Pipeline->SetSamplerState(1, EShaderType::EST_Pixel, FShadowMapManager::GetInstance().GetSamplerState());
	Pipeline->SetSamplerState(2, EShaderType::EST_Pixel, FShadowMapManager::GetInstance().GetMomentSampler());

	// Collect all dynamic lights into unified buffer
	TArray<FUnifiedDynamicLight> UnifiedLights;
	SpotLightMatrices.resize(FShadowMapManager::GetInstance().GetMaxSpotShadows());

	FCameraConstants CamInv = Context.CurrentCamera->GetCameraConstantsInverse();
	for (ULightComponentBase* Light : Context.Lights)
	{
		if (!Light || !Light->IsVisibleInHierarchy()) continue;

		// [UNIFIED FORWARD RENDERING] All light types (including Ambient) go through StructuredBuffer
		FUnifiedDynamicLight UnifiedLight = Light->GetUnifiedLightData();
		UnifiedLights.push_back(UnifiedLight);

		int32 ShadowMapIdx = Light->GetShadowMapIdx();
		if (Light->DoesCastShadows() && ShadowMapIdx >= 0)
		{
			FLightViewProj LightViewProj;
			LightViewProj.ViewMatrix = Light->GetLightViewMatrices(CamInv)[0];
			LightViewProj.ProjectionMatrix = Light->GetLightProjectionMatrix(CamInv);
			if (Light->GetLightType() == ELightComponentType::LightType_Spot)
			{
				SpotLightMatrices[ShadowMapIdx] = LightViewProj;
			}
			else if (Light->GetLightType() == ELightComponentType::LightType_Directional)
			{
				FRenderResourceFactory::UpdateConstantBufferData(CBDirectionalShadowMatrix, LightViewProj);
			}
		}
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
	SafeRelease(LocalLightCountForHeatmapSRV);
	SafeRelease(LocalLightCountForHeatmapUAV);
	SafeRelease(LocalLightCountForHeatmapBuffer);
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

	SafeRelease(SpotLightMatricesStructuredBuffer);
	SafeRelease(SpotLightMatricesSRV);
	SafeRelease(CBDirectionalShadowMatrix);
}

