#include "pch.h"
#include "Renderer/Public/ShaderFactory.h"
#include "Renderer/Public/ShaderManager.h"

namespace ShaderFactory
{
	TArray<FShaderDefine> ConvertMacrosToDefines(const D3D_SHADER_MACRO* InDefines)
	{
		TArray<FShaderDefine> Result;

		if (InDefines)
		{
			for (int i = 0; InDefines[i].Name != nullptr; ++i)
			{
				Result.push_back(FShaderDefine(
					string(InDefines[i].Name),
					InDefines[i].Definition ? string(InDefines[i].Definition) : ""
				));
			}
		}

		return Result;
	}

	FShaderKey CreateShaderKey(const wstring& Path, const D3D_SHADER_MACRO* Defines, EShaderType Type)
	{
		TArray<FShaderDefine> DefineArray = ConvertMacrosToDefines(Defines);
		return FShaderKey(Path, DefineArray, Type);
	}

	ID3D11VertexShader* CreateVertexShader(
		const FShaderKey& Key,
		ID3D11InputLayout** OutInputLayout,
		const TArray<D3D11_INPUT_ELEMENT_DESC>* LayoutDescs,
		bool bEnableHotReload)
	{
		// Get or compile from pool (Flyweight pattern + binary caching)
		FShaderPool& Pool = FShaderManager::Get().GetPool();
		ID3D11VertexShader* VS = Pool.GetOrCompileVS(Key, OutInputLayout, LayoutDescs);

		if (!VS)
		{
			UE_LOG_ERROR("ShaderFactory: Failed to create vertex shader '%ls'", Key.SourcePath.c_str());
			return nullptr;
		}

		// NOTE: Hot-reload registration is done by the caller (RenderResourceFactory)
		// because we need the final destination pointer address, not the local variable address

		return VS;
	}

	ID3D11PixelShader* CreatePixelShader(
		const FShaderKey& Key,
		bool bEnableHotReload)
	{
		// Get or compile from pool
		FShaderPool& Pool = FShaderManager::Get().GetPool();
		ID3D11PixelShader* PS = Pool.GetOrCompilePS(Key);

		if (!PS)
		{
			UE_LOG_ERROR("ShaderFactory: Failed to create pixel shader '%ls'", Key.SourcePath.c_str());
			return nullptr;
		}

		// NOTE: Hot-reload registration is done by the caller (RenderResourceFactory)
		// because we need the final destination pointer address, not the local variable address

		return PS;
	}

	ID3D11ComputeShader* CreateComputeShader(
		const FShaderKey& Key,
		const char* Entry,
		const char* Profile,
		bool bEnableHotReload)
	{
		// Get or compile from pool
		FShaderPool& Pool = FShaderManager::Get().GetPool();
		ID3D11ComputeShader* CS = Pool.GetOrCompileCS(Key, Entry, Profile);

		if (!CS)
		{
			UE_LOG_ERROR("ShaderFactory: Failed to create compute shader '%ls'", Key.SourcePath.c_str());
			return nullptr;
		}

		// NOTE: Hot-reload registration is done by the caller (RenderResourceFactory)
		// because we need the final destination pointer address, not the local variable address

		return CS;
	}
}
