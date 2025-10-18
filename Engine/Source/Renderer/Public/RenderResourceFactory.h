#pragma once
#include "Renderer/Public/Renderer.h"

class FRenderResourceFactory
{
public:
	static void CreateVertexShaderAndInputLayout(const wstring& InFilePath,
		const TArray<D3D11_INPUT_ELEMENT_DESC>& InInputLayoutDescs, ID3D11VertexShader** OutVertexShader, ID3D11InputLayout** OutInputLayout, const D3D_SHADER_MACRO* InDefines = nullptr);
	static ID3D11Buffer* CreateVertexBuffer(FNormalVertex* InVertices, uint32 InByteWidth);
	static ID3D11Buffer* CreateVertexBuffer(FVector* InVertices, uint32 InByteWidth, bool bCpuAccess);
	static ID3D11Buffer* CreateIndexBuffer(const void* InIndices, uint32 InByteWidth);
	static void CreatePixelShader(const wstring& InFilePath, ID3D11PixelShader** OutPixelShader, const D3D_SHADER_MACRO* InDefines = nullptr);
	static ID3D11SamplerState* CreateSamplerState(D3D11_FILTER InFilter, D3D11_TEXTURE_ADDRESS_MODE InAddressMode);
	static ID3D11RasterizerState* GetRasterizerState(const FRenderState& InRenderState);
	static void ReleaseRasterizerState();

	// Helper function
	static D3D11_CULL_MODE ToD3D11(ECullMode InCull);
	static D3D11_FILL_MODE ToD3D11(EFillMode InFill);

	template<typename T>
	static ID3D11Buffer* CreateConstantBuffer()
	{
		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = sizeof(T) + 0xf & 0xfffffff0;
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		ID3D11Buffer* Buffer = nullptr;
		URenderer::GetInstance().GetDevice()->CreateBuffer(&Desc, nullptr, &Buffer);
		return Buffer;
	}

	template<typename T>
	static void UpdateConstantBufferData(ID3D11Buffer* Buffer, const T& Data)
	{
		D3D11_MAPPED_SUBRESOURCE MappedResource = {};
		URenderer::GetInstance().GetDeviceContext()->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
		memcpy(MappedResource.pData, &Data, sizeof(T));
		URenderer::GetInstance().GetDeviceContext()->Unmap(Buffer, 0);
	}

	/**
	 * @brief Create StructuredBuffer
	 * @param ElementCount Number of elements
	 * @return ID3D11Buffer* (StructuredBuffer)
	 */
	template<typename T>
	static ID3D11Buffer* CreateStructuredBuffer(uint32 ElementCount)
	{
		D3D11_BUFFER_DESC BufferDesc = {};
		uint32 ElementSize = (sizeof(T) + 0xf) & 0xfffffff0;
		BufferDesc.ByteWidth = ElementSize * ElementCount;
		BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		BufferDesc.StructureByteStride = ElementSize;

		ID3D11Buffer* Buffer = nullptr;
		HRESULT hr = URenderer::GetInstance().GetDevice()->CreateBuffer(&BufferDesc, nullptr, &Buffer);

		if (FAILED(hr))
		{
			UE_LOG_ERROR("Renderer: StructuredBuffer creation failed");
			return nullptr;
		}

		return Buffer;
	}

	/**
	 * @brief Create ShaderResourceView for StructuredBuffer
	 * @param Buffer StructuredBuffer
	 * @param ElementCount Number of elements
	 * @return ID3D11ShaderResourceView*
	 */
	static ID3D11ShaderResourceView* CreateBufferSRV(ID3D11Buffer* Buffer, uint32 ElementCount)
	{
		if (!Buffer)
		{
			UE_LOG_ERROR("Renderer: Buffer is nullptr");
			return nullptr;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		SRVDesc.Buffer.FirstElement = 0;
		SRVDesc.Buffer.NumElements = ElementCount;

		ID3D11ShaderResourceView* SRV = nullptr;
		HRESULT hr = URenderer::GetInstance().GetDevice()->CreateShaderResourceView(Buffer, &SRVDesc, &SRV);

		if (FAILED(hr))
		{
			UE_LOG_ERROR("Renderer: StructuredBuffer SRV creation failed");
			return nullptr;
		}

		return SRV;
	}

	/**
	 * @brief Update StructuredBuffer data (template version)
	 * @param Buffer Buffer to update
	 * @param Data Data array
	 */
	template<typename T>
	static void UpdateStructuredBufferData(ID3D11Buffer* Buffer, const TArray<T>& Data)
	{
		if (!Buffer || Data.empty()) return;

		D3D11_MAPPED_SUBRESOURCE MappedResource = {};
		HRESULT hr = URenderer::GetInstance().GetDeviceContext()->Map(
			Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);

		if (SUCCEEDED(hr))
		{
			memcpy(MappedResource.pData, Data.data(), sizeof(T) * Data.size());
			URenderer::GetInstance().GetDeviceContext()->Unmap(Buffer, 0);
		}
	}

	/**
	 * @brief Reallocate StructuredBuffer with new capacity
	 * @param Buffer Buffer to reallocate (will be released and recreated)
	 * @param SRV SRV to reallocate (will be released and recreated)
	 * @param NewCapacity New capacity (number of elements)
	 * @note This function releases old resources and creates new ones
	 * @note Use this when the current buffer capacity is insufficient
	 */
	template<typename T>
	static void ReallocateStructuredBuffer(
		ID3D11Buffer*& Buffer,
		ID3D11ShaderResourceView*& SRV,
		uint32 NewCapacity)
	{
		if (NewCapacity == 0)
		{
			UE_LOG_ERROR("Renderer: ReallocateStructuredBuffer - NewCapacity is 0");
			return;
		}

		// Release old resources (order matters: SRV first, then Buffer)
		SafeRelease(SRV);
		SafeRelease(Buffer);

		// Create new resources
		Buffer = CreateStructuredBuffer<T>(NewCapacity);
		if (!Buffer)
		{
			UE_LOG_ERROR("Renderer: ReallocateStructuredBuffer - Buffer creation failed");
			return;
		}

		SRV = CreateBufferSRV(Buffer, NewCapacity);
		if (!SRV)
		{
			UE_LOG_ERROR("Renderer: ReallocateStructuredBuffer - SRV creation failed");
			SafeRelease(Buffer);  // Cleanup buffer on SRV failure
			return;
		}
	}

	template<typename T>
	static void UpdateVertexBufferData(ID3D11Buffer* InVertexBuffer, const TArray<T>& InVertices)
	{
		if (!URenderer::GetInstance().GetDeviceContext() || !InVertexBuffer || InVertices.empty()) return;

		D3D11_MAPPED_SUBRESOURCE MappedResource = {};
		if (FAILED(URenderer::GetInstance().GetDeviceContext()->Map(InVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource))) return;

		memcpy(MappedResource.pData, InVertices.data(), sizeof(T) * InVertices.size());
		URenderer::GetInstance().GetDeviceContext()->Unmap(InVertexBuffer, 0);
	}

private:
	struct FRasterKey
	{
		D3D11_FILL_MODE FillMode = {};
		D3D11_CULL_MODE CullMode = {};

		bool operator==(const FRasterKey& InKey) const
		{
			return FillMode == InKey.FillMode && CullMode == InKey.CullMode;
		}
	};

	struct FRasterKeyHasher
	{
		size_t operator()(const FRasterKey& InKey) const noexcept
		{
			auto Mix = [](size_t& H, size_t V)
			{
				H ^= V + 0x9e3779b97f4a7c15ULL + (H << 6) + (H << 2);
			};

			size_t H = 0;
			Mix(H, (size_t)InKey.FillMode);
			Mix(H, (size_t)InKey.CullMode);

			return H;
		}
	};

	static TMap<FRasterKey, ID3D11RasterizerState*, FRasterKeyHasher> RasterCache;
};
