#pragma once
#include "Renderer/Public/ShaderCache.h"
#include "Renderer/Public/ShaderBinaryCache.h"
#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

/**
 * @brief Shared shader object pool (Flyweight Pattern)
 *
 * Manages compiled shader objects with reference counting to enable sharing
 * across multiple RenderPass instances.
 *
 * Key Concept:
 * - Multiple passes may use the same shader (e.g., all StaticMeshPass instances share VSPhong)
 * - Instead of compiling N times, compile once and share
 * - Reference counting ensures proper cleanup
 *
 * Example:
 *   Pass1->VSPhong ─┐
 *   Pass2->VSPhong ─┼─→ [Single ID3D11VertexShader object]
 *   Pass3->VSPhong ─┘     RefCount = 3
 */
class FShaderPool
{
public:
	FShaderPool();
	~FShaderPool();

	/**
	 * @brief Initialize pool and binary cache system
	 */
	void Initialize();

	/**
	 * @brief Release all cached shaders
	 */
	void Release();

	/**
	 * @brief Get or compile vertex shader
	 *
	 * If shader with this key already exists in pool, returns cached version and increments RefCount.
	 * Otherwise, compiles (or loads from cache) and adds to pool.
	 *
	 * @param Key Shader variant identifier
	 * @param OutInputLayout Optional output for input layout
	 * @param LayoutDescs Input element descriptors (required if OutInputLayout is not nullptr)
	 * @return Compiled vertex shader, or nullptr on failure
	 */
	ID3D11VertexShader* GetOrCompileVS(
		const FShaderKey& Key,
		ID3D11InputLayout** OutInputLayout = nullptr,
		const TArray<D3D11_INPUT_ELEMENT_DESC>* LayoutDescs = nullptr);

	/**
	 * @brief Get or compile pixel shader
	 *
	 * @param Key Shader variant identifier
	 * @return Compiled pixel shader, or nullptr on failure
	 */
	ID3D11PixelShader* GetOrCompilePS(const FShaderKey& Key);

	/**
	 * @brief Get or compile compute shader
	 *
	 * @param Key Shader variant identifier
	 * @param Entry Entry point (default: "main")
	 * @param Profile Shader model (default: "cs_5_0")
	 * @return Compiled compute shader, or nullptr on failure
	 */
	ID3D11ComputeShader* GetOrCompileCS(
		const FShaderKey& Key,
		const char* Entry = "main",
		const char* Profile = "cs_5_0");

	/**
	 * @brief Force recompile a shader and update all references
	 *
	 * Used for hot-reload. Recompiles from source, updates cache, and
	 * returns new shader object. Caller must update their pointer.
	 *
	 * @param Key Shader variant identifier
	 * @return New shader object, or nullptr on failure (old shader remains valid)
	 */
	void* RecompileShader(const FShaderKey& Key);

	/**
	 * @brief Get total number of cached shaders
	 */
	size_t GetCachedShaderCount() const;

	/**
	 * @brief Precompile all known shader variants at engine startup
	 *
	 * Scans shader directory, loads from cache if available, compiles if needed.
	 * This ensures faster load times after first run.
	 *
	 * @return Number of shaders precompiled
	 */
	int32 PrecompileAllShaders();

private:
	/**
	 * @brief Cached vertex shader entry
	 */
	struct FCachedVertexShader
	{
		ComPtr<ID3D11VertexShader> Shader;
		ComPtr<ID3D11InputLayout> InputLayout;
		FILETIME LastCompileTime;

		FCachedVertexShader()
		{
			LastCompileTime.dwLowDateTime = 0;
			LastCompileTime.dwHighDateTime = 0;
		}
	};

	/**
	 * @brief Cached pixel shader entry
	 */
	struct FCachedPixelShader
	{
		ComPtr<ID3D11PixelShader> Shader;
		FILETIME LastCompileTime;

		FCachedPixelShader()
		{
			LastCompileTime.dwLowDateTime = 0;
			LastCompileTime.dwHighDateTime = 0;
		}
	};

	/**
	 * @brief Cached compute shader entry
	 */
	struct FCachedComputeShader
	{
		ComPtr<ID3D11ComputeShader> Shader;
		FILETIME LastCompileTime;

		FCachedComputeShader()
		{
			LastCompileTime.dwLowDateTime = 0;
			LastCompileTime.dwHighDateTime = 0;
		}
	};

	TMap<FShaderKey, FCachedVertexShader, FShaderKeyHasher> VSCache;   ///< Vertex shader cache
	TMap<FShaderKey, FCachedPixelShader, FShaderKeyHasher> PSCache;    ///< Pixel shader cache
	TMap<FShaderKey, FCachedComputeShader, FShaderKeyHasher> CSCache;  ///< Compute shader cache
	FShaderBinaryCache BinaryCache;                                     ///< Disk cache manager

	/**
	 * @brief Compile shader from source or load from binary cache
	 *
	 * Priority:
	 * 1. Try load from binary cache (.cso file)
	 * 2. If cache invalid/missing, compile from source
	 * 3. Save to binary cache for next time
	 *
	 * @param Key Shader variant identifier
	 * @param OutBytecode Output bytecode blob (caller must Release)
	 * @return True if compilation/load succeeded
	 */
	bool CompileOrLoadShader(const FShaderKey& Key, ID3DBlob** OutBytecode);

	/**
	 * @brief Compile shader from HLSL source
	 */
	bool CompileFromSource(
		const FShaderKey& Key,
		const char* EntryPoint,
		const char* Profile,
		ID3DBlob** OutBytecode);

	/**
	 * @brief Convert FShaderDefine array to D3D_SHADER_MACRO array
	 */
	TArray<D3D_SHADER_MACRO> ConvertToD3DMacros(const TArray<FShaderDefine>& Defines);

	/**
	 * @brief Get file timestamp
	 */
	FILETIME GetFileTimestamp(const wstring& FilePath) const;
};
