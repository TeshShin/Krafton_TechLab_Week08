#include "pch.h"
#include "Renderer/Public/ShaderPool.h"
#include "Renderer/Public/Renderer.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

FShaderPool::FShaderPool()
{
}

FShaderPool::~FShaderPool()
{
	Release();
}

void FShaderPool::Initialize()
{
	BinaryCache.Initialize();
	UE_LOG("ShaderPool: Initialized");
}

void FShaderPool::Release()
{
	// ComPtr automatically releases when cleared
	VSCache.clear();
	PSCache.clear();
	CSCache.clear();
	UE_LOG("ShaderPool: Released all shaders");
}

ID3D11VertexShader* FShaderPool::GetOrCompileVS(
	const FShaderKey& Key,
	ID3D11InputLayout** OutInputLayout,
	const TArray<D3D11_INPUT_ELEMENT_DESC>* LayoutDescs)
{
	// Check if already cached
	auto It = VSCache.find(Key);
	if (It != VSCache.end())
	{
		FCachedVertexShader& Cached = It->second;

		// Return AddRef'd pointer (caller owns a reference)
		ID3D11VertexShader* VS = nullptr;
		Cached.Shader.CopyTo(&VS);

		if (OutInputLayout && Cached.InputLayout)
		{
			Cached.InputLayout.CopyTo(OutInputLayout);
		}

		UE_LOG("ShaderPool: GetOrCompileVS cache hit '%ls'", Key.SourcePath.c_str());
		return VS;
	}

	// Not cached - compile or load
	ID3DBlob* Bytecode = nullptr;
	if (!CompileOrLoadShader(Key, &Bytecode))
	{
		UE_LOG_ERROR("ShaderPool: Failed to compile/load VS '%ls'", Key.SourcePath.c_str());
		return nullptr;
	}

	// Create D3D11 vertex shader
	ComPtr<ID3D11VertexShader> VS;
	HRESULT hr = URenderer::GetInstance().GetDevice()->CreateVertexShader(
		Bytecode->GetBufferPointer(),
		Bytecode->GetBufferSize(),
		nullptr,
		&VS);

	if (FAILED(hr))
	{
		UE_LOG_ERROR("ShaderPool: Failed to create vertex shader '%ls'", Key.SourcePath.c_str());
		SafeRelease(Bytecode);
		return nullptr;
	}

	// Create input layout if requested
	ComPtr<ID3D11InputLayout> InputLayout;
	if (OutInputLayout && LayoutDescs && !LayoutDescs->empty())
	{
		hr = URenderer::GetInstance().GetDevice()->CreateInputLayout(
			LayoutDescs->data(),
			static_cast<UINT>(LayoutDescs->size()),
			Bytecode->GetBufferPointer(),
			Bytecode->GetBufferSize(),
			&InputLayout);

		if (SUCCEEDED(hr))
		{
			InputLayout.CopyTo(OutInputLayout);
		}
	}

	SafeRelease(Bytecode);

	// Add to cache
	FCachedVertexShader Cached;
	Cached.Shader = VS;
	Cached.InputLayout = InputLayout;
	Cached.LastCompileTime = GetFileTimestamp(Key.SourcePath);

	VSCache[Key] = Cached;

	UE_LOG("ShaderPool: Compiled and cached VS '%ls'", Key.SourcePath.c_str());

	// Return AddRef'd pointer
	ID3D11VertexShader* Result = nullptr;
	VS.CopyTo(&Result);
	return Result;
}

ID3D11PixelShader* FShaderPool::GetOrCompilePS(const FShaderKey& Key)
{
	// Check if already cached
	auto It = PSCache.find(Key);
	if (It != PSCache.end())
	{
		FCachedPixelShader& Cached = It->second;

		// Return AddRef'd pointer
		ID3D11PixelShader* PS = nullptr;
		Cached.Shader.CopyTo(&PS);

		UE_LOG("ShaderPool: GetOrCompilePS cache hit '%ls'", Key.SourcePath.c_str());
		return PS;
	}

	// Not cached - compile or load
	ID3DBlob* Bytecode = nullptr;
	if (!CompileOrLoadShader(Key, &Bytecode))
	{
		UE_LOG_ERROR("ShaderPool: Failed to compile/load PS '%ls'", Key.SourcePath.c_str());
		return nullptr;
	}

	// Create D3D11 pixel shader
	ComPtr<ID3D11PixelShader> PS;
	HRESULT hr = URenderer::GetInstance().GetDevice()->CreatePixelShader(
		Bytecode->GetBufferPointer(),
		Bytecode->GetBufferSize(),
		nullptr,
		&PS);

	SafeRelease(Bytecode);

	if (FAILED(hr))
	{
		UE_LOG_ERROR("ShaderPool: Failed to create pixel shader '%ls'", Key.SourcePath.c_str());
		return nullptr;
	}

	// Add to cache
	FCachedPixelShader Cached;
	Cached.Shader = PS;
	Cached.LastCompileTime = GetFileTimestamp(Key.SourcePath);

	PSCache[Key] = Cached;

	UE_LOG("ShaderPool: Compiled and cached PS '%ls'", Key.SourcePath.c_str());

	// Return AddRef'd pointer
	ID3D11PixelShader* Result = nullptr;
	PS.CopyTo(&Result);
	return Result;
}

ID3D11ComputeShader* FShaderPool::GetOrCompileCS(
	const FShaderKey& Key,
	const char* Entry,
	const char* Profile)
{
	// Check if already cached
	auto It = CSCache.find(Key);
	if (It != CSCache.end())
	{
		FCachedComputeShader& Cached = It->second;

		// Return AddRef'd pointer
		ID3D11ComputeShader* CS = nullptr;
		Cached.Shader.CopyTo(&CS);

		UE_LOG("ShaderPool: GetOrCompileCS cache hit '%ls'", Key.SourcePath.c_str());
		return CS;
	}

	// Not cached - compile or load
	ID3DBlob* Bytecode = nullptr;
	if (!CompileOrLoadShader(Key, &Bytecode))
	{
		UE_LOG_ERROR("ShaderPool: Failed to compile/load CS '%ls'", Key.SourcePath.c_str());
		return nullptr;
	}

	// Create D3D11 compute shader
	ComPtr<ID3D11ComputeShader> CS;
	HRESULT hr = URenderer::GetInstance().GetDevice()->CreateComputeShader(
		Bytecode->GetBufferPointer(),
		Bytecode->GetBufferSize(),
		nullptr,
		&CS);

	SafeRelease(Bytecode);

	if (FAILED(hr))
	{
		UE_LOG_ERROR("ShaderPool: Failed to create compute shader '%ls'", Key.SourcePath.c_str());
		return nullptr;
	}

	// Add to cache
	FCachedComputeShader Cached;
	Cached.Shader = CS;
	Cached.LastCompileTime = GetFileTimestamp(Key.SourcePath);

	CSCache[Key] = Cached;

	UE_LOG("ShaderPool: Compiled and cached CS '%ls'", Key.SourcePath.c_str());

	// Return AddRef'd pointer
	ID3D11ComputeShader* Result = nullptr;
	CS.CopyTo(&Result);
	return Result;
}

void* FShaderPool::RecompileShader(const FShaderKey& Key)
{
	// Determine shader type and recompile
	switch (Key.Type)
	{
	case EShaderType::VertexShader:
	{
		auto It = VSCache.find(Key);
		if (It == VSCache.end())
		{
			UE_LOG_WARNING("ShaderPool: Cannot recompile non-cached VS '%ls'", Key.SourcePath.c_str());
			return nullptr;
		}

		// Compile new shader
		ID3DBlob* Bytecode = nullptr;
		if (!CompileOrLoadShader(Key, &Bytecode))
		{
			UE_LOG_ERROR("ShaderPool: Failed to recompile VS '%ls'", Key.SourcePath.c_str());
			return nullptr;
		}

		ComPtr<ID3D11VertexShader> NewVS;
		HRESULT hr = URenderer::GetInstance().GetDevice()->CreateVertexShader(
			Bytecode->GetBufferPointer(),
			Bytecode->GetBufferSize(),
			nullptr,
			&NewVS);

		FILETIME Timestamp = GetFileTimestamp(Key.SourcePath);
		BinaryCache.SaveToCache(Key, Bytecode->GetBufferPointer(), Bytecode->GetBufferSize(), Timestamp);
		SafeRelease(Bytecode);

		if (FAILED(hr))
		{
			UE_LOG_ERROR("ShaderPool: Failed to create new VS during recompile");
			return nullptr;
		}

		// Update cache (ComPtr automatically releases old shader)
		It->second.Shader = NewVS;
		It->second.LastCompileTime = Timestamp;

		UE_LOG("ShaderPool: Successfully recompiled VS '%ls'", Key.SourcePath.c_str());

		// Return AddRef'd pointer
		ID3D11VertexShader* Result = nullptr;
		NewVS.CopyTo(&Result);
		return Result;
	}

	case EShaderType::PixelShader:
	{
		auto It = PSCache.find(Key);
		if (It == PSCache.end())
		{
			UE_LOG_WARNING("ShaderPool: Cannot recompile non-cached PS '%ls'", Key.SourcePath.c_str());
			return nullptr;
		}

		ID3DBlob* Bytecode = nullptr;
		if (!CompileOrLoadShader(Key, &Bytecode))
		{
			UE_LOG_ERROR("ShaderPool: Failed to recompile PS '%ls'", Key.SourcePath.c_str());
			return nullptr;
		}

		ComPtr<ID3D11PixelShader> NewPS;
		HRESULT hr = URenderer::GetInstance().GetDevice()->CreatePixelShader(
			Bytecode->GetBufferPointer(),
			Bytecode->GetBufferSize(),
			nullptr,
			&NewPS);

		FILETIME Timestamp = GetFileTimestamp(Key.SourcePath);
		BinaryCache.SaveToCache(Key, Bytecode->GetBufferPointer(), Bytecode->GetBufferSize(), Timestamp);
		SafeRelease(Bytecode);

		if (FAILED(hr))
		{
			UE_LOG_ERROR("ShaderPool: Failed to create new PS during recompile");
			return nullptr;
		}

		// Update cache
		It->second.Shader = NewPS;
		It->second.LastCompileTime = Timestamp;

		UE_LOG("ShaderPool: Successfully recompiled PS '%ls'", Key.SourcePath.c_str());

		ID3D11PixelShader* Result = nullptr;
		NewPS.CopyTo(&Result);
		return Result;
	}

	case EShaderType::ComputeShader:
	{
		auto It = CSCache.find(Key);
		if (It == CSCache.end())
		{
			UE_LOG_WARNING("ShaderPool: Cannot recompile non-cached CS '%ls'", Key.SourcePath.c_str());
			return nullptr;
		}

		ID3DBlob* Bytecode = nullptr;
		if (!CompileOrLoadShader(Key, &Bytecode))
		{
			UE_LOG_ERROR("ShaderPool: Failed to recompile CS '%ls'", Key.SourcePath.c_str());
			return nullptr;
		}

		ComPtr<ID3D11ComputeShader> NewCS;
		HRESULT hr = URenderer::GetInstance().GetDevice()->CreateComputeShader(
			Bytecode->GetBufferPointer(),
			Bytecode->GetBufferSize(),
			nullptr,
			&NewCS);

		FILETIME Timestamp = GetFileTimestamp(Key.SourcePath);
		BinaryCache.SaveToCache(Key, Bytecode->GetBufferPointer(), Bytecode->GetBufferSize(), Timestamp);
		SafeRelease(Bytecode);

		if (FAILED(hr))
		{
			UE_LOG_ERROR("ShaderPool: Failed to create new CS during recompile");
			return nullptr;
		}

		// Update cache
		It->second.Shader = NewCS;
		It->second.LastCompileTime = Timestamp;

		UE_LOG("ShaderPool: Successfully recompiled CS '%ls'", Key.SourcePath.c_str());

		ID3D11ComputeShader* Result = nullptr;
		NewCS.CopyTo(&Result);
		return Result;
	}
	}

	return nullptr;
}

size_t FShaderPool::GetCachedShaderCount() const
{
	return VSCache.size() + PSCache.size() + CSCache.size();
}

int32 FShaderPool::PrecompileAllShaders()
{
	// TODO: Scan shader directory and precompile all variants
	UE_LOG("ShaderPool: PrecompileAllShaders not yet implemented");
	return 0;
}

bool FShaderPool::CompileOrLoadShader(const FShaderKey& Key, ID3DBlob** OutBytecode)
{
	// Try load from binary cache first
	FILETIME SourceTimestamp = GetFileTimestamp(Key.SourcePath);
	FShaderCacheEntry CacheEntry;

	if (BinaryCache.LoadFromCache(Key, CacheEntry))
	{
		// Create blob from cached bytecode
		HRESULT hr = D3DCreateBlob(CacheEntry.Bytecode.size(), OutBytecode);
		if (SUCCEEDED(hr))
		{
			memcpy((*OutBytecode)->GetBufferPointer(), CacheEntry.Bytecode.data(), CacheEntry.Bytecode.size());
			UE_LOG("ShaderPool: Loaded shader from cache '%ls'", Key.SourcePath.c_str());
			return true;
		}
	}

	// Cache miss or invalid - compile from source
	const char* EntryPoint = "main";
	const char* Profile = "vs_5_0";

	switch (Key.Type)
	{
	case EShaderType::VertexShader:
		EntryPoint = "mainVS";
		Profile = "vs_5_0";
		break;
	case EShaderType::PixelShader:
		EntryPoint = "mainPS";
		Profile = "ps_5_0";
		break;
	case EShaderType::ComputeShader:
		EntryPoint = "main";
		Profile = "cs_5_0";
		break;
	}

	if (!CompileFromSource(Key, EntryPoint, Profile, OutBytecode))
	{
		return false;
	}

	// Save to cache
	BinaryCache.SaveToCache(Key, (*OutBytecode)->GetBufferPointer(), (*OutBytecode)->GetBufferSize(), SourceTimestamp);

	return true;
}

bool FShaderPool::CompileFromSource(
	const FShaderKey& Key,
	const char* EntryPoint,
	const char* Profile,
	ID3DBlob** OutBytecode)
{
	// Convert FShaderDefine to D3D_SHADER_MACRO
	TArray<D3D_SHADER_MACRO> Macros = ConvertToD3DMacros(Key.Defines);

	ID3DBlob* ErrorBlob = nullptr;
	UINT Flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
	Flags |= D3DCOMPILE_DEBUG;
	Flags |= D3DCOMPILE_OPTIMIZATION_LEVEL0;  // 디버깅 편의성 위해, 최소 최적화 적용: 최적화를 완전히 끄면 물체가 빛을 받을 때, 이상한 artifacts가 생김
#endif

	HRESULT hr = D3DCompileFromFile(
		Key.SourcePath.c_str(),
		Macros.data(),
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		EntryPoint,
		Profile,
		Flags,
		0,
		OutBytecode,
		&ErrorBlob);

	if (FAILED(hr))
	{
		if (ErrorBlob)
		{
			UE_LOG_ERROR("ShaderPool: Compile error: %s", static_cast<char*>(ErrorBlob->GetBufferPointer()));
			SafeRelease(ErrorBlob);
		}
		return false;
	}

	UE_LOG("ShaderPool: Compiled shader from source '%ls'", Key.SourcePath.c_str());
	return true;
}

TArray<D3D_SHADER_MACRO> FShaderPool::ConvertToD3DMacros(const TArray<FShaderDefine>& Defines)
{
	TArray<D3D_SHADER_MACRO> Result;

	for (const FShaderDefine& Define : Defines)
	{
		D3D_SHADER_MACRO Macro;
		Macro.Name = Define.Name.c_str();
		Macro.Definition = Define.Definition.c_str();
		Result.push_back(Macro);
	}

	// Null terminator
	Result.push_back({ nullptr, nullptr });

	return Result;
}

FILETIME FShaderPool::GetFileTimestamp(const wstring& FilePath) const
{
	FILETIME Result = {};

	HANDLE FileHandle = CreateFileW(
		FilePath.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		GetFileTime(FileHandle, nullptr, nullptr, &Result);
		CloseHandle(FileHandle);
	}

	return Result;
}
