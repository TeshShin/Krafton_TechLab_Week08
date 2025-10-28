#pragma once
#include "Renderer/Public/Renderer.h"

class FRenderResourceFactory
{
public:
	/**
	 * @brief Compile a vertex shader from HLSL file and create input layout
	 *
	 * This function compiles a vertex shader using D3DCompileFromFile and creates
	 * the corresponding input layout for vertex attributes.
	 *
	 * @param InFilePath Path to .hlsl file (e.g., L"Asset/Shader/TextureVS.hlsl")
	 * @param InInputLayoutDescs Vertex input element descriptors (position, normal, texcoord, etc.)
	 * @param OutVertexShader Output pointer for compiled vertex shader
	 * @param OutInputLayout Output pointer for input layout (can be nullptr if not needed)
	 * @param InDefines Preprocessor macro array for conditional compilation (e.g., LIGHTING_MODEL_PHONG)
	 * @param bEnableHotReload If true, registers shader with ShaderManager for hot-reload support (default: true)
	 *
	 * @note Entry point: "mainVS", Profile: "vs_5_0"
	 * @note Compilation errors are output to debug console via OutputDebugStringA
	 * @note When bEnableHotReload=true, shader will automatically recompile when file changes
	 *
	 * TODO: Consider migrating to newer LoadVS() API for better readability (see ShaderManager)
	 * TODO: Add support for custom entry points and shader models
	 */
	static void CreateVertexShaderAndInputLayout(
		const wstring& InFilePath,
		const TArray<D3D11_INPUT_ELEMENT_DESC>& InInputLayoutDescs,
		ID3D11VertexShader** OutVertexShader,
		ID3D11InputLayout** OutInputLayout,
		const D3D_SHADER_MACRO* InDefines = nullptr,
		bool bEnableHotReload = true);

	template<typename T>
	static ID3D11Buffer* CreateVertexBuffer(const TArray<T>& InVertices, bool bCpuAccess = false)
	{
		if (InVertices.empty())
		{
			return nullptr;
		}

		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = static_cast<uint32>(InVertices.size() * sizeof(T));
		Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		if (bCpuAccess)
		{
			Desc.Usage = D3D11_USAGE_DYNAMIC;
			Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		}
		else
		{
			Desc.Usage = D3D11_USAGE_IMMUTABLE;
			Desc.CPUAccessFlags = 0;
		}

		D3D11_SUBRESOURCE_DATA InitData = { InVertices.data(), 0, 0 };
		ID3D11Buffer* VertexBuffer = nullptr;
		URenderer::GetInstance().GetDevice()->CreateBuffer(&Desc, &InitData, &VertexBuffer);
		return VertexBuffer;
	}

	static ID3D11Buffer* CreateIndexBuffer(const void* InIndices, uint32 InByteWidth);

	/**
	 * @brief Compile a pixel shader from HLSL file
	 *
	 * This function compiles a pixel shader using D3DCompileFromFile.
	 *
	 * @param InFilePath Path to .hlsl file (e.g., L"Asset/Shader/TexturePS.hlsl")
	 * @param OutPixelShader Output pointer for compiled pixel shader
	 * @param InDefines Preprocessor macro array for conditional compilation (can be nullptr)
	 * @param bEnableHotReload If true, registers shader with ShaderManager for hot-reload support (default: true)
	 *
	 * @note Entry point: "mainPS", Profile: "ps_5_0"
	 * @note Compilation errors are output to debug console via OutputDebugStringA
	 * @note When bEnableHotReload=true, shader will automatically recompile when file changes
	 *
	 * TODO: Consider migrating to newer LoadPS() API for better readability (see ShaderManager)
	 */
	static void CreatePixelShader(
		const wstring& InFilePath,
		ID3D11PixelShader** OutPixelShader,
		const D3D_SHADER_MACRO* InDefines = nullptr,
		bool bEnableHotReload = true);
	static ID3D11SamplerState* CreateSamplerState(D3D11_FILTER InFilter, D3D11_TEXTURE_ADDRESS_MODE InAddressMode);
	static ID3D11RasterizerState* GetRasterizerState(const FRenderState& InRenderState);
	static void Release();

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

	// ===============================
	// Compute Shader & UAV utilities
	// ===============================

	/**
	 * @brief Compile a compute shader from HLSL file
	 *
	 * This function compiles a compute shader using D3DCompileFromFile.
	 * Compute shaders are used for GPU parallelization (e.g., light culling, post-processing).
	 *
	 * @param InFilePath Path to .hlsl file (e.g., L"Asset/Shader/LightTilesComputeShader.hlsl")
	 * @param OutComputeShader Output pointer for compiled compute shader
	 * @param InDefines Preprocessor macro array for conditional compilation (can be nullptr)
	 * @param Entry Entry point function name (default: "main")
	 * @param Profile Shader model profile (default: "cs_5_0")
	 * @param bEnableHotReload If true, registers shader with ShaderManager for hot-reload support (default: true)
	 *
	 * @note Compilation errors are output to debug console
	 * @note When bEnableHotReload=true, shader will automatically recompile when file changes
	 * @note Signature changed from return value to output parameter for hot-reload consistency
	 *
	 * TODO: Consider migrating to newer LoadCS() API for better readability (see ShaderManager)
	 * TODO: Add support for cs_6_0+ features (wave intrinsics, etc.)
	 */
	static void CreateComputeShader(
		const std::wstring& InFilePath,
		ID3D11ComputeShader** OutComputeShader,
		const D3D_SHADER_MACRO* InDefines = nullptr,
		const char* Entry = "main",
		const char* Profile = "cs_5_0",
		bool bEnableHotReload = true);

	// Structured buffer with UAV (no CPU access; DEFAULT usage)
	static ID3D11Buffer* CreateStructuredBufferWithUAV(
		uint32 ElementSize,    // sizeof(T) (no padding; D3D requires multiple of 4)
		uint32 ElementCount,
		ID3D11ShaderResourceView** OutSRV,
		ID3D11UnorderedAccessView** OutUAV);

	// UAV creation for an existing structured buffer (DXGI_FORMAT_UNKNOWN)
	static ID3D11UnorderedAccessView* CreateBufferUAV(
		ID3D11Buffer* Buffer,
		uint32 NumElements,
		DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN);

	// ===============================

private:
	struct FRasterKey
	{
		D3D11_FILL_MODE FillMode = {};
		D3D11_CULL_MODE CullMode = {};
		int32 DepthBias = 0;
		float SlopeScaledDepthBias = 0.0f;
		float DepthBiasClamp = 0.0f;

		bool operator==(const FRasterKey& InKey) const
		{
			return FillMode == InKey.FillMode
				&& CullMode == InKey.CullMode
				&& DepthBias == InKey.DepthBias
				&& SlopeScaledDepthBias == InKey.SlopeScaledDepthBias
				&& DepthBiasClamp == InKey.DepthBiasClamp;
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
			Mix(H, (size_t)InKey.DepthBias);
			Mix(H, (size_t)InKey.SlopeScaledDepthBias);
			Mix(H, (size_t)InKey.DepthBiasClamp);

			return H;
		}
	};

	static TMap<FRasterKey, ID3D11RasterizerState*, FRasterKeyHasher> RasterCache;
};
