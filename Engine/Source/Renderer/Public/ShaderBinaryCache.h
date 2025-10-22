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
	 * @brief Load compiled shader from cache file with validation
	 *
	 * This method performs atomic read-and-validate operation:
	 * 1. Read cache file (once)
	 * 2. Validate header integrity
	 * 3. Validate source hash (MD5)
	 * 4. Validate shader folder timestamp
	 * 5. Validate compile flags
	 *
	 * SOLID Principles:
	 * - Single Responsibility: One method for complete cache loading
	 * - Don't Repeat Yourself: File read happens only once
	 * - Command-Query Separation: Returns bool + fills OutEntry on success
	 *
	 * @param Key Shader variant identifier
	 * @param ShaderFolderTimestamp Latest timestamp of any .hlsl file in shader folder
	 * @param CompileFlags D3DCompile flags used during compilation
	 * @param OutEntry Loaded cache entry (header + metadata + bytecode)
	 * @return True if cache exists, is valid, and loaded successfully
	 */
	bool LoadFromCache(
		const FShaderKey& Key,
		const FILETIME& ShaderFolderTimestamp,
		uint32 CompileFlags,
		FShaderCacheEntry& OutEntry);

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

	// ===== File I/O Operations =====

	/**
	 * @brief Ensure cache directory exists, create if needed
	 */
	bool EnsureCacheDirectoryExists();

	/**
	 * @brief Write cache entry to file
	 */
	bool WriteEntryToFile(const wstring& FilePath, const FShaderCacheEntry& Entry);

	/**
	 * @brief Read cache entry from file
	 */
	bool ReadEntryFromFile(const wstring& FilePath, FShaderCacheEntry& OutEntry);

	// ===== Validation Operations (SRP: Each method has single responsibility) =====

	/**
	 * @brief Validate entire cache entry against current state
	 *
	 * Aggregates all validation checks in correct order.
	 * Early-return on first validation failure for performance.
	 *
	 * @param Entry Cache entry to validate
	 * @param Key Shader key for hash validation
	 * @param ShaderFolderTimestamp Current shader folder timestamp
	 * @param CompileFlags Current compile flags
	 * @return True if all validations pass
	 */
	bool ValidateCacheEntry(
		const FShaderCacheEntry& Entry,
		const FShaderKey& Key,
		const FILETIME& ShaderFolderTimestamp,
		uint32 CompileFlags) const;

	/**
	 * @brief Validate cache header integrity
	 * @return True if header magic number and version are valid
	 */
	bool ValidateHeader(const FShaderCacheHeader& Header) const;

	/**
	 * @brief Validate source file hash
	 * @return True if current hash matches cached hash
	 */
	bool ValidateSourceHash(const FShaderCacheEntry& Entry, const FShaderKey& Key) const;

	/**
	 * @brief Validate shader folder timestamp
	 * @return True if no .hlsl file is newer than cached version
	 */
	bool ValidateTimestamp(const FShaderCacheEntry& Entry, const FILETIME& ShaderFolderTimestamp) const;

	/**
	 * @brief Validate compile flags
	 * @return True if compile flags match
	 */
	bool ValidateCompileFlags(const FShaderCacheEntry& Entry, uint32 CompileFlags) const;

	// ===== Utility Operations =====

	/**
	 * @brief Read file timestamp
	 */
	FILETIME GetFileTimestamp(const wstring& FilePath) const;

	/**
	 * @brief Compare two FILETIME structures
	 * @return True if Time1 is newer than Time2
	 */
	bool IsFileTimeNewer(const FILETIME& Time1, const FILETIME& Time2) const;
};
