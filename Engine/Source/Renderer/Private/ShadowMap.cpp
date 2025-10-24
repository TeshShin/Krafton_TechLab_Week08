#include "pch.h"
#include "Renderer/Public/ShadowMap.h"
#include "Renderer/Public/Renderer.h"

FShadowMap::FShadowMap()
{
}

FShadowMap::~FShadowMap()
{
	Release();
}

bool FShadowMap::Initialize(uint32 InWidth, uint32 InHeight)
{
	Release();

	ID3D11Device* Device = URenderer::GetInstance().GetDevice();
    Width = InWidth;
    Height = InHeight;

    HRESULT Result;

    // --- 텍스처 생성 ---
    D3D11_TEXTURE2D_DESC TexDesc = {};
    TexDesc.Width = Width;
    TexDesc.Height = Height;
    TexDesc.MipLevels = 1;
    TexDesc.ArraySize = 1;
    TexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    TexDesc.SampleDesc.Count = 1;
    TexDesc.SampleDesc.Quality = 0;
    TexDesc.Usage = D3D11_USAGE_DEFAULT;
    TexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    TexDesc.CPUAccessFlags = 0;
    TexDesc.MiscFlags = 0;

    Result = Device->CreateTexture2D(&TexDesc, nullptr, &ShadowMapTexture);
    if (FAILED(Result)) {
        UE_LOG_ERROR("FShadowMap: 텍스처 생성 실패");
        Release();
        return false;
    }

    // --- DSV 생성 ---
    D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
    DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
    DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    DSVDesc.Texture2D.MipSlice = 0;

    Result = Device->CreateDepthStencilView(ShadowMapTexture, &DSVDesc, &ShadowMapDSV);
    if (FAILED(Result)) {
        UE_LOG_ERROR("FShadowMap: DSV 생성 실패");
        Release();
        return false;
    }

    // --- SRV 생성 ---
    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    SRVDesc.Texture2D.MipLevels = 1;

    Result = Device->CreateShaderResourceView(ShadowMapTexture, &SRVDesc, &ShadowMapSRV);
    if (FAILED(Result)) {
        UE_LOG_ERROR("FShadowMap: SRV 생성 실패");
        Release();
        return false;
    }

    // --- 뷰포트 설정 ---
    Viewport.Width = static_cast<float>(Width);
    Viewport.Height = static_cast<float>(Height);
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;
    Viewport.TopLeftX = 0.0f;
    Viewport.TopLeftY = 0.0f;

    // --- 비교 샘플러 생성 (PCF용) ---
    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.BorderColor[0] = 1.0f; // 맵 밖은 그림자가 아님(1.0)
    SamplerDesc.BorderColor[1] = 1.0f;
    SamplerDesc.BorderColor[2] = 1.0f;
    SamplerDesc.BorderColor[3] = 1.0f;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL; // 픽셀 깊이 <= 맵 깊이
    SamplerDesc.MinLOD = 0;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    Result = Device->CreateSamplerState(&SamplerDesc, &ComparisonSampler);
    if (FAILED(Result)) {
        UE_LOG_ERROR("FShadowMap: 비교 샘플러 생성 실패");
        Release();
        return false;
    }

    return true;
}

bool FShadowMap::Resize(uint32 NewWidth, uint32 NewHeight)
{
	if (Width == NewWidth && Height == NewHeight) { return true; }
	if (NewWidth == 0 || NewHeight == 0) { return false; }
	Release();

	return Initialize(NewWidth, NewHeight);
}

void FShadowMap::Release()
{
	SafeRelease(ComparisonSampler);
	SafeRelease(ShadowMapSRV);
	SafeRelease(ShadowMapDSV);
	SafeRelease(ShadowMapTexture);

	Width = 0;
	Height = 0;
	Viewport = {};
}
