#pragma once
#include "Core/Public/CoreTypes.h"
#include "Renderer/Public/ShaderPool.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>

using namespace std;

/**
 * @brief Represents a single compiled shader variant with all metadata needed for hot-reload
 *
 * A shader variant is created when a shader file is compiled with specific preprocessor defines.
 * For example, TextureVS.hlsl compiled with LIGHTING_MODEL_PHONG is one variant,
 * and the same file compiled with LIGHTING_MODEL_GOURAUD is another variant.
 *
 * NOTE: After refactoring to use FShaderPool, this structure primarily tracks pointer addresses
 * for hot-reload updates. The actual shader objects and reference counting are managed by FShaderPool.
 */
struct FShaderVariant
{
	FShaderKey Key;                                  ///< Unique identifier for this shader variant in the pool
	wstring SourcePath;                              ///< Absolute or relative path to .hlsl file (e.g., "Asset/Shader/TextureVS.hlsl")
	TArray<D3D11_INPUT_ELEMENT_DESC> InputLayout;    ///< Input layout descriptors (only for vertex shaders)

	void** ShaderPtr;                                ///< Pointer to the shader pointer (e.g., &VSPhong in RenderPass)
	ID3D11InputLayout** InputLayoutPtr;              ///< Pointer to input layout pointer (only for vertex shaders, can be nullptr)

	FILETIME LastWriteTime;                          ///< Last successful compilation timestamp
	bool bLastCompileSucceeded;                      ///< Whether the last compilation attempt succeeded
	wstring LastErrorMessage;                        ///< Error message from last failed compilation attempt

	FShaderVariant()
		: ShaderPtr(nullptr)
		, InputLayoutPtr(nullptr)
		, LastWriteTime{}
		, bLastCompileSucceeded(true)
	{}
};

/**
 * @brief Central shader management system for hot-reload support
 *
 * FShaderManager provides shader hot-reload functionality by integrating with FShaderPool.
 * After refactoring, it serves as the bridge between RenderPass shader pointers and the
 * underlying shared shader pool (Flyweight pattern).
 *
 * Architecture (Post-Refactoring):
 * - FShaderPool: Manages compiled shader objects with reference counting and binary caching
 * - FShaderManager: Tracks pointer addresses for hot-reload and delegates compilation to pool
 * - RenderPass: Owns shader pointers, unaware of hot-reload implementation
 *
 * Design Philosophy:
 * - Non-intrusive: RenderPass classes don't need to know about hot-reload
 * - Transparent: Shader pointers are updated in-place, no code changes needed
 * - Robust: Compilation failures don't crash the application, old shaders remain valid
 * - Efficient: Shared compilation via Flyweight pattern, binary caching for fast startup
 *
 * Usage Pattern:
 * 1. RenderResourceFactory calls Register*Shader() during RenderPass construction
 * 2. ShaderManager delegates to Pool.GetOrCompile*() (cache hit or compile)
 * 3. Timestamp-based file watcher detects shader file changes
 * 4. ShaderManager recompiles all variants via Pool.RecompileShader()
 * 5. All tracked pointers are updated automatically
 *
 * @note This is a singleton class accessed via Get()
 * @note Thread-safety: Currently not thread-safe, should only be called from render thread
 */
class FShaderManager
{
public:
	/**
	 * @brief Get the singleton instance of ShaderManager
	 * @return Reference to the global ShaderManager instance
	 */
	static FShaderManager& Get();

	/**
	 * @brief Get the shader pool for direct access
	 * @return Reference to the shader pool
	 */
	FShaderPool& GetPool() { return Pool; }

	/**
	 * @brief Register a vertex shader for hot-reload tracking
	 *
	 * This function stores all information needed to recompile the shader later,
	 * including the source path, preprocessor defines, and input layout.
	 *
	 * @param InFilePath Path to the .hlsl file (e.g., L"Asset/Shader/TextureVS.hlsl")
	 * @param InInputLayoutDescs Input element descriptors for vertex layout
	 * @param OutVertexShader Pointer to the vertex shader pointer (e.g., &VSPhong)
	 * @param OutInputLayout Pointer to the input layout pointer (can be nullptr if not needed)
	 * @param InDefines Preprocessor macro array (will be copied internally), can be nullptr
	 *
	 * @note The function makes heap copies of InDefines and InInputLayoutDescs
	 * @note OutVertexShader must remain valid for the lifetime of the shader tracking
	 */
	void RegisterVertexShader(
		const wstring& InFilePath,
		const TArray<D3D11_INPUT_ELEMENT_DESC>& InInputLayoutDescs,
		ID3D11VertexShader** OutVertexShader,
		ID3D11InputLayout** OutInputLayout,
		const D3D_SHADER_MACRO* InDefines);

	/**
	 * @brief Register a pixel shader for hot-reload tracking
	 *
	 * @param InFilePath Path to the .hlsl file (e.g., L"Asset/Shader/TexturePS.hlsl")
	 * @param OutPixelShader Pointer to the pixel shader pointer (e.g., &PSPhong)
	 * @param InDefines Preprocessor macro array (will be copied internally), can be nullptr
	 *
	 * @note The function makes a heap copy of InDefines
	 * @note OutPixelShader must remain valid for the lifetime of the shader tracking
	 */
	void RegisterPixelShader(
		const wstring& InFilePath,
		ID3D11PixelShader** OutPixelShader,
		const D3D_SHADER_MACRO* InDefines);

	/**
	 * @brief Register a compute shader for hot-reload tracking
	 *
	 * @param InFilePath Path to the .hlsl file (e.g., L"Asset/Shader/LightTilesComputeShader.hlsl")
	 * @param OutComputeShader Pointer to the compute shader pointer
	 * @param InDefines Preprocessor macro array (will be copied internally), can be nullptr
	 * @param Entry Entry point function name (default: "main")
	 * @param Profile Shader model profile (default: "cs_5_0")
	 *
	 * @note The function makes a heap copy of InDefines
	 * @note OutComputeShader must remain valid for the lifetime of the shader tracking
	 */
	void RegisterComputeShader(
		const wstring& InFilePath,
		ID3D11ComputeShader** OutComputeShader,
		const D3D_SHADER_MACRO* InDefines,
		const char* Entry = "main",
		const char* Profile = "cs_5_0");

	/**
	 * @brief Reload a specific shader file and all its variants
	 *
	 * Finds all shader variants compiled from the given file path and recompiles them
	 * using the stored compilation parameters (defines, input layouts, etc.).
	 *
	 * If compilation fails:
	 * - The old shader remains valid and continues to be used
	 * - Error message is stored in the variant for debugging
	 * - bLastCompileSucceeded is set to false
	 *
	 * @param InFilePath Path to the .hlsl file to reload (e.g., L"Asset/Shader/TextureVS.hlsl")
	 * @return Number of variants successfully recompiled
	 *
	 * @note This function is safe to call even if compilation fails
	 * @note Only variants with matching file paths are recompiled
	 *
	 * TODO: Add callback for compilation results (success/failure notifications)
	 * TODO: Consider adding a "dry run" mode that checks syntax without updating shaders
	 */
	int32 ReloadShader(const wstring& InFilePath);

	/**
	 * @brief Reload all registered shaders
	 *
	 * Useful for testing or when multiple shader files have changed.
	 *
	 * @return Total number of variants successfully recompiled
	 *
	 * TODO: Optimize to only recompile shaders that have actually changed on disk
	 */
	int32 ReloadAllShaders();

	/**
	 * @brief Check all tracked shader files for modifications and reload if changed
	 *
	 * This function compares current file timestamps against cached LastWriteTime
	 * for each unique shader file. If any file has been modified, all variants
	 * compiled from that file are recompiled.
	 *
	 * Designed for periodic polling (e.g., every 0.5 seconds) rather than every frame.
	 *
	 * @return Number of files that were detected as modified and reloaded
	 *
	 * @note This function performs file I/O for each unique shader file
	 * @note Currently, each variant is recompiled separately (may have duplicates)
	 * @note Safe to call every frame, but recommended to throttle (see example)
	 *
	 * Example usage:
	 * @code
	 * static float TimeSinceLastCheck = 0.0f;
	 * TimeSinceLastCheck += DeltaTime;
	 * if (TimeSinceLastCheck >= 0.5f) {  // Check every 0.5 seconds
	 *     FShaderManager::Get().CheckAndReloadModifiedShaders();
	 *     TimeSinceLastCheck = 0.0f;
	 * }
	 * @endcode
	 */
	int32 CheckAndReloadModifiedShaders();

	/**
	 * @brief Get the number of registered shader variants
	 * @return Total number of tracked shader variants across all files
	 */
	size_t GetVariantCount() const { return Variants.size(); }

	/**
	 * @brief Check if a specific shader file has any registered variants
	 * @param InFilePath Path to check
	 * @return True if at least one variant exists for this file
	 */
	bool HasVariants(const wstring& InFilePath) const;

	/**
	 * @brief Get compilation status for all variants of a file
	 *
	 * @param InFilePath Path to the shader file
	 * @param OutSuccessCount Number of variants that compiled successfully
	 * @param OutFailureCount Number of variants that failed to compile
	 *
	 * TODO: Return detailed error messages for failed compilations
	 */
	void GetFileCompilationStatus(const wstring& InFilePath, int32& OutSuccessCount, int32& OutFailureCount) const;

	/**
	 * @brief Clear all registered shaders (use with caution!)
	 *
	 * This does NOT release the actual D3D11 shader objects (RenderPasses own those),
	 * it only clears the tracking information.
	 *
	 * @warning After calling this, hot-reload will no longer work until shaders are re-registered
	 */
	void ClearAll();

private:
	FShaderManager() = default;
	~FShaderManager() = default;
	FShaderManager(const FShaderManager&) = delete;
	FShaderManager& operator=(const FShaderManager&) = delete;

	/**
	 * @brief Convert D3D_SHADER_MACRO array to FShaderDefine array
	 * @param InDefines Source macro array (can be nullptr)
	 * @return Array of FShaderDefine structs
	 */
	TArray<FShaderDefine> ConvertDefines(const D3D_SHADER_MACRO* InDefines);

	/**
	 * @brief Internal helper to recompile a single shader variant
	 *
	 * Uses Pool.RecompileShader() to get new shader from pool, then updates
	 * the tracked pointer address.
	 *
	 * @param Variant The variant to recompile
	 * @return True if compilation succeeded, false otherwise
	 *
	 * @note On failure, the variant's shader pointer is NOT modified (old shader remains valid)
	 * @note Error messages are stored in Variant.LastErrorMessage
	 */
	bool RecompileVariant(FShaderVariant& Variant);

	/**
	 * @brief Get current file modification time
	 * @param InFilePath Path to the file
	 * @return FILETIME structure, or zero-initialized if file doesn't exist
	 */
	FILETIME GetFileWriteTime(const wstring& InFilePath) const;

	/**
	 * @brief Compare two FILETIME structures
	 * @return True if Time1 is newer than Time2
	 */
	bool IsFileTimeNewer(const FILETIME& Time1, const FILETIME& Time2) const;

private:
	FShaderPool Pool;                                         ///< Shared shader object pool (Flyweight pattern)
	TArray<FShaderVariant> Variants;                          ///< All registered shader variants
	TMap<wstring, TArray<size_t>> PathToVariantIndices;       ///< Fast lookup: file path -> variant indices

	FILETIME LastShaderFolderTimestamp;                       ///< Cached shader folder timestamp for detecting include file changes

	// TODO: Add mutex for thread-safety when file watcher runs on separate thread
	// TODO: Add performance metrics (recompile time, etc.)
	// TODO: Consider adding a compilation queue for batching multiple reloads
};
