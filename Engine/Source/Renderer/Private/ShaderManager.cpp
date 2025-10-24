#include "pch.h"
#include "Renderer/Public/ShaderManager.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Renderer/Public/Renderer.h"
#include <Windows.h>
#include <wincrypt.h>
#include <filesystem>
#include <fstream>

#pragma comment(lib, "crypt32.lib")

FShaderManager& FShaderManager::Get()
{
	static FShaderManager Instance;
	static bool bInitialized = false;

	if (!bInitialized)
	{
		Instance.Pool.Initialize();

		// Initialize shader folder timestamp to zero
		Instance.LastShaderFolderTimestamp.dwLowDateTime = 0;
		Instance.LastShaderFolderTimestamp.dwHighDateTime = 0;

		bInitialized = true;
		UE_LOG("ShaderManager: Initialized with FShaderPool");
	}

	return Instance;
}

void FShaderManager::RegisterVertexShader(
	const wstring& InFilePath,
	const TArray<D3D11_INPUT_ELEMENT_DESC>& InInputLayoutDescs,
	ID3D11VertexShader** OutVertexShader,
	ID3D11InputLayout** OutInputLayout,
	const D3D_SHADER_MACRO* InDefines)
{
	if (!OutVertexShader)
	{
		UE_LOG_ERROR("ShaderManager: Cannot register vertex shader with null output pointer");
		return;
	}

	// Convert D3D_SHADER_MACRO to FShaderDefine
	TArray<FShaderDefine> Defines = ConvertDefines(InDefines);

	// Create shader key for tracking
	FShaderKey Key(InFilePath, Defines, EShaderType::EST_Vertex);

	// Track variant for hot-reload (don't modify the shader, just track it)
	FShaderVariant Variant;
	Variant.Key = Key;
	Variant.SourcePath = InFilePath;
	Variant.InputLayout = InInputLayoutDescs;
	Variant.ShaderPtr = (void**)OutVertexShader;
	Variant.InputLayoutPtr = OutInputLayout;
	Variant.LastWriteTime = GetFileWriteTime(InFilePath);
	Variant.bLastCompileSucceeded = true;

	size_t Index = Variants.size();
	Variants.push_back(Variant);
	PathToVariantIndices[InFilePath].push_back(Index);

	UE_LOG("ShaderManager: Registered vertex shader variant #%zu from '%ls' (%zu defines)",
		Index, InFilePath.c_str(), Defines.size());
}

void FShaderManager::RegisterPixelShader(
	const wstring& InFilePath,
	ID3D11PixelShader** OutPixelShader,
	const D3D_SHADER_MACRO* InDefines)
{
	if (!OutPixelShader)
	{
		UE_LOG_ERROR("ShaderManager: Cannot register pixel shader with null output pointer");
		return;
	}

	// Convert D3D_SHADER_MACRO to FShaderDefine
	TArray<FShaderDefine> Defines = ConvertDefines(InDefines);

	// Create shader key for tracking
	FShaderKey Key(InFilePath, Defines, EShaderType::EST_Pixel);

	// Track variant for hot-reload (don't modify the shader, just track it)
	FShaderVariant Variant;
	Variant.Key = Key;
	Variant.SourcePath = InFilePath;
	Variant.ShaderPtr = (void**)OutPixelShader;
	Variant.InputLayoutPtr = nullptr;
	Variant.LastWriteTime = GetFileWriteTime(InFilePath);
	Variant.bLastCompileSucceeded = true;

	size_t Index = Variants.size();
	Variants.push_back(Variant);
	PathToVariantIndices[InFilePath].push_back(Index);

	UE_LOG("ShaderManager: Registered pixel shader variant #%zu from '%ls' (%zu defines)",
		Index, InFilePath.c_str(), Defines.size());
}

void FShaderManager::RegisterComputeShader(
	const wstring& InFilePath,
	ID3D11ComputeShader** OutComputeShader,
	const D3D_SHADER_MACRO* InDefines,
	const char* Entry,
	const char* Profile)
{
	if (!OutComputeShader)
	{
		UE_LOG_ERROR("ShaderManager: Cannot register compute shader with null output pointer");
		return;
	}

	// Convert D3D_SHADER_MACRO to FShaderDefine
	TArray<FShaderDefine> Defines = ConvertDefines(InDefines);

	// Create shader key for tracking
	FShaderKey Key(InFilePath, Defines, EShaderType::EST_Compute);

	// Track variant for hot-reload (don't modify the shader, just track it)
	FShaderVariant Variant;
	Variant.Key = Key;
	Variant.SourcePath = InFilePath;
	Variant.ShaderPtr = (void**)OutComputeShader;
	Variant.InputLayoutPtr = nullptr;
	Variant.LastWriteTime = GetFileWriteTime(InFilePath);
	Variant.bLastCompileSucceeded = true;

	size_t Index = Variants.size();
	Variants.push_back(Variant);
	PathToVariantIndices[InFilePath].push_back(Index);

	UE_LOG("ShaderManager: Registered compute shader variant #%zu from '%ls' (%zu defines)",
		Index, InFilePath.c_str(), Defines.size());

	// TODO: Store Entry and Profile for compute shaders if non-standard variants needed
}

int32 FShaderManager::ReloadShader(const wstring& InFilePath)
{
	auto It = PathToVariantIndices.find(InFilePath);
	if (It == PathToVariantIndices.end())
	{
		UE_LOG_WARNING("ShaderManager: No variants registered for '%ls'", InFilePath.c_str());
		return 0;
	}

	int32 SuccessCount = 0;
	const TArray<size_t>& Indices = It->second;

	UE_LOG("ShaderManager: Reloading %zu variant(s) from '%ls'...", Indices.size(), InFilePath.c_str());

	for (size_t Index : Indices)
	{
		if (Index >= Variants.size())
		{
			UE_LOG_ERROR("ShaderManager: Invalid variant index %zu", Index);
			continue;
		}

		FShaderVariant& Variant = Variants[Index];
		if (RecompileVariant(Variant))
		{
			SuccessCount++;
			UE_LOG("ShaderManager: ✓ Variant #%zu recompiled successfully", Index);
		}
		else
		{
			UE_LOG_ERROR("ShaderManager: ✗ Variant #%zu failed to recompile: %ls",
				Index, Variant.LastErrorMessage.c_str());
		}
	}

	UE_LOG("ShaderManager: Reload complete: %d/%zu variants succeeded", SuccessCount, Indices.size());
	return SuccessCount;
}

int32 FShaderManager::ReloadAllShaders()
{
	UE_LOG("ShaderManager: Reloading all %zu shader variant(s)...", Variants.size());

	int32 SuccessCount = 0;
	for (size_t i = 0; i < Variants.size(); ++i)
	{
		if (RecompileVariant(Variants[i]))
		{
			SuccessCount++;
		}
	}

	UE_LOG("ShaderManager: Reload all complete: %d/%zu variants succeeded", SuccessCount, Variants.size());
	return SuccessCount;
}

int32 FShaderManager::CheckAndReloadModifiedShaders()
{
	static const wstring ShaderFolderPath = L"Asset/Shader";

	// Check if this is first time (TrackedShaderFiles is empty)
	if (TrackedShaderFiles.empty())
	{
		// First time - initialize tracking for all .hlsl files in Asset/Shader
		UE_LOG("ShaderManager: Initializing shader file tracking for '%ls'", ShaderFolderPath.c_str());
		UpdateTrackedShaderFiles(ShaderFolderPath);
		UE_LOG("ShaderManager: Tracking %zu shader file(s)", TrackedShaderFiles.size());
		return 0;
	}

	// Track which files have been modified
	TArray<wstring> ModifiedFiles;

	try
	{
		// Scan all .hlsl files and compare with tracked metadata
		for (const auto& Entry : std::filesystem::recursive_directory_iterator(ShaderFolderPath))
		{
			if (Entry.is_regular_file())
			{
				wstring Extension = Entry.path().extension().wstring();
				if (Extension == L".hlsl")
				{
					wstring FilePath = Entry.path().wstring();

					// Get current file metadata
					FILETIME CurrentTime = GetFileWriteTime(FilePath);
					uint8 CurrentHash[16] = {};

					if (!CalculateFileMD5(FilePath, CurrentHash))
					{
						UE_LOG_WARNING("ShaderManager: Failed to calculate MD5 for '%ls', skipping", FilePath.c_str());
						continue;
					}

					// Check if file is tracked
					auto It = TrackedShaderFiles.find(FilePath);
					if (It == TrackedShaderFiles.end())
					{
						// New file detected (not previously tracked)
						UE_LOG("ShaderManager: New shader file detected: '%ls'", FilePath.c_str());
						ModifiedFiles.push_back(FilePath);

						// Add to tracking
						FShaderFileInfo& FileInfo = TrackedShaderFiles[FilePath];
						FileInfo.FilePath = FilePath;
						FileInfo.LastWriteTime = CurrentTime;
						memcpy(FileInfo.MD5Hash, CurrentHash, 16);
					}
					else
					{
						const FShaderFileInfo& CachedInfo = It->second;

						// Check timestamp first (fast check)
						bool bTimestampChanged = IsFileTimeNewer(CurrentTime, CachedInfo.LastWriteTime) ||
						                         IsFileTimeNewer(CachedInfo.LastWriteTime, CurrentTime);

						// If timestamp changed, verify with MD5 (accurate check)
						if (bTimestampChanged)
						{
							bool bContentChanged = (memcmp(CurrentHash, CachedInfo.MD5Hash, 16) != 0);

							if (bContentChanged)
							{
								UE_LOG("ShaderManager: File content modified: '%ls'", FilePath.c_str());
								ModifiedFiles.push_back(FilePath);

								// Update tracked info
								FShaderFileInfo& FileInfo = TrackedShaderFiles[FilePath];
								FileInfo.LastWriteTime = CurrentTime;
								memcpy(FileInfo.MD5Hash, CurrentHash, 16);
							}
							else
							{
								// Timestamp changed but content is same (e.g., file touched without edit)
								// Update timestamp silently
								TrackedShaderFiles[FilePath].LastWriteTime = CurrentTime;
							}
						}
					}
				}
			}
		}

		// Check for deleted files (files in TrackedShaderFiles but no longer exist)
		TArray<wstring> DeletedFiles;
		for (const auto& Pair : TrackedShaderFiles)
		{
			if (!std::filesystem::exists(Pair.first))
			{
				UE_LOG_WARNING("ShaderManager: Shader file deleted: '%ls'", Pair.first.c_str());
				DeletedFiles.push_back(Pair.first);
			}
		}

		// Remove deleted files from tracking
		for (const wstring& DeletedFile : DeletedFiles)
		{
			TrackedShaderFiles.erase(DeletedFile);
		}

		// If any file was modified or deleted, reload all shaders
		if (!ModifiedFiles.empty() || !DeletedFiles.empty())
		{
			UE_LOG("===== Shader Auto-Reload: %zu file(s) modified, %zu file(s) deleted =====",
				ModifiedFiles.size(), DeletedFiles.size());

			// Log each modified file
			for (const wstring& ModifiedFile : ModifiedFiles)
			{
				UE_LOG("  - Modified: '%ls'", ModifiedFile.c_str());
			}
			for (const wstring& DeletedFile : DeletedFiles)
			{
				UE_LOG("  - Deleted: '%ls'", DeletedFile.c_str());
			}

			// Clear binary cache to force recompilation from source
			// This ensures that modified files are actually recompiled, not loaded from stale cache
			UE_LOG("ShaderManager: Clearing binary cache to force recompilation");
			Pool.GetBinaryCache().ClearCache();

			// Reload all shader variants
			int32 ReloadedCount = ReloadAllShaders();
			UE_LOG("===== Shader Auto-Reload: %d shader variant(s) recompiled =====", ReloadedCount);

			return static_cast<int32>(ModifiedFiles.size() + DeletedFiles.size());
		}
	}
	catch (const std::exception& e)
	{
		UE_LOG_ERROR("ShaderManager: Exception during shader file checking: %s", e.what());
	}

	return 0;
}

bool FShaderManager::HasVariants(const wstring& InFilePath) const
{
	auto It = PathToVariantIndices.find(InFilePath);
	return It != PathToVariantIndices.end() && !It->second.empty();
}

void FShaderManager::GetFileCompilationStatus(const wstring& InFilePath, int32& OutSuccessCount, int32& OutFailureCount) const
{
	OutSuccessCount = 0;
	OutFailureCount = 0;

	auto It = PathToVariantIndices.find(InFilePath);
	if (It == PathToVariantIndices.end()) return;

	for (size_t Index : It->second)
	{
		if (Index < Variants.size())
		{
			if (Variants[Index].bLastCompileSucceeded)
				OutSuccessCount++;
			else
				OutFailureCount++;
		}
	}
}

void FShaderManager::ClearAll()
{
	UE_LOG_WARNING("ShaderManager: Clearing all %zu variant(s). Hot-reload will no longer work until re-registration.", Variants.size());

	// Note: We don't need to manually free FShaderDefine strings - they manage their own memory via std::string
	// We also don't release shaders here because FShaderPool manages them with reference counting

	Variants.clear();
	PathToVariantIndices.clear();

	// Optionally clear the pool as well (releases all shaders)
	// Pool.Release(); // Uncomment if you want to clear the entire pool
}

TArray<FShaderDefine> FShaderManager::ConvertDefines(const D3D_SHADER_MACRO* InDefines)
{
	TArray<FShaderDefine> Result;

	if (!InDefines)
	{
		return Result;
	}

	// Convert all defines until we hit the null terminator
	for (int i = 0; InDefines[i].Name != nullptr; ++i)
	{
		Result.push_back(FShaderDefine(
			string(InDefines[i].Name),
			InDefines[i].Definition ? string(InDefines[i].Definition) : ""
		));
	}

	return Result;
}

bool FShaderManager::RecompileVariant(FShaderVariant& Variant)
{
	// Use Pool.RecompileShader() to get new shader
	void* NewShaderPtr = Pool.RecompileShader(Variant.Key);

	if (!NewShaderPtr)
	{
		// Compilation failed: Keep old shader
		Variant.bLastCompileSucceeded = false;
		Variant.LastErrorMessage = L"Compilation failed (check debug output for details)";
		return false;
	}

	// Success: Update pointer based on shader type
	// IMPORTANT: Must release old shader to decrement RenderPass's reference count
	switch (Variant.Key.Type)
	{
	case EShaderType::EST_Vertex:
	{
		// Release RenderPass's old shader reference
		ID3D11VertexShader* OldVS = *(ID3D11VertexShader**)Variant.ShaderPtr;
		SafeRelease(OldVS);

		// Assign new shader (already AddRef'd by Pool.RecompileShader)
		*(ID3D11VertexShader**)Variant.ShaderPtr = (ID3D11VertexShader*)NewShaderPtr;

		// For vertex shaders, also update input layout if it exists
		if (Variant.InputLayoutPtr)
		{
			// Release old layout
			ID3D11InputLayout* OldLayout = *Variant.InputLayoutPtr;
			SafeRelease(OldLayout);

			// Get new layout from pool
			ID3D11InputLayout* NewLayout = nullptr;
			Pool.GetOrCompileVS(Variant.Key, &NewLayout, &Variant.InputLayout);
			if (NewLayout)
			{
				*Variant.InputLayoutPtr = NewLayout;
			}
		}
		break;
	}

	case EShaderType::EST_Pixel:
	{
		// Release old shader
		ID3D11PixelShader* OldPS = *(ID3D11PixelShader**)Variant.ShaderPtr;
		SafeRelease(OldPS);

		// Assign new shader
		*(ID3D11PixelShader**)Variant.ShaderPtr = (ID3D11PixelShader*)NewShaderPtr;
		break;
	}

	case EShaderType::EST_Compute:
	{
		// Release old shader
		ID3D11ComputeShader* OldCS = *(ID3D11ComputeShader**)Variant.ShaderPtr;
		SafeRelease(OldCS);

		// Assign new shader
		*(ID3D11ComputeShader**)Variant.ShaderPtr = (ID3D11ComputeShader*)NewShaderPtr;
		break;
	}
	}

	// Update metadata
	Variant.bLastCompileSucceeded = true;
	Variant.LastWriteTime = GetFileWriteTime(Variant.SourcePath);
	Variant.LastErrorMessage.clear();

	return true;
}

FILETIME FShaderManager::GetFileWriteTime(const wstring& InFilePath) const
{
	FILETIME Result = {};

	HANDLE FileHandle = CreateFileW(
		InFilePath.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		GetFileTime(FileHandle, nullptr, nullptr, &Result);
		CloseHandle(FileHandle);
	}

	return Result;
}

bool FShaderManager::IsFileTimeNewer(const FILETIME& Time1, const FILETIME& Time2) const
{
	// Convert FILETIME to 64-bit integer for easier comparison
	ULARGE_INTEGER t1, t2;
	t1.LowPart = Time1.dwLowDateTime;
	t1.HighPart = Time1.dwHighDateTime;
	t2.LowPart = Time2.dwLowDateTime;
	t2.HighPart = Time2.dwHighDateTime;

	return t1.QuadPart > t2.QuadPart;
}

bool FShaderManager::CalculateFileMD5(const wstring& FilePath, uint8 OutHash[16]) const
{
	// Read file content
	std::ifstream File(FilePath, std::ios::binary);
	if (!File.is_open())
	{
		UE_LOG_ERROR("ShaderManager: Cannot open file '%ls' for MD5 calculation", FilePath.c_str());
		return false;
	}

	// Get file size
	File.seekg(0, std::ios::end);
	size_t FileSize = File.tellg();
	File.seekg(0, std::ios::beg);

	// Read entire file into memory
	TArray<char> Content(FileSize);
	File.read(Content.data(), FileSize);
	File.close();

	// Calculate MD5 using Windows Crypto API
	HCRYPTPROV hProv = 0;
	HCRYPTHASH hHash = 0;

	if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		UE_LOG_ERROR("ShaderManager: CryptAcquireContext failed");
		return false;
	}

	if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
	{
		CryptReleaseContext(hProv, 0);
		UE_LOG_ERROR("ShaderManager: CryptCreateHash failed");
		return false;
	}

	// Hash the file content
	CryptHashData(hHash, reinterpret_cast<const BYTE*>(Content.data()), static_cast<DWORD>(Content.size()), 0);

	// Get hash value
	DWORD HashLen = 16;
	CryptGetHashParam(hHash, HP_HASHVAL, OutHash, &HashLen, 0);

	CryptDestroyHash(hHash);
	CryptReleaseContext(hProv, 0);

	return true;
}

void FShaderManager::UpdateTrackedShaderFiles(const wstring& ShaderFolderPath)
{
	try
	{
		// Recursively iterate all .hlsl files in the shader folder
		for (const auto& Entry : std::filesystem::recursive_directory_iterator(ShaderFolderPath))
		{
			if (Entry.is_regular_file())
			{
				wstring Extension = Entry.path().extension().wstring();
				if (Extension == L".hlsl")
				{
					wstring FilePath = Entry.path().wstring();

					// Get current file metadata
					FILETIME CurrentTime = GetFileWriteTime(FilePath);
					uint8 CurrentHash[16] = {};

					if (!CalculateFileMD5(FilePath, CurrentHash))
					{
						UE_LOG_WARNING("ShaderManager: Failed to calculate MD5 for '%ls'", FilePath.c_str());
						continue;
					}

					// Update or insert tracked file info
					FShaderFileInfo& FileInfo = TrackedShaderFiles[FilePath];
					FileInfo.FilePath = FilePath;
					FileInfo.LastWriteTime = CurrentTime;
					memcpy(FileInfo.MD5Hash, CurrentHash, 16);
				}
			}
		}
	}
	catch (const std::exception& e)
	{
		UE_LOG_ERROR("ShaderManager: Exception while scanning shader folder '%ls': %s",
			ShaderFolderPath.c_str(), e.what());
	}
}
