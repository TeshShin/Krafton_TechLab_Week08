#include "pch.h"
#include "Renderer/Public/ShaderManager.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Renderer/Public/Renderer.h"
#include <Windows.h>

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
	FShaderKey Key(InFilePath, Defines, EShaderType::VertexShader);

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
	FShaderKey Key(InFilePath, Defines, EShaderType::PixelShader);

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
	FShaderKey Key(InFilePath, Defines, EShaderType::ComputeShader);

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
	int32 ModifiedFileCount = 0;

	// Check shader folder timestamp for include file changes
	static const wstring ShaderFolderPath = L"Asset/Shader";
	FILETIME CurrentFolderTimestamp = Pool.GetBinaryCache().GetShaderFolderTimestamp(ShaderFolderPath);

	// Check if this is first time (LastShaderFolderTimestamp is zero)
	bool bIsFirstCheck = (LastShaderFolderTimestamp.dwLowDateTime == 0 &&
	                      LastShaderFolderTimestamp.dwHighDateTime == 0);

	if (bIsFirstCheck)
	{
		// First time - just cache the timestamp
		LastShaderFolderTimestamp = CurrentFolderTimestamp;
	}
	else if (CurrentFolderTimestamp.dwLowDateTime != LastShaderFolderTimestamp.dwLowDateTime ||
	         CurrentFolderTimestamp.dwHighDateTime != LastShaderFolderTimestamp.dwHighDateTime)
	{
		// Folder timestamp changed - some file in shader folder was modified (e.g., include files like LightingFunctions.hlsl)
		UE_LOG("ShaderManager: Shader folder modified (include files may have changed) - reloading ALL shaders");

		int32 ReloadedCount = ReloadAllShaders();
		LastShaderFolderTimestamp = CurrentFolderTimestamp;

		if (ReloadedCount > 0)
		{
			UE_LOG("===== Shader Auto-Reload: Folder change detected, %d variant(s) recompiled =====", ReloadedCount);
			return 1; // Return 1 to indicate folder-level change
		}
	}

	// Check individual shader files for modifications
	for (const auto& Pair : PathToVariantIndices)
	{
		const wstring& FilePath = Pair.first;
		const TArray<size_t>& VariantIndices = Pair.second;

		if (VariantIndices.empty()) continue;

		// Get current file timestamp
		FILETIME CurrentFileTime = GetFileWriteTime(FilePath);

		// Check if file exists (zero timestamp means file not found)
		if (CurrentFileTime.dwLowDateTime == 0 && CurrentFileTime.dwHighDateTime == 0)
		{
			// File doesn't exist or can't be accessed
			continue;
		}

		// Compare against the first variant's cached timestamp
		// (All variants from same file should have same timestamp)
		size_t FirstVariantIndex = VariantIndices[0];
		if (FirstVariantIndex >= Variants.size()) continue;

		const FILETIME& CachedFileTime = Variants[FirstVariantIndex].LastWriteTime;

		// Check if file was modified
		if (IsFileTimeNewer(CurrentFileTime, CachedFileTime))
		{
			UE_LOG("ShaderManager: File modified detected: '%ls'", FilePath.c_str());

			// Reload all variants from this file
			int32 ReloadedCount = ReloadShader(FilePath);

			if (ReloadedCount > 0)
			{
				ModifiedFileCount++;
			}
		}
	}

	if (ModifiedFileCount > 0)
	{
		UE_LOG("===== Shader Auto-Reload: %d file(s) detected and recompiled =====", ModifiedFileCount);
	}

	return ModifiedFileCount;
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
	case EShaderType::VertexShader:
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

	case EShaderType::PixelShader:
	{
		// Release old shader
		ID3D11PixelShader* OldPS = *(ID3D11PixelShader**)Variant.ShaderPtr;
		SafeRelease(OldPS);

		// Assign new shader
		*(ID3D11PixelShader**)Variant.ShaderPtr = (ID3D11PixelShader*)NewShaderPtr;
		break;
	}

	case EShaderType::ComputeShader:
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
