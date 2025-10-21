#include "pch.h"
#include "Renderer/Public/ShaderManager.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Renderer/Public/Renderer.h"
#include <Windows.h>

FShaderManager& FShaderManager::Get()
{
	static FShaderManager Instance;
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

	FShaderVariant Variant;
	Variant.SourcePath = InFilePath;
	Variant.Type = EShaderType::VertexShader;
	Variant.ShaderPtr = (void**)OutVertexShader;
	Variant.InputLayoutPtr = OutInputLayout;
	Variant.Defines = CopyDefines(InDefines);
	Variant.InputLayout = InInputLayoutDescs;
	Variant.LastWriteTime = GetFileWriteTime(InFilePath);
	Variant.bLastCompileSucceeded = true;

	size_t Index = Variants.size();
	Variants.push_back(Variant);
	PathToVariantIndices[InFilePath].push_back(Index);

	UE_LOG("ShaderManager: Registered vertex shader variant #%zu from '%ls' (%zu defines)",
		Index, InFilePath.c_str(), Variant.Defines.size());
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

	FShaderVariant Variant;
	Variant.SourcePath = InFilePath;
	Variant.Type = EShaderType::PixelShader;
	Variant.ShaderPtr = (void**)OutPixelShader;
	Variant.InputLayoutPtr = nullptr;
	Variant.Defines = CopyDefines(InDefines);
	Variant.LastWriteTime = GetFileWriteTime(InFilePath);
	Variant.bLastCompileSucceeded = true;

	size_t Index = Variants.size();
	Variants.push_back(Variant);
	PathToVariantIndices[InFilePath].push_back(Index);

	UE_LOG("ShaderManager: Registered pixel shader variant #%zu from '%ls' (%zu defines)",
		Index, InFilePath.c_str(), Variant.Defines.size());
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

	FShaderVariant Variant;
	Variant.SourcePath = InFilePath;
	Variant.Type = EShaderType::ComputeShader;
	Variant.ShaderPtr = (void**)OutComputeShader;
	Variant.InputLayoutPtr = nullptr;
	Variant.Defines = CopyDefines(InDefines);
	Variant.LastWriteTime = GetFileWriteTime(InFilePath);
	Variant.bLastCompileSucceeded = true;

	size_t Index = Variants.size();
	Variants.push_back(Variant);
	PathToVariantIndices[InFilePath].push_back(Index);

	UE_LOG("ShaderManager: Registered compute shader variant #%zu from '%ls' (%zu defines)",
		Index, InFilePath.c_str(), Variant.Defines.size());

	// TODO: Store Entry and Profile for compute shaders (currently not needed for basic hot-reload)
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

	// Iterate through unique shader files (not variants)
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

	// Free heap-allocated define strings
	for (FShaderVariant& Variant : Variants)
	{
		for (D3D_SHADER_MACRO& Macro : Variant.Defines)
		{
			if (Macro.Name)
			{
				free((void*)Macro.Name);
				Macro.Name = nullptr;
			}
			if (Macro.Definition)
			{
				free((void*)Macro.Definition);
				Macro.Definition = nullptr;
			}
		}
	}

	Variants.clear();
	PathToVariantIndices.clear();
}

TArray<D3D_SHADER_MACRO> FShaderManager::CopyDefines(const D3D_SHADER_MACRO* InDefines)
{
	TArray<D3D_SHADER_MACRO> Result;

	if (!InDefines)
	{
		// No defines: add null terminator
		Result.push_back({ nullptr, nullptr });
		return Result;
	}

	// Copy all defines until we hit the null terminator
	for (int i = 0; InDefines[i].Name != nullptr; ++i)
	{
		D3D_SHADER_MACRO Macro;
		Macro.Name = _strdup(InDefines[i].Name);
		Macro.Definition = InDefines[i].Definition ? _strdup(InDefines[i].Definition) : nullptr;
		Result.push_back(Macro);
	}

	// Add null terminator
	Result.push_back({ nullptr, nullptr });

	return Result;
}

bool FShaderManager::RecompileVariant(FShaderVariant& Variant)
{
	// Prepare macro array pointer (D3D11 expects nullptr if no defines)
	const D3D_SHADER_MACRO* DefinesPtr = Variant.Defines.empty() ? nullptr : Variant.Defines.data();

	bool bSuccess = false;

	switch (Variant.Type)
	{
	case EShaderType::VertexShader:
	{
		ID3D11VertexShader* NewShader = nullptr;
		ID3D11InputLayout* NewLayout = nullptr;

		// Recompile using RenderResourceFactory (with bEnableHotReload=false to avoid re-registration)
		FRenderResourceFactory::CreateVertexShaderAndInputLayout(
			Variant.SourcePath,
			Variant.InputLayout,
			&NewShader,
			Variant.InputLayoutPtr ? &NewLayout : nullptr,
			DefinesPtr,
			false  // ⭐ Disable hot-reload to prevent infinite recursion
		);

		if (NewShader)
		{
			// Success: Replace old shader
			SafeRelease(*(ID3D11VertexShader**)Variant.ShaderPtr);
			*(ID3D11VertexShader**)Variant.ShaderPtr = NewShader;

			if (Variant.InputLayoutPtr && NewLayout)
			{
				SafeRelease(*Variant.InputLayoutPtr);
				*Variant.InputLayoutPtr = NewLayout;
			}

			bSuccess = true;
			Variant.bLastCompileSucceeded = true;
			Variant.LastWriteTime = GetFileWriteTime(Variant.SourcePath);
			Variant.LastErrorMessage.clear();
		}
		else
		{
			// Failure: Keep old shader, store error
			Variant.bLastCompileSucceeded = false;
			Variant.LastErrorMessage = L"Compilation failed (check debug output for details)";
		}
		break;
	}

	case EShaderType::PixelShader:
	{
		ID3D11PixelShader* NewShader = nullptr;

		FRenderResourceFactory::CreatePixelShader(
			Variant.SourcePath,
			&NewShader,
			DefinesPtr,
			false  // ⭐ Disable hot-reload
		);

		if (NewShader)
		{
			SafeRelease(*(ID3D11PixelShader**)Variant.ShaderPtr);
			*(ID3D11PixelShader**)Variant.ShaderPtr = NewShader;

			bSuccess = true;
			Variant.bLastCompileSucceeded = true;
			Variant.LastWriteTime = GetFileWriteTime(Variant.SourcePath);
			Variant.LastErrorMessage.clear();
		}
		else
		{
			Variant.bLastCompileSucceeded = false;
			Variant.LastErrorMessage = L"Compilation failed (check debug output for details)";
		}
		break;
	}

	case EShaderType::ComputeShader:
	{
		ID3D11ComputeShader* NewShader = nullptr;

		FRenderResourceFactory::CreateComputeShader(
			Variant.SourcePath,
			&NewShader,
			DefinesPtr,
			"main",
			"cs_5_0",
			false  // ⭐ Disable hot-reload
		);

		if (NewShader)
		{
			SafeRelease(*(ID3D11ComputeShader**)Variant.ShaderPtr);
			*(ID3D11ComputeShader**)Variant.ShaderPtr = NewShader;

			bSuccess = true;
			Variant.bLastCompileSucceeded = true;
			Variant.LastWriteTime = GetFileWriteTime(Variant.SourcePath);
			Variant.LastErrorMessage.clear();
		}
		else
		{
			Variant.bLastCompileSucceeded = false;
			Variant.LastErrorMessage = L"Compilation failed (check debug output for details)";
		}
		break;
	}
	}

	return bSuccess;
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
