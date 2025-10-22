#pragma once
#include "Renderer/Public/ShaderCache.h"
#include <Windows.h>

/**
 * @brief Manages shader binary cache files (.cso)
 *
 * Responsibilities:
 * - Save compiled shader bytecode to disk
 * - Load cached bytecode from disk
 * - Validate cache integrity (checksum, timestamp)
 * - Determine if recompilation is needed
 *
 * Cache Directory Structure:
 *   Intermediate/ShaderCache/
 *   ├── TextureVS_PHONG_1A2B3C4D.cso
 *   ├── TextureVS_GOURAUD_2B3C4D5E.cso
 *   ├── TexturePS_PHONG_3C4D5E6F.cso
 *   └── ...
 */
class FShaderBinaryCache
{
public:
	FShaderBinaryCache();
	~FShaderBinaryCache();

	/**
	 * @brief Initialize cache system and ensure cache directory exists
	 * @param InCacheDirectory Path to cache directory (default: "Intermediate/ShaderCache")
	 */
	void Initialize(const wstring& InCacheDirectory = L"Intermediate/ShaderCache");

	/**
	 * @brief Save compiled shader to cache file
	 *
	 * @param Key Shader variant identifier
	 * @param Bytecode Compiled shader bytecode
	 * @param BytecodeSize Size of bytecode in bytes
	 * @param ShaderFolderTimestamp Latest timestamp of any .hlsl file in shader folder
	 * @param CompileFlags D3DCompile flags used during compilation
	 * @return True if save succeeded
	 */
	bool SaveToCache(
		const FShaderKey& Key,
		const void* Bytecode,
		size_t BytecodeSize,
		const FILETIME& ShaderFolderTimestamp,
		uint32 CompileFlags);

	/**
	 * @brief Load compiled shader from cache file
	 *
	 * @param Key Shader variant identifier
	 * @param OutEntry Loaded cache entry (header + metadata + bytecode)
	 * @return True if load succeeded and cache is valid
	 */
	bool LoadFromCache(
		const FShaderKey& Key,
		FShaderCacheEntry& OutEntry);

	/**
	 * @brief Check if cache file exists and is valid for given key
	 *
	 * @param Key Shader variant identifier
	 * @param ShaderFolderTimestamp Latest timestamp of any .hlsl file in shader folder
	 * @param CompileFlags D3DCompile flags used during compilation
	 * @return True if cache exists and is up-to-date
	 */
	bool IsCacheValid(
		const FShaderKey& Key,
		const FILETIME& ShaderFolderTimestamp,
		uint32 CompileFlags);

	/**
	 * @brief Calculate MD5 hash of shader source + defines
	 *
	 * Used for cache validation. If source or defines change, hash changes.
	 *
	 * @param Key Shader key containing source path and defines
	 * @param OutHash Output MD5 hash (16 bytes)
	 * @return True if hash calculated successfully
	 */
	bool CalculateSourceHash(
		const FShaderKey& Key,
		uint8 OutHash[16]);

	/**
	 * @brief Get full path to cache file for given key
	 * @param Key Shader variant identifier
	 * @return Full path like "Intermediate/ShaderCache/TextureVS_PHONG_1A2B3C4D.cso"
	 */
	wstring GetCacheFilePath(const FShaderKey& Key) const;

	/**
	 * @brief Clear all cache files (useful for debugging)
	 */
	void ClearCache();

	/**
	 * @brief Get cache directory path
	 */
	const wstring& GetCacheDirectory() const { return CacheDirectory; }

	/**
	 * @brief Get latest timestamp of all .hlsl files in shader folder
	 *
	 * Used for dependency tracking. If any .hlsl file in the folder changes,
	 * all shaders should be recompiled due to potential #include dependencies.
	 *
	 * @param ShaderFolderPath Path to shader folder (e.g., "Asset/Shader")
	 * @return Latest FILETIME of any .hlsl file in the folder
	 */
	FILETIME GetShaderFolderTimestamp(const wstring& ShaderFolderPath) const;

private:
	wstring CacheDirectory;  ///< Path to cache directory

	/**
	 * @brief Ensure cache directory exists, create if needed
	 */
	bool EnsureCacheDirectoryExists();

	/**
	 * @brief Read file timestamp
	 */
	FILETIME GetFileTimestamp(const wstring& FilePath) const;

	/**
	 * @brief Compare two FILETIME structures
	 * @return True if Time1 is newer than Time2
	 */
	bool IsFileTimeNewer(const FILETIME& Time1, const FILETIME& Time2) const;

	/**
	 * @brief Write cache entry to file
	 */
	bool WriteEntryToFile(const wstring& FilePath, const FShaderCacheEntry& Entry);

	/**
	 * @brief Read cache entry from file
	 */
	bool ReadEntryFromFile(const wstring& FilePath, FShaderCacheEntry& OutEntry);
};
