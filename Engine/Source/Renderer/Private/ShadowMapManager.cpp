#include "pch.h"
#include "Renderer/Public/ShadowMapManager.h"
#include "Renderer/Public/Renderer.h"
#include "Scene/Public/Component/LightComponentBase.h"

FShadowMapManager::FShadowMapManager()
{
}

FShadowMapManager::~FShadowMapManager()
{
}

FShadowMapManager& FShadowMapManager::GetInstance()
{
	static FShadowMapManager Instance;
	return Instance;
}

void FShadowMapManager::Initalize(uint32 InMaxShadows, uint32 InResolution)
{
	Release();
	ID3D11Device* Device = URenderer::GetInstance().GetDevice();

    Resolution = InResolution;
    MaxShadows = InMaxShadows;

    // --- Texture2DArray ---
    D3D11_TEXTURE2D_DESC TexDesc = {};
    TexDesc.Width = Resolution;
    TexDesc.Height = Resolution;
    TexDesc.MipLevels = 1;
    TexDesc.ArraySize = MaxShadows;
    TexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    TexDesc.SampleDesc.Count = 1;
    TexDesc.SampleDesc.Quality = 0;
    TexDesc.Usage = D3D11_USAGE_DEFAULT;
    TexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
    TexDesc.CPUAccessFlags = 0;
    TexDesc.MiscFlags = 0;

    HRESULT hr = Device->CreateTexture2D(&TexDesc, nullptr, &ShadowMapArrayTexture);
    if (FAILED(hr))
    {
	    return;
    }

	// --- SRV ---
    D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    SrvDesc.Texture2DArray.MostDetailedMip = 0;
    SrvDesc.Texture2DArray.MipLevels = 1;
    SrvDesc.Texture2DArray.FirstArraySlice = 0;
    SrvDesc.Texture2DArray.ArraySize = MaxShadows;

    hr = Device->CreateShaderResourceView(ShadowMapArrayTexture, &SrvDesc, &ShadowMapArraySRV);
    if (FAILED(hr))
    {
	    return;
    }

	// --- DSV ---
    D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
    DsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    DsvDesc.Texture2DArray.MipSlice = 0;
    ShadowMapSliceDSVs.resize(MaxShadows);

    for (uint32 i = 0; i < MaxShadows; ++i)
    {
        DsvDesc.Texture2DArray.FirstArraySlice = i;
        DsvDesc.Texture2DArray.ArraySize = 1;

        hr = Device->CreateDepthStencilView(ShadowMapArrayTexture, &DsvDesc, &ShadowMapSliceDSVs[i]);
        if (FAILED(hr))
        {
            ShadowMapSliceDSVs.clear();
            return;
        }
    }

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

	hr = Device->CreateSamplerState(&SamplerDesc, &ShadowMapSamplerState);
	if (FAILED(hr))
	{
		return;
	}

	InitializeForDebug(Device);
}

void FShadowMapManager::Release()
{
	SafeRelease(ShadowMapArrayTexture);
	SafeRelease(ShadowMapArraySRV);
	for (uint32 i = 0; i < MaxShadows; ++i)
	{
		SafeRelease(ShadowMapSliceDSVs[i]);
	}
}

void FShadowMapManager::ClearShadowMaps()
{
	ID3D11DeviceContext* Context = URenderer::GetInstance().GetDeviceContext();
	for (uint32 Idx = 0; Idx < CurrentShadowIdx; ++Idx)
	{
		Context->ClearDepthStencilView(GetDSV(Idx), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}
	CurrentShadowIdx = 0;
}

void FShadowMapManager::AllocateShadowMap(class ULightComponentBase* Light)
{
	if (CurrentShadowIdx < MaxShadows)
	{
		Light->SetShadowMapIdx(CurrentShadowIdx++);
		return;
	}

	Light->SetShadowMapIdx(-1);
}

ID3D11DepthStencilView* FShadowMapManager::GetDSV(uint32 ShadowMapIdx) const
{
	if (ShadowMapIdx < CurrentShadowIdx)
	{
		return ShadowMapSliceDSVs[ShadowMapIdx];
	}
	return nullptr;
}

void FShadowMapManager::InitializeForDebug(ID3D11Device* Device)
{
	// --- ImGui 디버그용 텍스처 생성 ---
	D3D11_TEXTURE2D_DESC DebugTexDesc = {};
	ShadowMapArrayTexture->GetDesc(&DebugTexDesc);
	DebugTexDesc.ArraySize = 1;
	DebugTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	DebugTexDesc.Usage = D3D11_USAGE_DEFAULT;
	DebugTexDesc.MiscFlags = 0;
	DebugTexDesc.Format = DXGI_FORMAT_R32_FLOAT;

	HRESULT hr = Device->CreateTexture2D(&DebugTexDesc, nullptr, &ImGuiDebugTexture);
	if (FAILED(hr))
	{
		return;
	}

	// --- ImGui 디버그용 SRV 생성 ---
	D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.Format = DebugTexDesc.Format;
	SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SrvDesc.Texture2D.MostDetailedMip = 0;
	SrvDesc.Texture2D.MipLevels = 1;

	hr = Device->CreateShaderResourceView(ImGuiDebugTexture, &SrvDesc, &ImGuiDebugSRV);
	if (FAILED(hr))
	{
		return;
	}
}

ID3D11ShaderResourceView* FShadowMapManager::GetSRVForImGuiDebug(uint32 ShadowMapIdx)
{
	ID3D11DeviceContext* Context = URenderer::GetInstance().GetDeviceContext();
	if (ShadowMapIdx >= MaxShadows || !Context || !ShadowMapArrayTexture || !ImGuiDebugTexture) { return nullptr; }

	UINT SrcMipLevels = 1;
	UINT SrcSubresource = D3D11CalcSubresource(0, ShadowMapIdx, SrcMipLevels);
	UINT DstSubresource = 0;

	Context->CopySubresourceRegion(
		ImGuiDebugTexture, DstSubresource, 0, 0, 0,
		ShadowMapArrayTexture, SrcSubresource, nullptr
	);

	return ImGuiDebugSRV;
}
