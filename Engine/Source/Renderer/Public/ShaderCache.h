#pragma once
#include "Core/Public/CoreTypes.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>

using namespace std;

/**
 * @brief Shader type enumeration
 */
enum class EShaderType : uint8
{
	VertexShader,
	PixelShader,
	ComputeShader
};

/**
 * @brief Preprocessor define for shader compilation
 *
 * Replacement for D3D_SHADER_MACRO with ownership semantics
 */
struct FShaderDefine
{
	string Name;         ///< Macro name (e.g., "LIGHTING_MODEL_PHONG")
	string Definition;   ///< Macro value (e.g., "1")

	FShaderDefine() = default;
	FShaderDefine(const string& InName, const string& InDefinition)
		: Name(InName), Definition(InDefinition) {}

	/**
	 * @brief Construct from D3D_SHADER_MACRO
	 */
	static FShaderDefine FromD3DMacro(const D3D_SHADER_MACRO& Macro)
	{
		return FShaderDefine(
			Macro.Name ? string(Macro.Name) : "",
			Macro.Definition ? string(Macro.Definition) : ""
		);
	}

	bool operator==(const FShaderDefine& Other) const
	{
		return Name == Other.Name && Definition == Other.Definition;
	}

	bool operator<(const FShaderDefine& Other) const
	{
		if (Name != Other.Name) return Name < Other.Name;
		return Definition < Other.Definition;
	}
};

/**
 * @brief Unique key for identifying shader variants
 *
 * Represents a unique shader compilation: (source file + preprocessor defines + type)
 * Used for caching, hash lookups, and determining when recompilation is needed.
 *
 * Example:
 *   - Key1: TextureVS.hlsl + LIGHTING_MODEL_PHONG + VertexShader
 *   - Key2: TextureVS.hlsl + LIGHTING_MODEL_GOURAUD + VertexShader
 *   → Different keys, compiled separately
 */
struct FShaderKey
{
	wstring SourcePath;              ///< Path to .hlsl file (e.g., L"Asset/Shader/TextureVS.hlsl")
	TArray<FShaderDefine> Defines;   ///< Preprocessor macros, sorted for consistency
	EShaderType Type;                ///< Shader stage type

	FShaderKey()
		: Type(EShaderType::VertexShader)
	{}

	FShaderKey(const wstring& InPath, const TArray<FShaderDefine>& InDefines, EShaderType InType)
		: SourcePath(InPath), Defines(InDefines), Type(InType)
	{
		// Sort defines for consistent hashing
		std::sort(Defines.begin(), Defines.end());
	}

	/**
	 * @brief Create key from D3D_SHADER_MACRO array
	 */
	static FShaderKey FromD3DMacros(const wstring& InPath, const D3D_SHADER_MACRO* InDefines, EShaderType InType)
	{
		TArray<FShaderDefine> Defines;
		if (InDefines)
		{
			for (int i = 0; InDefines[i].Name != nullptr; ++i)
			{
				Defines.push_back(FShaderDefine::FromD3DMacro(InDefines[i]));
			}
		}
		return FShaderKey(InPath, Defines, InType);
	}

	/**
	 * @brief Calculate hash for this key
	 * @return 64-bit hash value
	 */
	size_t GetHash() const
	{
		size_t Hash = 0;

		// Hash source path
		Hash ^= std::hash<wstring>{}(SourcePath) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);

		// Hash shader type
		Hash ^= static_cast<size_t>(Type) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);

		// Hash defines
		for (const FShaderDefine& Define : Defines)
		{
			Hash ^= std::hash<string>{}(Define.Name) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
			Hash ^= std::hash<string>{}(Define.Definition) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
		}

		return Hash;
	}

	/**
	 * @brief Generate cache file name for this shader variant
	 * @return Filename like "TextureVS_PHONG_1A2B3C4D.cso"
	 */
	wstring GetCacheFileName() const
	{
		// Extract base filename without extension
		size_t LastSlash = SourcePath.find_last_of(L"/\\");
		size_t LastDot = SourcePath.find_last_of(L'.');
		wstring BaseName = SourcePath.substr(LastSlash + 1, LastDot - LastSlash - 1);

		// Append defines abbreviation
		wstring DefinesSuffix;
		for (const FShaderDefine& Define : Defines)
		{
			if (!DefinesSuffix.empty()) DefinesSuffix += L"_";

			// Extract key part (e.g., "LIGHTING_MODEL_PHONG" → "PHONG")
			string Name = Define.Name;
			size_t LastUnderscore = Name.find_last_of('_');
			string ShortName = (LastUnderscore != string::npos) ? Name.substr(LastUnderscore + 1) : Name;

			DefinesSuffix += wstring(ShortName.begin(), ShortName.end());
		}

		// Append hash
		wchar_t HashStr[17];
		swprintf_s(HashStr, L"%016zX", GetHash() & 0xFFFFFFFF); // Use lower 32 bits for readability

		// Build filename: "TextureVS_PHONG_1A2B3C4D.cso"
		wstring FileName = BaseName;
		if (!DefinesSuffix.empty())
		{
			FileName += L"_" + DefinesSuffix;
		}
		FileName += L"_" + wstring(HashStr);
		FileName += L".cso";

		return FileName;
	}

	bool operator==(const FShaderKey& Other) const
	{
		return Type == Other.Type &&
		       SourcePath == Other.SourcePath &&
		       Defines == Other.Defines;
	}
};

/**
 * @brief Hash functor for FShaderKey (for use with TMap)
 */
struct FShaderKeyHasher
{
	size_t operator()(const FShaderKey& Key) const noexcept
	{
		return Key.GetHash();
	}
};

/**
 * @brief Header for compiled shader cache files (.cso)
 *
 * File format:
 * [Header][Metadata][Bytecode]
 */
struct FShaderCacheHeader
{
	static constexpr uint64 MAGIC_NUMBER = 0x4F53435F4C544B00; // "KTL_CSO\0"
	static constexpr uint32 VERSION = 1;

	uint64 Magic;          ///< Magic number for validation
	uint32 Version;        ///< File format version
	uint32 ShaderType;     ///< EShaderType as uint32
	uint8 MD5Hash[16];     ///< MD5 checksum of source + defines

	FShaderCacheHeader()
		: Magic(MAGIC_NUMBER)
		, Version(VERSION)
		, ShaderType(0)
	{
		memset(MD5Hash, 0, sizeof(MD5Hash));
	}

	bool IsValid() const
	{
		return Magic == MAGIC_NUMBER && Version == VERSION;
	}
};

/**
 * @brief Metadata stored in shader cache file
 */
struct FShaderCacheMetadata
{
	wstring SourcePath;                 ///< Original .hlsl file path
	TArray<FShaderDefine> Defines;      ///< Preprocessor defines used
	FILETIME CompileTimestamp;          ///< When this was compiled
	uint32 BytecodeSize;                ///< Size of shader bytecode

	FShaderCacheMetadata()
		: BytecodeSize(0)
	{
		CompileTimestamp.dwLowDateTime = 0;
		CompileTimestamp.dwHighDateTime = 0;
	}
};

/**
 * @brief Complete cached shader data
 */
struct FShaderCacheEntry
{
	FShaderCacheHeader Header;
	FShaderCacheMetadata Metadata;
	TArray<uint8> Bytecode;  ///< Compiled shader bytecode

	FShaderCacheEntry() = default;
};
