#pragma once
#include "Renderer/Public/ShaderCache.h"
#include <d3d11.h>

/**
 * @brief Modern shader creation API using Flyweight pattern and binary caching
 *
 * ShaderFactory provides a clean, pool-based API for shader creation with:
 * - Automatic shader sharing via Flyweight pattern (same variant = same object)
 * - Binary caching (.cso files) for fast startup
 * - MD5 + timestamp validation for cache invalidation
 * - ComPtr-based reference counting (no manual RefCount tracking)
 *
 * Usage Example (with hot-reload support):
 * @code
 * class MyRenderPass {
 *     ID3D11VertexShader* VS = nullptr;
 *     ID3D11InputLayout* Layout = nullptr;
 *     ID3D11PixelShader* PS = nullptr;
 *
 *     void Initialize() {
 *         // Step 1: Create shaders using ShaderFactory
 *         D3D_SHADER_MACRO Defines[] = {
 *             { "LIGHTING_MODEL", "PHONG" },
 *             { nullptr, nullptr }
 *         };
 *
 *         FShaderKey VSKey = ShaderFactory::CreateShaderKey(
 *             L"Asset/Shader/MyShader.hlsl", Defines, EShaderType::VertexShader);
 *         VS = ShaderFactory::CreateVertexShader(VSKey, &Layout, &LayoutDescs);
 *
 *         FShaderKey PSKey = ShaderFactory::CreateShaderKey(
 *             L"Asset/Shader/MyShader.hlsl", Defines, EShaderType::PixelShader);
 *         PS = ShaderFactory::CreatePixelShader(PSKey);
 *
 *         // Step 2: Register for hot-reload (optional)
 *         FShaderManager::Get().RegisterVertexShader(
 *             L"Asset/Shader/MyShader.hlsl", LayoutDescs, &VS, &Layout, Defines);
 *         FShaderManager::Get().RegisterPixelShader(
 *             L"Asset/Shader/MyShader.hlsl", &PS, Defines);
 *     }
 *
 *     void Release() {
 *         SafeRelease(VS);
 *         SafeRelease(Layout);
 *         SafeRelease(PS);
 *     }
 * };
 * @endcode
 *
 * @note For temporary shaders without hot-reload, skip Step 2
 * @note Replaces legacy RenderResourceFactory::CreateVertexShaderAndInputLayout
 */
namespace ShaderFactory
{
	// ===== Helper Functions =====

	/**
	 * @brief Convert D3D_SHADER_MACRO array to FShaderDefine array
	 *
	 * @param InDefines D3D macro array (nullptr-terminated), can be nullptr
	 * @return Array of FShaderDefine
	 */
	TArray<FShaderDefine> ConvertMacrosToDefines(const D3D_SHADER_MACRO* InDefines);

	/**
	 * @brief Create shader key from parameters
	 *
	 * @param Path Shader file path
	 * @param Defines D3D macro array (nullptr-terminated), can be nullptr
	 * @param Type Shader type
	 * @return FShaderKey for pool lookup
	 */
	FShaderKey CreateShaderKey(const wstring& Path, const D3D_SHADER_MACRO* Defines, EShaderType Type);

	// ===== Shader Creation =====

	/**
	 * @brief Create vertex shader from pool with input layout support
	 *
	 * Uses Flyweight pattern + binary caching for optimal performance.
	 * Same shader variant is shared across multiple users via ComPtr reference counting.
	 *
	 * Workflow:
	 * 1. Check pool cache (same Key = cache hit, increment COM RefCount)
	 * 2. If miss, try load from binary cache (.cso file)
	 * 3. If invalid, compile from source and save to cache
	 * 4. Add to pool and return AddRef'd pointer
	 *
	 * @param Key Shader key (file path + defines + type)
	 * @param OutInputLayout Output for input layout (optional, can be nullptr)
	 * @param LayoutDescs Input layout descriptors (required if OutInputLayout is not nullptr)
	 * @param bEnableHotReload If true, registers with ShaderManager for hot-reload (default: true)
	 * @return AddRef'd vertex shader pointer, or nullptr on failure
	 *
	 * @note Caller is responsible for SafeRelease() when done
	 * @note Input layout is cached in pool and reused across users
	 * @note If compilation fails, old shader remains valid (safe hot-reload)
	 */
	ID3D11VertexShader* CreateVertexShader(
		const FShaderKey& Key,
		ID3D11InputLayout** OutInputLayout,
		const TArray<D3D11_INPUT_ELEMENT_DESC>* LayoutDescs,
		bool bEnableHotReload = true);

	/**
	 * @brief Create pixel shader from pool
	 *
	 * @param Key Shader key
	 * @param bEnableHotReload If true, registers for hot-reload (default: true)
	 * @return AddRef'd pixel shader pointer, or nullptr on failure
	 *
	 * @note Caller is responsible for SafeRelease()
	 */
	ID3D11PixelShader* CreatePixelShader(
		const FShaderKey& Key,
		bool bEnableHotReload = true);

	/**
	 * @brief Create compute shader from pool
	 *
	 * @param Key Shader key
	 * @param Entry Entry point (default: "main")
	 * @param Profile Shader model (default: "cs_5_0")
	 * @param bEnableHotReload If true, registers for hot-reload (default: true)
	 * @return AddRef'd compute shader pointer, or nullptr on failure
	 *
	 * @note Caller is responsible for SafeRelease()
	 */
	ID3D11ComputeShader* CreateComputeShader(
		const FShaderKey& Key,
		const char* Entry = "main",
		const char* Profile = "cs_5_0",
		bool bEnableHotReload = true);
}
