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

void FShadowMapManager::Initialize(EShadowFilterType InFilterType, uint32 InMaxSpotShadows, uint32 InSpotResolution, uint32 InMaxPointShadowCubes, uint32 InPointResolution, uint32 InDirLightResolution)
{
	Device = URenderer::GetInstance().GetDevice();
	Context = URenderer::GetInstance().GetDeviceContext();

	ShadowSettings.FilterType = InFilterType;

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
	ShadowSettings.MaxSpotShadows = InMaxSpotShadows;
    ShadowSettings.SpotResolution = InSpotResolution;
    StatData.Config_SpotResolution = ShadowSettings.SpotResolution;
    StatData.Config_MaxSpotShadows = ShadowSettings.MaxSpotShadows;

	SpotShadowMapSliceDSVs.resize(ShadowSettings.MaxSpotShadows);
	SpotShadowMomentsSliceRTVs.resize(ShadowSettings.MaxSpotShadows);

    HRESULT hr;

    if (ShadowSettings.FilterType == EShadowFilterType::SFT_None || ShadowSettings.FilterType == EShadowFilterType::SFT_PCF)
    {
        // --- (PCF) Texture2DArray ---
        D3D11_TEXTURE2D_DESC SpotTexDesc = {};
        SpotTexDesc.Width = ShadowSettings.SpotResolution;
        SpotTexDesc.Height = ShadowSettings.SpotResolution;
        SpotTexDesc.MipLevels = 1;
        SpotTexDesc.ArraySize = ShadowSettings.MaxSpotShadows;
        SpotTexDesc.Format = DXGI_FORMAT_R32_TYPELESS; // PCF용
        SpotTexDesc.SampleDesc.Count = 1;
        SpotTexDesc.Usage = D3D11_USAGE_DEFAULT;
        SpotTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL; // PCF용
        SpotTexDesc.CPUAccessFlags = 0;
        SpotTexDesc.MiscFlags = 0;

        hr = Device->CreateTexture2D(&SpotTexDesc, nullptr, &SpotShadowMapArrayTexture);
        if (FAILED(hr))
        {
            UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE TEXTURE");
            return;
        }

        StatData.VRAM_Spot_Depth = static_cast<uint64_t>(ShadowSettings.SpotResolution) * ShadowSettings.SpotResolution * ShadowSettings.MaxSpotShadows * 4;

        // --- (PCF) SRV ---
        D3D11_SHADER_RESOURCE_VIEW_DESC SpotSrvDesc = {};
        SpotSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        SpotSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        SpotSrvDesc.Texture2DArray.MipLevels = 1;
    	SpotSrvDesc.Texture2DArray.ArraySize = ShadowSettings.MaxSpotShadows;
    	SpotSrvDesc.Texture2DArray.MostDetailedMip = 0;
    	SpotSrvDesc.Texture2DArray.FirstArraySlice = 0;

        hr = Device->CreateShaderResourceView(SpotShadowMapArrayTexture, &SpotSrvDesc, &SpotShadowMapArraySRV);
        if (FAILED(hr))
        {
            UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE SRV");
            return;
        }

        // --- (PCF) DSV ---
        D3D11_DEPTH_STENCIL_VIEW_DESC SpotDsvDesc = {};
        SpotDsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        SpotDsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        SpotDsvDesc.Texture2DArray.MipSlice = 0;

        for (uint32 i = 0; i < ShadowSettings.MaxSpotShadows; ++i)
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
    }
    else // VSM 계열
    {
        // --- (VSM) Moments Texture2DArray ---
        D3D11_TEXTURE2D_DESC MomentsDesc = {};
        MomentsDesc.Width = ShadowSettings.SpotResolution;
        MomentsDesc.Height = ShadowSettings.SpotResolution;
        MomentsDesc.MipLevels = 1;
        MomentsDesc.ArraySize = ShadowSettings.MaxSpotShadows;
        MomentsDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        MomentsDesc.SampleDesc.Count = 1;
        MomentsDesc.Usage = D3D11_USAGE_DEFAULT;
        MomentsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        MomentsDesc.CPUAccessFlags = 0;
        MomentsDesc.MiscFlags = 0;

        hr = Device->CreateTexture2D(&MomentsDesc, nullptr, &SpotShadowMapArrayTexture);
        if (FAILED(hr))
        {
           return;
        }
        StatData.VRAM_Spot_Moments = static_cast<uint64>(ShadowSettings.SpotResolution) * ShadowSettings.SpotResolution * ShadowSettings.MaxSpotShadows * 8;

        // --- (VSM) SRV ---
        D3D11_SHADER_RESOURCE_VIEW_DESC MomentsSRVDesc = {};
        MomentsSRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        MomentsSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        MomentsSRVDesc.Texture2DArray.MipLevels = 1;
        MomentsSRVDesc.Texture2DArray.ArraySize = ShadowSettings.MaxSpotShadows;
    	MomentsSRVDesc.Texture2DArray.FirstArraySlice = 0;
    	MomentsSRVDesc.Texture2DArray.MostDetailedMip = 0;

        hr = Device->CreateShaderResourceView(SpotShadowMapArrayTexture, &MomentsSRVDesc, &SpotShadowMapArraySRV);
        if (FAILED(hr))
        {
           return;
        }

        // --- (VSM) RTVs ---
        D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
        RTVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        RTVDesc.Texture2DArray.MipSlice = 0;
        for (uint32 i = 0; i < ShadowSettings.MaxSpotShadows; ++i)
        {
           RTVDesc.Texture2DArray.FirstArraySlice = i;
           RTVDesc.Texture2DArray.ArraySize = 1;

           hr = Device->CreateRenderTargetView(SpotShadowMapArrayTexture, &RTVDesc, &SpotShadowMomentsSliceRTVs[i]);
           if (FAILED(hr))
           {
              SpotShadowMomentsSliceRTVs.clear();
           }
        }
    }

    UpdateTotalVRAMStats();
}

void FShadowMapManager::InitializePointShadows(uint32 InMaxPointShadowCubes, uint32 InPointResolution)
{
    ReleasePointShadows();
    ShadowSettings.MaxPointShadows = InMaxPointShadowCubes;
	ShadowSettings.PointResolution = InPointResolution;

	HRESULT hr;

    DXGI_FORMAT TextureFormat;
    DXGI_FORMAT SRVFormat;
    DXGI_FORMAT RTVFormat;
    uint64 BytesPerPixel;

    if (ShadowSettings.FilterType == EShadowFilterType::SFT_None || ShadowSettings.FilterType == EShadowFilterType::SFT_PCF)
    {
        TextureFormat = DXGI_FORMAT_R32_TYPELESS;
        SRVFormat = DXGI_FORMAT_R32_FLOAT;
        RTVFormat = DXGI_FORMAT_R32_FLOAT;
        BytesPerPixel = 4;
    }
    else
    {
        TextureFormat = DXGI_FORMAT_R32G32_TYPELESS;
        SRVFormat = DXGI_FORMAT_R32G32_FLOAT;
        RTVFormat = DXGI_FORMAT_R32G32_FLOAT;
        BytesPerPixel = 8;
    }

    // --- TextureCubeArray ---
    D3D11_TEXTURE2D_DESC RTVTexDesc = {};
    RTVTexDesc.Width = ShadowSettings.PointResolution;
    RTVTexDesc.Height = ShadowSettings.PointResolution;
    RTVTexDesc.MipLevels = 1;
    RTVTexDesc.ArraySize = ShadowSettings.MaxPointShadows * 6;
    RTVTexDesc.Format = TextureFormat;
    RTVTexDesc.SampleDesc.Count = 1;
    RTVTexDesc.SampleDesc.Quality = 0;
    RTVTexDesc.Usage = D3D11_USAGE_DEFAULT;
    RTVTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    RTVTexDesc.CPUAccessFlags = 0;
    RTVTexDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    hr = Device->CreateTexture2D(&RTVTexDesc, nullptr, &PointShadowCubeArrayTexture);
    if (FAILED(hr))
    {
        UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE POINT SHADOW TEXTURE");
        return;
    }

	if (ShadowSettings.FilterType == EShadowFilterType::SFT_None || ShadowSettings.FilterType == EShadowFilterType::SFT_PCF)
    {
        StatData.VRAM_Point_RTV = static_cast<uint64>(ShadowSettings.PointResolution) * ShadowSettings.PointResolution * (ShadowSettings.MaxPointShadows * 6) * BytesPerPixel;
    }
    else
    {
        StatData.VRAM_Point_Moments = static_cast<uint64>(ShadowSettings.PointResolution) * ShadowSettings.PointResolution * (ShadowSettings.MaxPointShadows * 6) * BytesPerPixel;
    }

    // --- SRV 생성 (TextureCubeArray) ---
    D3D11_SHADER_RESOURCE_VIEW_DESC PointSrvDesc = {};
    PointSrvDesc.Format = SRVFormat;
    PointSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
    PointSrvDesc.TextureCubeArray.MostDetailedMip = 0;
    PointSrvDesc.TextureCubeArray.MipLevels = 1;
    PointSrvDesc.TextureCubeArray.First2DArrayFace = 0;
    PointSrvDesc.TextureCubeArray.NumCubes = ShadowSettings.MaxPointShadows;

    hr = Device->CreateShaderResourceView(PointShadowCubeArrayTexture, &PointSrvDesc, &PointShadowCubeArraySRV);
    if (FAILED(hr))
    {
        UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE SRV");
        return;
    }

    // --- RTV Array 생성 (Texture2DArray 뷰) ---
    D3D11_RENDER_TARGET_VIEW_DESC PointRtvDesc = {};
    PointRtvDesc.Format = RTVFormat;
    PointRtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    PointRtvDesc.Texture2DArray.MipSlice = 0;

    PointShadowCubeSliceRTVs.resize(ShadowSettings.MaxPointShadows * 6);

    for (uint32 i = 0; i < ShadowSettings.MaxPointShadows * 6; ++i)
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

    // --- 섀도우 패스 전용 공용 DSV ---
    D3D11_TEXTURE2D_DESC DepthTexDesc = {};
    DepthTexDesc.Width = ShadowSettings.PointResolution;
    DepthTexDesc.Height = ShadowSettings.PointResolution;
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
    StatData.VRAM_Point_Pass_DSV = static_cast<uint64>(ShadowSettings.PointResolution) * ShadowSettings.PointResolution * 1 * 4;

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

    // --- 공통 설정 ---
    StatData.Config_PointResolution = ShadowSettings.PointResolution;
    StatData.Config_MaxPointShadowCubes = ShadowSettings.MaxPointShadows;
    UpdateTotalVRAMStats();
}

void FShadowMapManager::InitializeDirectionalShadow(uint32 InResolution)
{
	ReleaseDirectionalShadow();
    ShadowSettings.DirResolution = InResolution;
    StatData.Config_SpotResolution = ShadowSettings.DirResolution;

	DirShadowMomentRTVs.resize(DirShadowCascadesMax);
	DirShadowCascadeDSVs.resize(DirShadowCascadesMax);

    HRESULT hr;
    if (ShadowSettings.FilterType == EShadowFilterType::SFT_None || ShadowSettings.FilterType == EShadowFilterType::SFT_PCF)
    {
        // --- (PCF) Texture2DArray ---
        D3D11_TEXTURE2D_DESC DirTexDesc = {};
        DirTexDesc.Width = ShadowSettings.DirResolution;
        DirTexDesc.Height = ShadowSettings.DirResolution;
        DirTexDesc.MipLevels = 1;
        DirTexDesc.ArraySize = DirShadowCascadesMax;
        DirTexDesc.Format = DXGI_FORMAT_R32_TYPELESS; // PCF용
        DirTexDesc.SampleDesc.Count = 1;
        DirTexDesc.Usage = D3D11_USAGE_DEFAULT;
        DirTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL; // PCF용
        DirTexDesc.CPUAccessFlags = 0;
        DirTexDesc.MiscFlags = 0;

        hr = Device->CreateTexture2D(&DirTexDesc, nullptr, &DirShadowTexture);
        if (FAILED(hr))
        {
            UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE TEXTURE");
            return;
        }

        StatData.VRAM_Directional_Depth = static_cast<uint64_t>(ShadowSettings.DirResolution) * ShadowSettings.DirResolution * DirShadowCascadesMax * 4;

        // --- (PCF) SRV ---
        D3D11_SHADER_RESOURCE_VIEW_DESC DirSrvDesc = {};
        DirSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        DirSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        DirSrvDesc.Texture2DArray.MipLevels = 1;
    	DirSrvDesc.Texture2DArray.ArraySize = DirShadowCascadesMax;
    	DirSrvDesc.Texture2DArray.MostDetailedMip = 0;
    	DirSrvDesc.Texture2DArray.FirstArraySlice = 0;

        hr = Device->CreateShaderResourceView(DirShadowTexture, &DirSrvDesc, &DirShadowSRV);
        if (FAILED(hr))
        {
            UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE SRV");
            return;
        }

        // --- (PCF) DSV ---
        D3D11_DEPTH_STENCIL_VIEW_DESC DirDsvDesc = {};
        DirDsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        DirDsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        DirDsvDesc.Texture2DArray.MipSlice = 0;

        for (uint32 i = 0; i < DirShadowCascadesMax; ++i)
        {
            DirDsvDesc.Texture2DArray.FirstArraySlice = i;
            DirDsvDesc.Texture2DArray.ArraySize = 1;

            hr = Device->CreateDepthStencilView(DirShadowTexture, &DirDsvDesc, &DirShadowCascadeDSVs[i]);
            if (FAILED(hr))
            {
                DirShadowCascadeDSVs.clear();
                UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE DSV");
                return;
            }
        }

    	// --- 섀도우 패스 전용 공용 DSV ---
    	D3D11_TEXTURE2D_DESC DepthTexDesc = {};
    	DepthTexDesc.Width = ShadowSettings.DirResolution;
    	DepthTexDesc.Height = ShadowSettings.DirResolution;
    	DepthTexDesc.MipLevels = 1;
    	DepthTexDesc.ArraySize = 1;
    	DepthTexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    	DepthTexDesc.SampleDesc.Count = 1;
    	DepthTexDesc.Usage = D3D11_USAGE_DEFAULT;
    	DepthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    	DepthTexDesc.CPUAccessFlags = 0;
    	DepthTexDesc.MiscFlags = 0;

    	hr = Device->CreateTexture2D(&DepthTexDesc, nullptr, &DirShadowDepthTexture);
    	if (FAILED(hr))
    	{
    		UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE SHADOW DSV TEXTURE");
    		return;
    	}
    	StatData.VRAM_Directional_Pass_DSV = static_cast<uint64>(ShadowSettings.DirResolution) * ShadowSettings.DirResolution * 1 * 4;

    	D3D11_DEPTH_STENCIL_VIEW_DESC DirDepthDSVDesc = {};
    	DirDepthDSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
    	DirDepthDSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    	DirDepthDSVDesc.Texture2D.MipSlice = 0;

    	hr = Device->CreateDepthStencilView(DirShadowDepthTexture, &DirDepthDSVDesc, &DirShadowDepthDSV);
    	if (FAILED(hr))
    	{
    		UE_LOG_ERROR("[ShadowMapManager] FAILED TO CREATE SHADOW DSV");
    		return;
    	}
    }
    else // VSM 계열
    {
        // --- (VSM) Moments Texture2DArray ---
        D3D11_TEXTURE2D_DESC MomentsDesc = {};
        MomentsDesc.Width = ShadowSettings.DirResolution;
        MomentsDesc.Height = ShadowSettings.DirResolution;
        MomentsDesc.MipLevels = 1;
        MomentsDesc.ArraySize = DirShadowCascadesMax;
        MomentsDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        MomentsDesc.SampleDesc.Count = 1;
        MomentsDesc.Usage = D3D11_USAGE_DEFAULT;
        MomentsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        MomentsDesc.CPUAccessFlags = 0;
        MomentsDesc.MiscFlags = 0;

        hr = Device->CreateTexture2D(&MomentsDesc, nullptr, &DirShadowTexture);
        if (FAILED(hr))
        {
           return;
        }
        StatData.VRAM_Spot_Moments = static_cast<uint64>(ShadowSettings.DirResolution) * ShadowSettings.DirResolution * DirShadowCascadesMax * 8;

        // --- (VSM) SRV ---
        D3D11_SHADER_RESOURCE_VIEW_DESC MomentsSRVDesc = {};
        MomentsSRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        MomentsSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        MomentsSRVDesc.Texture2DArray.MipLevels = 1;
        MomentsSRVDesc.Texture2DArray.ArraySize = DirShadowCascadesMax;
    	MomentsSRVDesc.Texture2DArray.FirstArraySlice = 0;
    	MomentsSRVDesc.Texture2DArray.MostDetailedMip = 0;

        hr = Device->CreateShaderResourceView(DirShadowTexture, &MomentsSRVDesc, &DirShadowSRV);
        if (FAILED(hr))
        {
           return;
        }

        // --- (VSM) RTVs ---
        D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
        RTVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        RTVDesc.Texture2DArray.MipSlice = 0;
        for (uint32 i = 0; i < DirShadowCascadesMax; ++i)
        {
           RTVDesc.Texture2DArray.FirstArraySlice = i;
           RTVDesc.Texture2DArray.ArraySize = 1;

           hr = Device->CreateRenderTargetView(DirShadowTexture, &RTVDesc, &DirShadowMomentRTVs[i]);
           if (FAILED(hr))
           {
              DirShadowMomentRTVs.clear();
           }
        }
    }

    UpdateTotalVRAMStats();
}

void FShadowMapManager::ReleaseSpotShadows()
{
	SafeRelease(SpotShadowMapArrayTexture);
	SafeRelease(SpotShadowMapArraySRV);
	for (uint32 Idx = 0; Idx < ShadowSettings.MaxSpotShadows; ++Idx)
	{
		SafeRelease(SpotShadowMapSliceDSVs[Idx]);
	}
	for (uint32 Idx = 0; Idx < ShadowSettings.MaxSpotShadows; ++Idx)
	{
		SafeRelease(SpotShadowMomentsSliceRTVs[Idx]);
	}
}

void FShadowMapManager::ReleasePointShadows()
{
	SafeRelease(PointShadowCubeArrayTexture);
	SafeRelease(PointShadowCubeArraySRV);
	for (uint32 Idx = 0; Idx < ShadowSettings.MaxPointShadows * 6; ++Idx)
	{
		SafeRelease(PointShadowCubeSliceRTVs[Idx]);
	}
	SafeRelease(PointShadowDepthTexture);
	SafeRelease(PointShadowDepthDSV);
}

void FShadowMapManager::ReleaseDirectionalShadow()
{
	SafeRelease(DirShadowSRV);
	SafeRelease(DirShadowTexture);
	for (uint32 Idx = 0; Idx < DirShadowMomentRTVs.size(); ++Idx)
	{
		SafeRelease(DirShadowMomentRTVs[Idx]);
	}
	for (uint32 Idx = 0; Idx < DirShadowCascadeDSVs.size(); ++Idx)
	{
		SafeRelease(DirShadowCascadeDSVs[Idx]);
	}

	for (auto* Tex : ImGuiDebugTextures_Dir) { SafeRelease(Tex); }
	ImGuiDebugTextures_Dir.clear();
	for (auto* SRV : ImGuiDebugSRVs_Dir) { SafeRelease(SRV); }
	ImGuiDebugSRVs_Dir.clear();

	for (auto* RTV : DirShadowMomentRTVs) { SafeRelease(RTV); }
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

	//-- RTV Clear --//
	for (uint32 Idx = 0; Idx < CurrentSpotShadowIdx; ++Idx)
	{
		if (!SpotShadowMomentsSliceRTVs[Idx]) { continue; }
		Context->ClearRenderTargetView(SpotShadowMomentsSliceRTVs[Idx], ClearColor);
	}

	for (uint32 Idx = 0; Idx < CurrentPointCubeIdx * 6; ++Idx)
	{
		if (!PointShadowCubeSliceRTVs[Idx]) { continue; }
		Context->ClearRenderTargetView(PointShadowCubeSliceRTVs[Idx], ClearColor);
	}
	for (uint32 Idx = 0; Idx < DirShadowCascadesMax; ++Idx)
	{
		if (Idx < DirShadowMomentRTVs.size() && DirShadowMomentRTVs[Idx])
		{
			Context->ClearRenderTargetView(DirShadowMomentRTVs[Idx], ClearColor);
		}
	}

	//-- DSV Clear --//
	for (uint32 Idx = 0; Idx < DirShadowCascadesMax; ++Idx)
	{
		if (Idx < DirShadowCascadeDSVs.size() && DirShadowCascadeDSVs[Idx])
		{
			Context->ClearDepthStencilView(DirShadowCascadeDSVs[Idx], D3D11_CLEAR_DEPTH, 1.0f, 0);
		}
	}
	if (DirShadowDepthDSV) { Context->ClearDepthStencilView(DirShadowDepthDSV, D3D11_CLEAR_DEPTH, 1.0f, 0); }

	if (PointShadowDepthDSV) { Context->ClearDepthStencilView(PointShadowDepthDSV, D3D11_CLEAR_DEPTH, 1.0f, 0); }

	for (uint32 Idx = 0; Idx < CurrentSpotShadowIdx; ++Idx)
	{
		if (!SpotShadowMapSliceDSVs[Idx]) { continue; }
		Context->ClearDepthStencilView(SpotShadowMapSliceDSVs[Idx], D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

	CurrentSpotShadowIdx = 0;
	CurrentPointCubeIdx = 0;

	bIsDirShadowAllocated = false;
}

void FShadowMapManager::AllocateShadowMap(class ULightComponentBase* Light)
{
	switch (Light->GetLightType())
	{
	case ELightComponentType::LightType_Spot:
		if (CurrentSpotShadowIdx <= ShadowSettings.MaxSpotShadows)
		{
			Light->SetShadowMapIdx(CurrentSpotShadowIdx++);
			return;
		}
		break;
	case ELightComponentType::LightType_Point:
		if (CurrentPointCubeIdx <= ShadowSettings.MaxPointShadows)
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
		return ShadowSettings.SpotResolution * Light->GetShadowResolutionScale();
	case ELightComponentType::LightType_Point:
		return ShadowSettings.PointResolution * Light->GetShadowResolutionScale();
	case ELightComponentType::LightType_Directional:
		return ShadowSettings.DirResolution * Light->GetShadowResolutionScale();
	default:
		break;
	}
	return 0;
}

uint32 FShadowMapManager::GetShadowPassCount(ULightComponentBase* Light) const
{
	if (!Light || Light->GetShadowMapIdx() < 0)
	{
		return 0;
	}

	switch (Light->GetLightType())
	{
	case ELightComponentType::LightType_Point:
		return 6;
	case ELightComponentType::LightType_Spot:
		return 1;
	case ELightComponentType::LightType_Directional:
		if (Light->GetShadowProjectionMode() == EShadowProjectionMode::CSM)
		{
			return DirShadowCascadesMax;
		}
		return 1;
	default:
		return 0;
	}
}

void FShadowMapManager::GetShadowPassViews(ULightComponentBase* Light, uint32 PassIdx, ID3D11RenderTargetView** OutRTV, ID3D11DepthStencilView** OutDSV)
{
	if (!OutRTV || !OutDSV) { return; }

	*OutRTV = nullptr;
	*OutDSV = nullptr;

	int32 ShadowMapIdx = Light->GetShadowMapIdx();
	if (ShadowMapIdx < 0) { return; }

	switch (Light->GetLightType())
	{
	case ELightComponentType::LightType_Point:
		*OutDSV = GetPointLightDSV();
		*OutRTV = GetPointLightRTV(ShadowMapIdx, PassIdx);
		break;

	case ELightComponentType::LightType_Spot:
		*OutRTV = GetSpotLightRTV(ShadowMapIdx); // (VSM용)
		*OutDSV = GetSpotLightDSV(ShadowMapIdx); // (PCF용)
		break;

	case ELightComponentType::LightType_Directional:
		*OutRTV = GetDirectionalLightRTV(PassIdx); // (VSM용)
		*OutDSV = GetDirectionalLightDSV(PassIdx); // (PCF용)
		break;
	}
}

ID3D11ShaderResourceView* FShadowMapManager::GetSpotLightSRV() const
{
	return SpotShadowMapArraySRV;
}

ID3D11DepthStencilView* FShadowMapManager::GetSpotLightDSV(uint32 SpotShadowIdx) const
{
	if (SpotShadowIdx < 0 || SpotShadowIdx >= ShadowSettings.MaxSpotShadows)
	{
		return nullptr;
	}

	if (ShadowSettings.FilterType == EShadowFilterType::SFT_None || ShadowSettings.FilterType == EShadowFilterType::SFT_PCF)
	{
		return SpotShadowMapSliceDSVs[SpotShadowIdx];
	}
	return nullptr;
}

ID3D11RenderTargetView* FShadowMapManager::GetSpotLightRTV(uint32 SpotShadowIdx) const
{
	if (SpotShadowIdx < 0 || SpotShadowIdx >= ShadowSettings.MaxSpotShadows)
	{
		return nullptr;
	}

	if (ShadowSettings.FilterType == EShadowFilterType::SFT_None || ShadowSettings.FilterType == EShadowFilterType::SFT_PCF)
	{
		return nullptr;
	}
	return SpotShadowMomentsSliceRTVs[SpotShadowIdx];
}


ID3D11ShaderResourceView* FShadowMapManager::GetPointLightSRV() const
{
	return PointShadowCubeArraySRV;
}

ID3D11DepthStencilView* FShadowMapManager::GetPointLightDSV() const
{
	return PointShadowDepthDSV;
}

ID3D11RenderTargetView* FShadowMapManager::GetPointLightRTV(uint32 PointShadowIdx, uint32 PointSliceIdx) const
{
	if (PointShadowIdx < 0 || PointShadowIdx >ShadowSettings.MaxPointShadows || PointSliceIdx < 0 || PointSliceIdx >= 6) { return nullptr; }
	return PointShadowCubeSliceRTVs[PointShadowIdx * 6 + PointSliceIdx];
}

void FShadowMapManager::UpdateShadowSettings(const FShadowSettings& InSettings)
{
	bool bReinitialize = false;
	if (InSettings.FilterType != ShadowSettings.FilterType ||
		InSettings.MaxSpotShadows != ShadowSettings.MaxSpotShadows ||
		InSettings.SpotResolution != ShadowSettings.SpotResolution ||
		InSettings.MaxPointShadows != ShadowSettings.MaxPointShadows ||
		InSettings.PointResolution != ShadowSettings.PointResolution ||
		InSettings.DirResolution != ShadowSettings.DirResolution)
	{
		bReinitialize = true;
	}

	ShadowSettings = InSettings;

	if (bReinitialize)
	{
		Initialize(ShadowSettings.FilterType, ShadowSettings.MaxSpotShadows, ShadowSettings.SpotResolution,
				   ShadowSettings.MaxPointShadows, ShadowSettings.PointResolution, ShadowSettings.DirResolution);
	}
}

void FShadowMapManager::UpdateFilterType(const EShadowFilterType& InFilterType)
{
	if (InFilterType == ShadowSettings.FilterType) { return; }
	ShadowSettings.FilterType = InFilterType;
	Initialize(ShadowSettings.FilterType, ShadowSettings.MaxSpotShadows, ShadowSettings.SpotResolution,
		ShadowSettings.MaxPointShadows, ShadowSettings.PointResolution, ShadowSettings.DirResolution);
}

ID3D11ShaderResourceView* FShadowMapManager::GetDirectionalLightSRV() const
{
	return DirShadowSRV;
}

ID3D11DepthStencilView* FShadowMapManager::GetDirectionalLightDSV(uint32 CascadeIdx) const
{
	if (CascadeIdx >= DirShadowCascadesMax)
	{
		return nullptr;
	}

	if (ShadowSettings.FilterType == EShadowFilterType::SFT_None || ShadowSettings.FilterType == EShadowFilterType::SFT_PCF)
	{
		return DirShadowCascadeDSVs[CascadeIdx];
	}
	return DirShadowDepthDSV; // 공용 DSV
}

ID3D11RenderTargetView* FShadowMapManager::GetDirectionalLightRTV(uint32 CascadeIdx) const
{
	if (CascadeIdx >= DirShadowCascadesMax)
	{
		return nullptr;
	}

	if (ShadowSettings.FilterType == EShadowFilterType::SFT_None || ShadowSettings.FilterType == EShadowFilterType::SFT_PCF)
	{
		return nullptr;
	}
	return DirShadowMomentRTVs[CascadeIdx];
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

		ImGuiDebugTextures_Dir.resize(DirShadowCascadesMax, nullptr);
		ImGuiDebugSRVs_Dir.resize(DirShadowCascadesMax, nullptr);

		D3D11_TEXTURE2D_DESC Desc = {};
		Desc.Width = srcDesc.Width;
		Desc.Height = srcDesc.Height;
		Desc.MipLevels = 1;
		Desc.ArraySize = 1;
		Desc.Format = DXGI_FORMAT_R32_FLOAT; // SRV 읽기용
		Desc.SampleDesc.Count = 1;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		for (uint32 i = 0; i < DirShadowCascadesMax; ++i)
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
    if (SpotIndex >= ShadowSettings.MaxSpotShadows) { return nullptr; }

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
	if (!Context || !PointShadowCubeArrayTexture || CubeIndex >= ShadowSettings.MaxPointShadows)
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


ID3D11ShaderResourceView* FShadowMapManager::GetDirectionalSRVForImGuiDebug(uint32 CascadeIdx)
{
	if (!Context || CascadeIdx >= DirShadowCascadesMax) return nullptr;

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
			StatData.VRAM_Point_RTV +
			StatData.VRAM_Point_Moments +
			StatData.VRAM_Point_Pass_DSV;
}
