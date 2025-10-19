#include "pch.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Renderer/Public/Renderer.h"
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

void FRenderResourceFactory::CreateVertexShaderAndInputLayout(const wstring& InFilePath,
                                                              const TArray<D3D11_INPUT_ELEMENT_DESC>& InInputLayoutDescs, ID3D11VertexShader** OutVertexShader, ID3D11InputLayout** OutInputLayout, const D3D_SHADER_MACRO* InDefines)
{
	ID3DBlob* VertexShaderBlob = nullptr;
	ID3DBlob* ErrorBlob = nullptr;

	UINT Flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
	Flags |= D3DCOMPILE_DEBUG;
#endif

	HRESULT Result = D3DCompileFromFile(InFilePath.data(), InDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainVS", "vs_5_0", Flags, 0, &VertexShaderBlob, &ErrorBlob);
	if (FAILED(Result))
	{
		if (ErrorBlob) { OutputDebugStringA(static_cast<char*>(ErrorBlob->GetBufferPointer())); SafeRelease(ErrorBlob); }
		SafeRelease(VertexShaderBlob);
		return;
	}

	URenderer::GetInstance().GetDevice()->CreateVertexShader(VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), nullptr, OutVertexShader);
	if (InInputLayoutDescs.size() > 0)
		URenderer::GetInstance().GetDevice()->CreateInputLayout(InInputLayoutDescs.data(), static_cast<uint32>(InInputLayoutDescs.size()), VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), OutInputLayout);

	SafeRelease(VertexShaderBlob);
}

ID3D11Buffer* FRenderResourceFactory::CreateIndexBuffer(const void* InIndices, uint32 InByteWidth)
{
	D3D11_BUFFER_DESC Desc = { InByteWidth, D3D11_USAGE_IMMUTABLE, D3D11_BIND_INDEX_BUFFER, 0, 0, 0 };
	D3D11_SUBRESOURCE_DATA InitData = { InIndices, 0, 0 };
	ID3D11Buffer* IndexBuffer = nullptr;
	URenderer::GetInstance().GetDevice()->CreateBuffer(&Desc, &InitData, &IndexBuffer);
	return IndexBuffer;
}

void FRenderResourceFactory::CreatePixelShader(const wstring& InFilePath, ID3D11PixelShader** OutPixelShader, const D3D_SHADER_MACRO* InDefines)
{
	ID3DBlob* PixelShaderBlob = nullptr;
	ID3DBlob* ErrorBlob = nullptr;

	UINT Flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
	Flags |= D3DCOMPILE_DEBUG;
#endif

	HRESULT Result = D3DCompileFromFile(InFilePath.data(), InDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainPS", "ps_5_0", Flags, 0, &PixelShaderBlob, &ErrorBlob);
	if (FAILED(Result))
	{
		if (ErrorBlob) { OutputDebugStringA(static_cast<char*>(ErrorBlob->GetBufferPointer())); SafeRelease(ErrorBlob); }
		SafeRelease(PixelShaderBlob);
		return;
	}

	URenderer::GetInstance().GetDevice()->CreatePixelShader(PixelShaderBlob->GetBufferPointer(), PixelShaderBlob->GetBufferSize(), nullptr, OutPixelShader);
	SafeRelease(PixelShaderBlob);
}

ID3D11SamplerState* FRenderResourceFactory::CreateSamplerState(D3D11_FILTER InFilter, D3D11_TEXTURE_ADDRESS_MODE InAddressMode)
{
	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter = InFilter;
	SamplerDesc.AddressU = InAddressMode;
	SamplerDesc.AddressV = InAddressMode;
	SamplerDesc.AddressW = InAddressMode;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamplerDesc.MinLOD = 0;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	ID3D11SamplerState* SamplerState = nullptr;
	if (FAILED(URenderer::GetInstance().GetDevice()->CreateSamplerState(&SamplerDesc, &SamplerState)))
	{
		UE_LOG_ERROR("Renderer: 샘플러 스테이트 생성 실패");
		return nullptr;
	}
	return SamplerState;
}

ID3D11RasterizerState* FRenderResourceFactory::GetRasterizerState(const FRenderState& InRenderState)
{
	const FRasterKey Key{ ToD3D11(InRenderState.FillMode), ToD3D11(InRenderState.CullMode) };
	if (auto Iter = RasterCache.find(Key); Iter != RasterCache.end())
	{
		return Iter->second;
	}

	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = Key.FillMode;
	RasterizerDesc.CullMode = Key.CullMode;
	RasterizerDesc.FrontCounterClockwise = TRUE;
	RasterizerDesc.DepthClipEnable = TRUE;

	ID3D11RasterizerState* RasterizerState = nullptr;
	if (FAILED(URenderer::GetInstance().GetDevice()->CreateRasterizerState(&RasterizerDesc, &RasterizerState)))
	{
		return nullptr;
	}

	RasterCache.emplace(Key, RasterizerState);
	return RasterizerState;
}

void FRenderResourceFactory::ReleaseRasterizerState()
{
	for (auto& Cache : RasterCache)
	{
		SafeRelease(Cache.second);
	}
	RasterCache.clear();
}

D3D11_CULL_MODE FRenderResourceFactory::ToD3D11(ECullMode InCull)
{
	switch (InCull)
	{
	case ECullMode::Back: return D3D11_CULL_BACK;
	case ECullMode::Front: return D3D11_CULL_FRONT;
	case ECullMode::None: return D3D11_CULL_NONE;
	default: return D3D11_CULL_BACK;
	}
}

D3D11_FILL_MODE FRenderResourceFactory::ToD3D11(EFillMode InFill)
{
	switch (InFill)
	{
	case EFillMode::Solid: return D3D11_FILL_SOLID;
	case EFillMode::WireFrame: return D3D11_FILL_WIREFRAME;
	default: return D3D11_FILL_SOLID;
	}
}

ID3D11ComputeShader* FRenderResourceFactory::CreateComputeShader(
    const std::wstring& InFilePath,
    const D3D_SHADER_MACRO* InDefines,
    const char* Entry,
    const char* Profile)
{
    ID3D11Device* Device = URenderer::GetInstance().GetDevice();
    ID3DBlob* ShaderBlob = nullptr;
    ID3DBlob* ErrorBlob  = nullptr;

    UINT Flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    Flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(
        InFilePath.c_str(),
        InDefines,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        Entry,
        Profile,
        Flags,
        0,
        &ShaderBlob,
        &ErrorBlob);

    if (FAILED(hr))
    {
        if (ErrorBlob)
        {
            UE_LOG_ERROR("Renderer: CS compile error for %ls\n%s",
                InFilePath.c_str(),
                (const char*)ErrorBlob->GetBufferPointer());
            ErrorBlob->Release();
        }
        SafeRelease(ShaderBlob);
        return nullptr;
    }

    ID3D11ComputeShader* CS = nullptr;
    hr = Device->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &CS);
    SafeRelease(ShaderBlob);

    if (FAILED(hr))
    {
        UE_LOG_ERROR("Renderer: CreateComputeShader failed for %ls", InFilePath.c_str());
        return nullptr;
    }
    return CS;
}

// Structured buffer (SRV + UAV). Use DEFAULT (no CPU access) for UAV use.
ID3D11Buffer* FRenderResourceFactory::CreateStructuredBufferWithUAV(
    uint32 ElementSize,
    uint32 ElementCount,
    ID3D11ShaderResourceView** OutSRV,
    ID3D11UnorderedAccessView** OutUAV)
{
    if (OutSRV) *OutSRV = nullptr;
    if (OutUAV) *OutUAV = nullptr;

    if (ElementSize == 0 || (ElementSize % 4) != 0 || ElementCount == 0)
    {
        UE_LOG_ERROR("Renderer: CreateStructuredBufferWithUAV invalid args (size=%u, count=%u)", ElementSize, ElementCount);
        return nullptr;
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = ElementSize * ElementCount;
    Desc.Usage = D3D11_USAGE_DEFAULT; // IMPORTANT: UAVs should not be DYNAMIC
    Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    Desc.CPUAccessFlags = 0;
    Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    Desc.StructureByteStride = ElementSize;

    ID3D11Buffer* Buffer = nullptr;
    HRESULT hr = URenderer::GetInstance().GetDevice()->CreateBuffer(&Desc, nullptr, &Buffer);
    if (FAILED(hr))
    {
        UE_LOG_ERROR("Renderer: CreateStructuredBufferWithUAV CreateBuffer failed");
        return nullptr;
    }

    if (OutSRV)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        SRVDesc.BufferEx.FirstElement = 0;
        SRVDesc.BufferEx.NumElements = ElementCount;
        hr = URenderer::GetInstance().GetDevice()->CreateShaderResourceView(Buffer, &SRVDesc, OutSRV);
        if (FAILED(hr))
        {
            UE_LOG_ERROR("Renderer: CreateStructuredBufferWithUAV SRV failed");
            SafeRelease(Buffer);
            return nullptr;
        }
    }

    if (OutUAV)
    {
        *OutUAV = CreateBufferUAV(Buffer, ElementCount, DXGI_FORMAT_UNKNOWN);
        if (!*OutUAV)
        {
            UE_LOG_ERROR("Renderer: CreateStructuredBufferWithUAV UAV failed");
            SafeRelease(*OutSRV);
            SafeRelease(Buffer);
            return nullptr;
        }
    }

    return Buffer;
}

ID3D11UnorderedAccessView* FRenderResourceFactory::CreateBufferUAV(
    ID3D11Buffer* Buffer,
    uint32 NumElements,
    DXGI_FORMAT Format)
{
    if (!Buffer || NumElements == 0)
    {
        UE_LOG_ERROR("Renderer: CreateBufferUAV invalid args");
        return nullptr;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    UAVDesc.Format = Format; // DXGI_FORMAT_UNKNOWN for structured
    UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    UAVDesc.Buffer.FirstElement = 0;
    UAVDesc.Buffer.NumElements = NumElements;
    UAVDesc.Buffer.Flags = 0; // not RAW

    ID3D11UnorderedAccessView* UAV = nullptr;
    HRESULT hr = URenderer::GetInstance().GetDevice()->CreateUnorderedAccessView(Buffer, &UAVDesc, &UAV);
    if (FAILED(hr))
    {
        UE_LOG_ERROR("Renderer: CreateBufferUAV failed");
        return nullptr;
    }
    return UAV;
}

// ---------------------
// CS bind/dispatch utils
// ---------------------
void FRenderResourceFactory::CSSetSRV(uint32 Slot, ID3D11ShaderResourceView* SRV)
{
    URenderer::GetInstance().GetDeviceContext()->CSSetShaderResources(Slot, 1, &SRV);
}

void FRenderResourceFactory::CSSetUAV(uint32 Slot, ID3D11UnorderedAccessView* UAV, uint32 InitialCount)
{
    URenderer::GetInstance().GetDeviceContext()->CSSetUnorderedAccessViews(Slot, 1, &UAV, &InitialCount);
}

void FRenderResourceFactory::CSUnsetSRV(uint32 Slot, uint32 Count)
{
    ID3D11ShaderResourceView* NullSRV[8] = {};
    URenderer::GetInstance().GetDeviceContext()->CSSetShaderResources(Slot, Count, NullSRV);
}

void FRenderResourceFactory::CSUnsetUAV(uint32 Slot, uint32 Count)
{
    ID3D11UnorderedAccessView* NullUAV[8] = {};
    uint32 Counts[8] = { 0,0,0,0,0,0,0,0 };
    URenderer::GetInstance().GetDeviceContext()->CSSetUnorderedAccessViews(Slot, Count, NullUAV, Counts);
}

void FRenderResourceFactory::CSSetShader(ID3D11ComputeShader* CS)
{
    URenderer::GetInstance().GetDeviceContext()->CSSetShader(CS, nullptr, 0);
}

void FRenderResourceFactory::CSDispatch(uint32 GroupX, uint32 GroupY, uint32 GroupZ)
{
    URenderer::GetInstance().GetDeviceContext()->Dispatch(GroupX, GroupY, GroupZ);
}

TMap<FRenderResourceFactory::FRasterKey, ID3D11RasterizerState*, FRenderResourceFactory::FRasterKeyHasher> FRenderResourceFactory::RasterCache;
