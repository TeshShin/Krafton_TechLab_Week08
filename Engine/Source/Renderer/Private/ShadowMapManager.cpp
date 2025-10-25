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

void FShadowMapManager::Initialize(uint32 InMaxSpotShadows, uint32 InSpotResolution, uint32 InMaxPointShadowCubes, uint32 InPointResolution)
{
	Device = URenderer::GetInstance().GetDevice();
	Context = URenderer::GetInstance().GetDeviceContext();

	InitializeSpotShadows(InMaxSpotShadows, InSpotResolution);
	InitializePointShadows(InMaxPointShadowCubes, InPointResolution);

	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	SamplerDesc.BorderColor[0] = 1.0f;
	SamplerDesc.BorderColor[1] = 1.0f;
	SamplerDesc.BorderColor[2] = 1.0f;
	SamplerDesc.BorderColor[3] = 1.0f;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
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
    RTVTexDesc.Format = DXGI_FORMAT_R32_FLOAT;
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

    // --- SRV ---
    D3D11_SHADER_RESOURCE_VIEW_DESC PointSrvDesc = {};
    PointSrvDesc.Format = RTVTexDesc.Format;
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

    // --- RTV Array ---
    D3D11_RENDER_TARGET_VIEW_DESC PointRtvDesc = {};
    PointRtvDesc.Format = RTVTexDesc.Format;
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
}

void FShadowMapManager::ReleaseSpotShadows()
{
	SafeRelease(SpotShadowMapArrayTexture);
	SafeRelease(SpotShadowMapArraySRV);
	for (uint32 Idx = 0; Idx < MaxSpotShadows; ++Idx)
	{
		SafeRelease(SpotShadowMapSliceDSVs[Idx]);
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

void FShadowMapManager::Release()
{
	ReleaseSpotShadows();
	ReleasePointShadows();
}

void FShadowMapManager::ClearShadowMaps()
{
	for (uint32 Idx = 0; Idx < CurrentSpotShadowIdx; ++Idx)
	{
		Context->ClearDepthStencilView(SpotShadowMapSliceDSVs[Idx], D3D11_CLEAR_DEPTH, 1.0f, 0);
	}
	CurrentSpotShadowIdx = 0;

	constexpr float ClearColor[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
	for (uint32 Idx = 0; Idx < CurrentPointCubeIdx * 6; ++Idx)
	{
		Context->ClearRenderTargetView(PointShadowCubeSliceRTVs[Idx], ClearColor);
	}
	CurrentPointCubeIdx = 0;
	Context->ClearDepthStencilView(PointShadowDepthDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void FShadowMapManager::AllocateShadowMap(class ULightComponentBase* Light)
{
	switch (Light->GetLightType())
	{
	case ELightComponentType::LightType_Spot:
		if (CurrentSpotShadowIdx < MaxSpotShadows)
		{
			Light->SetShadowMapIdx(CurrentSpotShadowIdx++);
			return;
		}
		break;
	case ELightComponentType::LightType_Point:
		if (CurrentPointCubeIdx < MaxPointShadowCubes)
		{
			Light->SetShadowMapIdx(CurrentPointCubeIdx++);
			return;
		}
		break;
	case ELightComponentType::LightType_Directional:
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
		break;
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

	if (CurrentPointCubeIdx < MaxPointShadowCubes)
	{
		for (uint32 Idx = 0; Idx < 6; ++Idx)
		{
			OutRTVs.emplace_back(PointShadowCubeSliceRTVs[ShadowMapIdx * 6 + Idx]);
		}
	}
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

        hr = Device->CreateTexture2D(&Desc, nullptr, &ImGuiDebugTexture_Point);
        if (FAILED(hr))
        {
            UE_LOG_ERROR("[ShadowMapManager] Failed to create Point Debug Texture.");
            return;
        }

        // SRV 생성
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = Desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        hr = Device->CreateShaderResourceView(ImGuiDebugTexture_Point, &srvDesc, &ImGuiDebugSRV_Point);
        if (FAILED(hr))
        {
            UE_LOG_ERROR("[ShadowMapManager] Failed to create Point Debug SRV.");
            return;
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

ID3D11ShaderResourceView* FShadowMapManager::GetPointSRVForImGuiDebug(uint32 CubeIndex, uint32 FaceIndex)
{
    // 1. 유효성 검사
    if (!Context || !PointShadowCubeArrayTexture || !ImGuiDebugTexture_Point || !ImGuiDebugSRV_Point)
    {
        return nullptr;
    }
    if (CubeIndex >= MaxPointShadowCubes || FaceIndex >= 6)
    {
        return nullptr;
    }

    uint32 FinalSliceIndex = (CubeIndex * 6) + FaceIndex;
    UINT SrcSubresource = D3D11CalcSubresource(0, FinalSliceIndex, 1);
    UINT DstSubresource = 0;

    // 리소스 복사 (GPU 작업)
    Context->CopySubresourceRegion(
        ImGuiDebugTexture_Point, DstSubresource, 0, 0, 0,
        PointShadowCubeArrayTexture, SrcSubresource, nullptr
    );

    return ImGuiDebugSRV_Point;
}
