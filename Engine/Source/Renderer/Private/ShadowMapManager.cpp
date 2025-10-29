#include "pch.h"
#include "Renderer/Public/ShadowMapManager.h"
#include "Renderer/Public/Renderer.h"
#include "Scene/Public/Component/LightComponentBase.h"

FShadowMapManager::FShadowMapManager()
{
}

FShadowMapManager::~FShadowMapManager()
{
	Release();
}

FShadowMapManager& FShadowMapManager::GetInstance()
{
	static FShadowMapManager Instance;
	return Instance;
}

void FShadowMapManager::Initialize(uint32 InMaxSpotShadows, uint32 InSpotResolution, uint32 InMaxPointShadowCubes, uint32 InPointResolution, uint32 InDirLightResolution)
{
	Device = URenderer::GetInstance().GetDevice();
	Context = URenderer::GetInstance().GetDeviceContext();

	InitializeSpotShadows(InMaxSpotShadows, InSpotResolution);
	InitializePointShadows(InMaxPointShadowCubes, InPointResolution);
	InitializeDirectionalShadow(InDirLightResolution);

    D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	SamplerDesc.BorderColor[0] = 1.0f;
	SamplerDesc.BorderColor[1] = 1.0f;
	SamplerDesc.BorderColor[2] = 1.0f;
	SamplerDesc.BorderColor[3] = 1.0f;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	SamplerDesc.MipLODBias = 0.0f;
	SamplerDesc.MaxAnisotropy = 1;
	SamplerDesc.MinLOD = 0;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	HRESULT hr = Device->CreateSamplerState(&SamplerDesc, &ShadowMapSamplerState);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE SAMPLER STATE");
		return;
	}

	InitializeForDebug();

    // Linear sampler for moments sampling (non-comparison)
    D3D11_SAMPLER_DESC LinearSamp = {};
    LinearSamp.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    LinearSamp.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    LinearSamp.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    LinearSamp.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    LinearSamp.BorderColor[0] = 1.0f;
    LinearSamp.BorderColor[1] = 1.0f;
    LinearSamp.BorderColor[2] = 1.0f;
    LinearSamp.BorderColor[3] = 1.0f;
    LinearSamp.ComparisonFunc = D3D11_COMPARISON_NEVER;
    LinearSamp.MinLOD = 0.0f;
    LinearSamp.MaxLOD = D3D11_FLOAT32_MAX;
    hr = Device->CreateSamplerState(&LinearSamp, &MomentSamplerState);
    if (FAILED(hr)) { return; }
}

void FShadowMapManager::InitializeSpotShadows(uint32 InMaxSpotShadows, uint32 InSpotResolution)
{
	ReleaseSpotShadows();
	MaxSpotShadows = InMaxSpotShadows;
	SpotResolution = InSpotResolution;

    // --- Texture2DArray ---
    D3D11_TEXTURE2D_DESC SpotTexDesc = {};
    SpotTexDesc.Width = SpotResolution;
    SpotTexDesc.Height = SpotResolution;
    SpotTexDesc.MipLevels = 1;
    SpotTexDesc.ArraySize = MaxSpotShadows;
    SpotTexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    SpotTexDesc.SampleDesc.Count = 1;
    SpotTexDesc.SampleDesc.Quality = 0;
    SpotTexDesc.Usage = D3D11_USAGE_DEFAULT;
    SpotTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
    SpotTexDesc.CPUAccessFlags = 0;
    SpotTexDesc.MiscFlags = 0;

    HRESULT hr = Device->CreateTexture2D(&SpotTexDesc, nullptr, &SpotShadowMapArrayTexture);
    if (FAILED(hr))
    {
        UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE TEXTURE");
	    return;
    }

	StatData.VRAM_Spot_Depth = static_cast<uint64_t>(SpotResolution) * SpotResolution * MaxSpotShadows * 4;
	StatData.Config_SpotResolution = SpotResolution;
	StatData.Config_MaxSpotShadows = MaxSpotShadows;

	// --- SRV ---
    D3D11_SHADER_RESOURCE_VIEW_DESC SpotSrvDesc = {};
    SpotSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    SpotSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    SpotSrvDesc.Texture2DArray.MostDetailedMip = 0;
    SpotSrvDesc.Texture2DArray.MipLevels = 1;
    SpotSrvDesc.Texture2DArray.FirstArraySlice = 0;
    SpotSrvDesc.Texture2DArray.ArraySize = MaxSpotShadows;

    hr = Device->CreateShaderResourceView(SpotShadowMapArrayTexture, &SpotSrvDesc, &SpotShadowMapArraySRV);
    if (FAILED(hr))
    {
        UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE SRV");
	    return;
    }

	// --- DSV ---
    D3D11_DEPTH_STENCIL_VIEW_DESC SpotDsvDesc = {};
    SpotDsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    SpotDsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    SpotDsvDesc.Texture2DArray.MipSlice = 0;
    SpotShadowMapSliceDSVs.resize(MaxSpotShadows);

    for (uint32 i = 0; i < MaxSpotShadows; ++i)
    {
        SpotDsvDesc.Texture2DArray.FirstArraySlice = i;
        SpotDsvDesc.Texture2DArray.ArraySize = 1;

        hr = Device->CreateDepthStencilView(SpotShadowMapArrayTexture, &SpotDsvDesc, &SpotShadowMapSliceDSVs[i]);
        if (FAILED(hr))
        {
            SpotShadowMapSliceDSVs.clear();
        	UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE DSV");
            return;
        }
    }

	// --- Moments Texture2DArray (RG32_FLOAT) for VSM ---
	D3D11_TEXTURE2D_DESC MomentsDesc = {};
	MomentsDesc.Width = SpotResolution;
	MomentsDesc.Height = SpotResolution;
	MomentsDesc.MipLevels = 1;
	MomentsDesc.ArraySize = MaxSpotShadows;
	MomentsDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	MomentsDesc.SampleDesc.Count = 1;
	MomentsDesc.SampleDesc.Quality = 0;
	MomentsDesc.Usage = D3D11_USAGE_DEFAULT;
	MomentsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	MomentsDesc.CPUAccessFlags = 0;
	MomentsDesc.MiscFlags = 0;

	hr = Device->CreateTexture2D(&MomentsDesc, nullptr, &SpotShadowMomentsArrayTexture);
	if (FAILED(hr))
	{
		return;
	}
	StatData.VRAM_Spot_Moments = static_cast<uint64>(SpotResolution) * SpotResolution * MaxSpotShadows * 8;

	// SRV for moments
	D3D11_SHADER_RESOURCE_VIEW_DESC MomentsSRVDesc = {};
	MomentsSRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	MomentsSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	MomentsSRVDesc.Texture2DArray.MostDetailedMip = 0;
	MomentsSRVDesc.Texture2DArray.MipLevels = 1;
	MomentsSRVDesc.Texture2DArray.FirstArraySlice = 0;
	MomentsSRVDesc.Texture2DArray.ArraySize = MaxSpotShadows;
	hr = Device->CreateShaderResourceView(SpotShadowMomentsArrayTexture, &MomentsSRVDesc, &SpotShadowMomentsSRV);
	if (FAILED(hr))
	{
		return;
	}

	// RTVs for moments per slice
	D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	RTVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	RTVDesc.Texture2DArray.MipSlice = 0;
	SpotShadowMomentsSliceRTVs.resize(MaxSpotShadows);
	for (uint32 i = 0; i < MaxSpotShadows; ++i)
	{
		RTVDesc.Texture2DArray.FirstArraySlice = i;
		RTVDesc.Texture2DArray.ArraySize = 1;
		hr = Device->CreateRenderTargetView(SpotShadowMomentsArrayTexture, &RTVDesc, &SpotShadowMomentsSliceRTVs[i]);
		if (FAILED(hr))
		{
			SpotShadowMomentsSliceRTVs.clear();
		}
	}

	UpdateTotalVRAMStats();
}

void FShadowMapManager::InitializePointShadows(uint32 InMaxPointShadowCubes, uint32 InPointResolution)
{
    ReleasePointShadows();
    MaxPointShadowCubes = InMaxPointShadowCubes;
    PointResolution = InPointResolution;

    D3D11_TEXTURE2D_DESC RTVTexDesc = {};
    RTVTexDesc.Width = PointResolution;
    RTVTexDesc.Height = PointResolution;
    RTVTexDesc.MipLevels = 1;
    RTVTexDesc.ArraySize = MaxPointShadowCubes * 6;
    RTVTexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    RTVTexDesc.SampleDesc.Count = 1;
    RTVTexDesc.SampleDesc.Quality = 0;
    RTVTexDesc.Usage = D3D11_USAGE_DEFAULT;
    RTVTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    RTVTexDesc.CPUAccessFlags = 0;
    RTVTexDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    HRESULT hr = Device->CreateTexture2D(&RTVTexDesc, nullptr, &PointShadowCubeArrayTexture);
    if (FAILED(hr))
    {
        UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE RTV TEXTURE");
        return;
    }

	StatData.VRAM_Point_Moments = static_cast<uint64>(PointResolution) * PointResolution * (MaxPointShadowCubes * 6) * 4;
	StatData.Config_PointResolution = PointResolution;
	StatData.Config_MaxPointShadowCubes = MaxPointShadowCubes;

    // --- SRV ---
    D3D11_SHADER_RESOURCE_VIEW_DESC PointSrvDesc = {};
    PointSrvDesc.Format = DXGI_FORMAT_R32_FLOAT; // VSM시에 R32G32 로 바꿔야함
    PointSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
    PointSrvDesc.TextureCubeArray.MostDetailedMip = 0;
    PointSrvDesc.TextureCubeArray.MipLevels = 1;
    PointSrvDesc.TextureCubeArray.First2DArrayFace = 0;
    PointSrvDesc.TextureCubeArray.NumCubes = MaxPointShadowCubes;

    hr = Device->CreateShaderResourceView(PointShadowCubeArrayTexture, &PointSrvDesc, &PointShadowCubeArraySRV);
    if (FAILED(hr))
    {
        UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE SRV");
        return;
    }

	// // --- SRV 2 (PCF용) ---
	// D3D11_SHADER_RESOURCE_VIEW_DESC PcfSrvDesc = {};
	// PcfSrvDesc.Format = DXGI_FORMAT_R32_FLOAT; // float1
	// PcfSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
	// PcfSrvDesc.TextureCubeArray.MostDetailedMip = 0;
	// PcfSrvDesc.TextureCubeArray.MipLevels = 1;
	// PcfSrvDesc.TextureCubeArray.First2DArrayFace = 0;
	// PcfSrvDesc.TextureCubeArray.NumCubes = MaxPointShadowCubes;
	//
	// // 리소스는 '동일한' 텍스처를 사용
	// hr = Device->CreateShaderResourceView(PointShadowCubeArrayTexture, &PcfSrvDesc, &PointShadowCubeArraySRV_PCF); // PCF용 SRV
	// if (FAILED(hr))
	// {
	// 	UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE PCF SRV");
	// 	return;
	// }

    // --- RTV Array ---
    D3D11_RENDER_TARGET_VIEW_DESC PointRtvDesc = {};
    PointRtvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    PointRtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    PointRtvDesc.Texture2DArray.MipSlice = 0;

    PointShadowCubeSliceRTVs.resize(MaxPointShadowCubes * 6);

    for (uint32 i = 0; i < MaxPointShadowCubes * 6; ++i)
    {
        PointRtvDesc.Texture2DArray.FirstArraySlice = i;
        PointRtvDesc.Texture2DArray.ArraySize = 1;

        hr = Device->CreateRenderTargetView(PointShadowCubeArrayTexture, &PointRtvDesc, &PointShadowCubeSliceRTVs[i]);
        if (FAILED(hr))
        {
            PointShadowCubeSliceRTVs.clear();
            UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE RTV");
            return;
        }
    }

	// --- 섀도우 패스 전용 DSV ---
	D3D11_TEXTURE2D_DESC DepthTexDesc = {};
	DepthTexDesc.Width = PointResolution;
	DepthTexDesc.Height = PointResolution;
	DepthTexDesc.MipLevels = 1;
	DepthTexDesc.ArraySize = 1;
	DepthTexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	DepthTexDesc.SampleDesc.Count = 1;
	DepthTexDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	DepthTexDesc.CPUAccessFlags = 0;
	DepthTexDesc.MiscFlags = 0;

	hr = Device->CreateTexture2D(&DepthTexDesc, nullptr, &PointShadowDepthTexture);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE SHADOW DSV TEXTURE");
		return;
	}
	StatData.VRAM_Point_Pass_DSV = static_cast<uint64>(PointResolution) * PointResolution * 1 * 4;

	D3D11_DEPTH_STENCIL_VIEW_DESC PointDsvDesc = {};
	PointDsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	PointDsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	PointDsvDesc.Texture2D.MipSlice = 0;

	hr = Device->CreateDepthStencilView(PointShadowDepthTexture, &PointDsvDesc, &PointShadowDepthDSV);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE SHADOW DSV");
		return;
	}

	UpdateTotalVRAMStats();
}

void FShadowMapManager::InitializeDirectionalShadow(uint32 InResolution)
{
	ReleaseDirectionalShadow();
	DirResolution = InResolution;

	// --- 단일 Texture2D 생성 ---
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width = DirResolution;
	TexDesc.Height = DirResolution;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = DirLightMaxNumCascades;
	TexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.Usage = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
	TexDesc.CPUAccessFlags = 0;
	TexDesc.MiscFlags = 0;

	HRESULT hr = Device->CreateTexture2D(&TexDesc, nullptr, &DirShadowTexture);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE DIR LIGHT TEXTURE");
		return;
	}

	//StatData.VRAM_Directional_Depth = static_cast<uint64_t>(DirResolution) * DirResolution * 1 * 4;
	StatData.VRAM_Directional_Depth = static_cast<uint64_t>(DirResolution) * DirResolution * DirLightMaxNumCascades * 4;
	StatData.Config_DirResolution = InResolution;

	// --- SRVs (Texture2D) ---
	D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SrvDesc.Texture2DArray.MostDetailedMip = 0;
	SrvDesc.Texture2DArray.MipLevels = 1;
	SrvDesc.Texture2DArray.FirstArraySlice = 0;
	SrvDesc.Texture2DArray.ArraySize = DirLightMaxNumCascades;

	hr = Device->CreateShaderResourceView(DirShadowTexture, &SrvDesc, &DirShadowSRV);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE DIR LIGHT SRV");
		return;
	}

	// --- DSVs (Texture2D) ---
	D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
	DsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
	DsvDesc.Texture2DArray.MipSlice = 0;
	DsvDesc.Flags = 0;
	 
	DirLightCascadeDSVs.resize(DirLightMaxNumCascades);
	for (uint32 i = 0; i < DirLightMaxNumCascades; ++i)
	{
		DsvDesc.Texture2DArray.FirstArraySlice = i;
		DsvDesc.Texture2DArray.ArraySize = 1;
		hr = Device->CreateDepthStencilView(DirShadowTexture, &DsvDesc, &DirLightCascadeDSVs[i]);
		if (FAILED(hr))
		{
			UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE DIR LIGHT DSV SLICE");
			DirLightCascadeDSVs.clear();
			return;
		}
	} 
	DirShadowDSV = DirLightCascadeDSVs[0];

	// --- Moments Texture2DArray (RG32_FLOAT) for VSM ---
	D3D11_TEXTURE2D_DESC MomentsDesc = {};
	MomentsDesc.Width = DirResolution;
	MomentsDesc.Height = DirResolution;
	MomentsDesc.MipLevels = 1;
	MomentsDesc.ArraySize = 1;
	MomentsDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	MomentsDesc.SampleDesc.Count = 1;
	MomentsDesc.SampleDesc.Quality = 0;
	MomentsDesc.Usage = D3D11_USAGE_DEFAULT;
	MomentsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	MomentsDesc.CPUAccessFlags = 0;
	MomentsDesc.MiscFlags = 0;

	hr = Device->CreateTexture2D(&MomentsDesc, nullptr, &DirShadowMomentTexture);
	if (FAILED(hr)) { return; }

	StatData.VRAM_Directional_Moments = static_cast<uint64_t>(DirResolution) * DirResolution * 1 * 8;

	// SRV for moments
	D3D11_SHADER_RESOURCE_VIEW_DESC MomentsSRVDesc = {};
	MomentsSRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	MomentsSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	MomentsSRVDesc.Texture2D.MostDetailedMip = 0;
	MomentsSRVDesc.Texture2D.MipLevels = 1;
	hr = Device->CreateShaderResourceView(DirShadowMomentTexture, &MomentsSRVDesc, &DirShadowMomentSRV);
	if (FAILED(hr))
	{
		return;
	}

	// RTVs for moments per slice
	D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	RTVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Texture2D.MipSlice = 0;
	hr = Device->CreateRenderTargetView(DirShadowMomentTexture, &RTVDesc, &DirShadowMomentRTV);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE DIR LIGHT MOMENT RTV");
		return;
	}


	//InitializeForDebug();

	UpdateTotalVRAMStats();
}

void FShadowMapManager::ReleaseSpotShadows()
{
	SafeRelease(SpotShadowMapArrayTexture);
	SafeRelease(SpotShadowMapArraySRV);
	for (uint32 Idx = 0; Idx < MaxSpotShadows; ++Idx)
	{
		SafeRelease(SpotShadowMapSliceDSVs[Idx]);
	}

	SafeRelease(SpotShadowMomentsArrayTexture);
	SafeRelease(SpotShadowMomentsSRV);
	for (uint32 Idx = 0; Idx < MaxSpotShadows; ++Idx)
	{
		SafeRelease(SpotShadowMomentsSliceRTVs[Idx]);
	}
}

void FShadowMapManager::ReleasePointShadows()
{
	SafeRelease(PointShadowCubeArrayTexture);
	SafeRelease(PointShadowCubeArraySRV);
	for (uint32 Idx = 0; Idx < MaxPointShadowCubes * 6; ++Idx)
	{
		SafeRelease(PointShadowCubeSliceRTVs[Idx]);
	}
	SafeRelease(PointShadowDepthTexture);
	SafeRelease(PointShadowDepthDSV);
}

void FShadowMapManager::ReleaseDirectionalShadow()
{
	for (auto* dsv : DirLightCascadeDSVs) { SafeRelease(dsv); }
	DirLightCascadeDSVs.clear();

	//DirShadwoDSV = DirLightCascadeDSVs[0]으로 사용중 
	//SafeRelease(DirShadowDSV);
	SafeRelease(DirShadowSRV);
	SafeRelease(DirShadowTexture);

	for (auto* tex : ImGuiDebugTextures_Dir) { SafeRelease(tex); }
	ImGuiDebugTextures_Dir.clear();
	for (auto* srv : ImGuiDebugSRVs_Dir) { SafeRelease(srv); }
	ImGuiDebugSRVs_Dir.clear();

	SafeRelease(DirShadowMomentTexture);
	SafeRelease(DirShadowMomentSRV);
	SafeRelease(DirShadowMomentRTV);
}

void FShadowMapManager::Release()
{
	ReleaseSpotShadows();
	ReleasePointShadows();
	ReleaseDirectionalShadow();
    SafeRelease(MomentSamplerState);
    SafeRelease(ShadowMapSamplerState);
}

void FShadowMapManager::ClearShadowMaps()
{
	const float ClearColor[4] = { 1.0f, 1.0f, 0.0f, 0.0f }; // M1=1, M2=1 initializes no-occluder

	for (uint32 Idx = 0; Idx < CurrentSpotShadowIdx; ++Idx)
	{
		Context->ClearDepthStencilView(SpotShadowMapSliceDSVs[Idx], D3D11_CLEAR_DEPTH, 1.0f, 0);
		Context->ClearRenderTargetView(SpotShadowMomentsSliceRTVs[Idx], ClearColor);
	}
	CurrentSpotShadowIdx = 0;

	for (uint32 Idx = 0; Idx < CurrentPointCubeIdx * 6; ++Idx)
	{
		Context->ClearRenderTargetView(PointShadowCubeSliceRTVs[Idx], ClearColor);
	}
	CurrentPointCubeIdx = 0;
	Context->ClearDepthStencilView(PointShadowDepthDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	Context->ClearDepthStencilView(DirShadowDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
	Context->ClearRenderTargetView(DirShadowMomentRTV, ClearColor);

	for (uint32 i = 0; i < DirLightMaxNumCascades; ++i)
		if (i < DirLightCascadeDSVs.size() && DirLightCascadeDSVs[i])
			Context->ClearDepthStencilView(DirLightCascadeDSVs[i], D3D11_CLEAR_DEPTH, 1.0f, 0);
	 
	bIsDirShadowAllocated = false;
}

void FShadowMapManager::AllocateShadowMap(class ULightComponentBase* Light)
{
	switch (Light->GetLightType())
	{
	case ELightComponentType::LightType_Spot:
		if (CurrentSpotShadowIdx <= MaxSpotShadows)
		{
			Light->SetShadowMapIdx(CurrentSpotShadowIdx++);
			return;
		}
		break;
	case ELightComponentType::LightType_Point:
		if (CurrentPointCubeIdx <= MaxPointShadowCubes)
		{
			Light->SetShadowMapIdx(CurrentPointCubeIdx++);
			return;
		}
		break;
	case ELightComponentType::LightType_Directional:
		if (!bIsDirShadowAllocated)
		{
			bIsDirShadowAllocated = true;
			Light->SetShadowMapIdx(0);
			return;
		}
		break;
	default:
		break;
	}

	Light->SetShadowMapIdx(-1);
}

uint32 FShadowMapManager::GetResolution(class ULightComponentBase* Light) const
{
	switch (Light->GetLightType())
	{
	case ELightComponentType::LightType_Spot:
		return SpotResolution;
	case ELightComponentType::LightType_Point:
		return PointResolution;
	case ELightComponentType::LightType_Directional:
		return DirResolution;
	default:
		break;
	}
	return 0;
}

ID3D11DepthStencilView* FShadowMapManager::GetSpotLightDSV(uint32 SpotShadowIdx) const
{
	if (SpotShadowIdx < 0 || SpotShadowIdx >= MaxSpotShadows)
	{
		return nullptr;
	}
	return SpotShadowMapSliceDSVs[SpotShadowIdx];
}

void FShadowMapManager::GetPointShadowRTVs(class ULightComponentBase* Light, TArray<ID3D11RenderTargetView*>& OutRTVs) const
{
	OutRTVs.clear();
	int32 ShadowMapIdx = Light->GetShadowMapIdx();

	if (ShadowMapIdx < 0) { return; }

	if (CurrentPointCubeIdx <= MaxPointShadowCubes)
	{
		for (uint32 Idx = 0; Idx < 6; ++Idx)
		{
			OutRTVs.emplace_back(PointShadowCubeSliceRTVs[ShadowMapIdx * 6 + Idx]);
		}
	}
}

ID3D11DepthStencilView* FShadowMapManager::GetDirectionalLightDSV(uint32 CascadeIdx) const
{
	if (CascadeIdx >= DirLightMaxNumCascades) return nullptr;
	return DirLightCascadeDSVs[CascadeIdx];
}

void FShadowMapManager::InitializeForDebug()
{
	// 0. 필수 오브젝트 확인
    if (!Device || !SpotShadowMapArrayTexture || !PointShadowCubeArrayTexture)
    {
        UE_LOG_ERROR("[ShadowMapManager] Debug Init Failed: Device or Source Textures are null.");
        return;
    }

    HRESULT hr;
    // --- 1. 스포트라이트 디버그 리소스 생성 ---
    {
        D3D11_TEXTURE2D_DESC SrcDesc;
        SpotShadowMapArrayTexture->GetDesc(&SrcDesc);

        D3D11_TEXTURE2D_DESC Desc = {};
        Desc.Width = SrcDesc.Width;
        Desc.Height = SrcDesc.Height;
        Desc.MipLevels = 1;
        Desc.ArraySize = 1;
        Desc.Format = DXGI_FORMAT_R32_FLOAT;
        Desc.SampleDesc.Count = 1;
        Desc.Usage = D3D11_USAGE_DEFAULT;
        Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        Desc.CPUAccessFlags = 0;
        Desc.MiscFlags = 0;

        hr = Device->CreateTexture2D(&Desc, nullptr, &ImGuiDebugTexture_Spot);
        if (FAILED(hr))
        {
            UE_LOG_ERROR("[ShadowMapManager] Failed to create Spot Debug Texture.");
            return;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
        SrvDesc.Format = Desc.Format;
        SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SrvDesc.Texture2D.MostDetailedMip = 0;
        SrvDesc.Texture2D.MipLevels = 1;

        hr = Device->CreateShaderResourceView(ImGuiDebugTexture_Spot, &SrvDesc, &ImGuiDebugSRV_Spot);
        if (FAILED(hr))
        {
            UE_LOG_ERROR("[ShadowMapManager] Failed to create Spot Debug SRV.");
            return;
        }
    }

    // --- 2. 포인트라이트 디버그 리소스 생성 ---
    {
        D3D11_TEXTURE2D_DESC SrcDesc;
        PointShadowCubeArrayTexture->GetDesc(&SrcDesc);

        D3D11_TEXTURE2D_DESC Desc = {};
        Desc.Width = SrcDesc.Width;
        Desc.Height = SrcDesc.Height;
        Desc.MipLevels = 1;
        Desc.ArraySize = 1;
        Desc.Format = DXGI_FORMAT_R32_FLOAT;
        Desc.SampleDesc.Count = 1;
        Desc.Usage = D3D11_USAGE_DEFAULT;
        Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        Desc.CPUAccessFlags = 0;
        Desc.MiscFlags = 0;

    	for (uint32 Idx = 0; Idx < 6; Idx++)
    	{
    		hr = Device->CreateTexture2D(&Desc, nullptr, &ImGuiDebugTextures_Point[Idx]);
    		if (FAILED(hr))
    		{
    			UE_LOG_ERROR("[ShadowMapManager] Failed to create Point Debug Texture.");
    			return;
    		}
    	}

        // SRV 생성
        D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
        SrvDesc.Format = Desc.Format;
        SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SrvDesc.Texture2D.MostDetailedMip = 0;
        SrvDesc.Texture2D.MipLevels = 1;

    	for (uint32 Idx = 0; Idx < 6; Idx++)
    	{
    		hr = Device->CreateShaderResourceView(ImGuiDebugTextures_Point[Idx], &SrvDesc, &ImGuiDebugSRVs_Point[Idx]);
    		if (FAILED(hr))
    		{
    			UE_LOG_ERROR("[ShadowMapManager] Failed to create Point Debug SRV.");
    			return;
    		}
    	}
    }

	// --- 3. 디렉셔널 라이트 디버그 리소스 생성 ---
	{
		D3D11_TEXTURE2D_DESC srcDesc = {};
		DirShadowTexture->GetDesc(&srcDesc);

		ImGuiDebugTextures_Dir.resize(DirLightMaxNumCascades, nullptr);
		ImGuiDebugSRVs_Dir.resize(DirLightMaxNumCascades, nullptr);

		D3D11_TEXTURE2D_DESC Desc = {};
		Desc.Width = srcDesc.Width;
		Desc.Height = srcDesc.Height;
		Desc.MipLevels = 1;
		Desc.ArraySize = 1;
		Desc.Format = DXGI_FORMAT_R32_FLOAT; // SRV 읽기용
		Desc.SampleDesc.Count = 1;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		for (uint32 i = 0; i < DirLightMaxNumCascades; ++i)
		{
			HRESULT hr = Device->CreateTexture2D(&Desc, nullptr, &ImGuiDebugTextures_Dir[i]);
			if (FAILED(hr)) { UE_LOG_ERROR("Create Dir Debug Tex failed"); return; }

			D3D11_SHADER_RESOURCE_VIEW_DESC sdesc = {};
			sdesc.Format = Desc.Format;
			sdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			sdesc.Texture2D.MostDetailedMip = 0;
			sdesc.Texture2D.MipLevels = 1;

			hr = Device->CreateShaderResourceView(ImGuiDebugTextures_Dir[i], &sdesc, &ImGuiDebugSRVs_Dir[i]);
			if (FAILED(hr)) { UE_LOG_ERROR("Create Dir Debug SRV failed"); return; }
		}
	}
}

ID3D11ShaderResourceView* FShadowMapManager::GetSpotSRVForImGuiDebug(uint32 SpotIndex)
{
    if (!Context || !SpotShadowMapArrayTexture || !ImGuiDebugTexture_Spot || !ImGuiDebugSRV_Spot) { return nullptr; }
    if (SpotIndex >= MaxSpotShadows) { return nullptr; }

    UINT SrcSubresource = D3D11CalcSubresource(0, SpotIndex, 1);
    UINT DstSubresource = 0;

    // 리소스 복사 (GPU 작업)
    Context->CopySubresourceRegion(ImGuiDebugTexture_Spot, DstSubresource,0, 0, 0,
        SpotShadowMapArrayTexture, SrcSubresource, nullptr
    );

    return ImGuiDebugSRV_Spot;
}

void FShadowMapManager::UpdatePointShadowDebugTextures(uint32 CubeIndex)
{
	if (!Context || !PointShadowCubeArrayTexture || CubeIndex >= MaxPointShadowCubes)
	{
		return;
	}

	// 6개의 모든 면을 순회하며 개별 텍스처에 복사
	for (uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
	{
		if (!ImGuiDebugTextures_Point[FaceIndex])
		{
			continue; // 디버그 텍스처가 준비되지 않음
		}

		uint32 FinalSliceIndex = (CubeIndex * 6) + FaceIndex;
		UINT SrcSubresource = D3D11CalcSubresource(0, FinalSliceIndex, 1);
		UINT DstSubresource = 0; // 각 2D 텍스처의 0번 서브리소스

		Context->CopySubresourceRegion(
			ImGuiDebugTextures_Point[FaceIndex], DstSubresource, 0, 0, 0,
			PointShadowCubeArrayTexture, SrcSubresource, nullptr
		);
	}
}

ID3D11ShaderResourceView* FShadowMapManager::GetPointSRVForImGuiDebug(uint32 CubeIndex, uint32 FaceIndex)
{
	if (FaceIndex >= 6)
	{
		return nullptr;
	}
	return ImGuiDebugSRVs_Point[FaceIndex];
}

ID3D11ShaderResourceView* FShadowMapManager::GetDirectionalSRVForImGuiDebug()
{
	// 1. 유효성 검사
	if (!Context || !DirShadowTexture || !ImGuiDebugTexture_Dir || !ImGuiDebugSRV_Dir)
	{
		return nullptr;
	}

	// 2. 원본/대상 서브리소스 인덱스 (둘 다 0)
	UINT srcSubresource = 0; // D3D11CalcSubresource(0, 0, 1)
	UINT dstSubresource = 0;

	// 3. 리소스 복사 (GPU 작업)
	Context->CopySubresourceRegion(
		ImGuiDebugTexture_Dir,      // 대상
		dstSubresource,             // 대상 서브리소스
		0, 0, 0,                    // 대상 위치
		DirShadowTexture,      // 원본
		srcSubresource,             // 원본 서브리소스
		nullptr                     // 전체 영역 복사
	);

	// 4. 복사된 텍스처의 SRV 반환
	return ImGuiDebugSRV_Dir;
}

ID3D11ShaderResourceView* FShadowMapManager::GetDirectionalSRVForImGuiDebug(uint32 CascadeIdx)
{
	if (!Context || CascadeIdx >= DirLightMaxNumCascades) return nullptr;

	const UINT srcSubresource = D3D11CalcSubresource(0, CascadeIdx, 1);
	const UINT dstSubresource = 0;

	Context->CopySubresourceRegion(
		ImGuiDebugTextures_Dir[CascadeIdx], dstSubresource, 0, 0, 0,
		DirShadowTexture, srcSubresource, nullptr);

	return ImGuiDebugSRVs_Dir[CascadeIdx];
}


const FShadowStatData& FShadowMapManager::GetShadowStats() const
{
	StatData.Allocated_Directional = bIsDirShadowAllocated ? 1 : 0;
	StatData.Allocated_Spot = CurrentSpotShadowIdx;
	StatData.Allocated_PointCubes = CurrentPointCubeIdx;

	return StatData;
}

void FShadowMapManager::UpdateTotalVRAMStats()
{
	StatData.VRAM_Total =
			StatData.VRAM_Directional_Depth +
			StatData.VRAM_Directional_Moments +
			StatData.VRAM_Spot_Depth +
			StatData.VRAM_Spot_Moments +
			StatData.VRAM_Point_Moments +
			StatData.VRAM_Point_Pass_DSV;
}
