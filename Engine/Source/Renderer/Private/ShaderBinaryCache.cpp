#include "pch.h"
#include "Renderer/Public/ShaderBinaryCache.h"
#include <fstream>
#include <filesystem>
#include <wincrypt.h>

#pragma comment(lib, "crypt32.lib")

FShaderBinaryCache::FShaderBinaryCache()
	: CacheDirectory(L"Intermediate/ShaderCache")
{
}

FShaderBinaryCache::~FShaderBinaryCache()
{
}

void FShaderBinaryCache::Initialize(const wstring& InCacheDirectory)
{
	CacheDirectory = InCacheDirectory;
	EnsureCacheDirectoryExists();
	UE_LOG("ShaderBinaryCache: Initialized at '%ls'", CacheDirectory.c_str());
}

bool FShaderBinaryCache::SaveToCache(
	const FShaderKey& Key,
	const void* Bytecode,
	size_t BytecodeSize,
	const FILETIME& ShaderFolderTimestamp,
	uint32 CompileFlags)
{
	// Build cache entry
	FShaderCacheEntry Entry;
	Entry.Header.ShaderType = static_cast<uint32>(Key.Type);

	// Calculate source hash
	if (!CalculateSourceHash(Key, Entry.Header.MD5Hash))
	{
		UE_LOG_ERROR("ShaderBinaryCache: Failed to calculate hash for cache save");
		return false;
	}

	// Fill metadata
	Entry.Metadata.SourcePath = Key.SourcePath;
	Entry.Metadata.Defines = Key.Defines;
	Entry.Metadata.ShaderFolderTimestamp = ShaderFolderTimestamp;
	Entry.Metadata.BytecodeSize = static_cast<uint32>(BytecodeSize);
	Entry.Metadata.CompileFlags = CompileFlags;

	// Copy bytecode
	Entry.Bytecode.resize(BytecodeSize);
	memcpy(Entry.Bytecode.data(), Bytecode, BytecodeSize);

	// Write to file
	wstring FilePath = GetCacheFilePath(Key);
	if (WriteEntryToFile(FilePath, Entry))
	{
		UE_LOG("ShaderBinaryCache: Saved '%ls' (%zu bytes)", FilePath.c_str(), BytecodeSize);
		return true;
	}

	return false;
}

bool FShaderBinaryCache::LoadFromCache(
	const FShaderKey& Key,
	const FILETIME& ShaderFolderTimestamp,
	uint32 CompileFlags,
	FShaderCacheEntry& OutEntry)
{
	wstring FilePath = GetCacheFilePath(Key);

	// Check if file exists
	if (!std::filesystem::exists(FilePath))
	{
		return false;
	}

	// Step 1: Read cache file (ONLY ONCE - DRY principle)
	if (!ReadEntryFromFile(FilePath, OutEntry))
	{
		UE_LOG_WARNING("ShaderBinaryCache: Failed to read '%ls'", FilePath.c_str());
		return false;
	}

	// Step 2: Validate cache entry (Single validation point - SRP principle)
	if (!ValidateCacheEntry(OutEntry, Key, ShaderFolderTimestamp, CompileFlags))
	{
		// Validation method already logs specific reason
		return false;
	}

	UE_LOG("ShaderBinaryCache: Successfully loaded and validated '%ls' (%u bytes)",
		FilePath.c_str(), OutEntry.Metadata.BytecodeSize);
	return true;
}

bool FShaderBinaryCache::CalculateSourceHash(
	const FShaderKey& Key,
	uint8 OutHash[16])
{
	// Read source file content
	std::ifstream File(Key.SourcePath, std::ios::binary);
	if (!File.is_open())
	{
		UE_LOG_ERROR("ShaderBinaryCache: Cannot open source file '%ls' for hashing", Key.SourcePath.c_str());
		return false;
	}

	// Get file size
	File.seekg(0, std::ios::end);
	size_t FileSize = File.tellg();
	File.seekg(0, std::ios::beg);

	// Read content
	TArray<char> Content(FileSize);
	File.read(Content.data(), FileSize);
	File.close();

	// Append defines to content for hashing
	string DefinesStr;
	for (const FShaderDefine& Define : Key.Defines)
	{
		DefinesStr += Define.Name + "=" + Define.Definition + ";";
	}

	// Calculate MD5
	HCRYPTPROV hProv = 0;
	HCRYPTHASH hHash = 0;

	if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		return false;
	}

	if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
	{
		CryptReleaseContext(hProv, 0);
		return false;
	}

	// Hash source content
	CryptHashData(hHash, reinterpret_cast<const BYTE*>(Content.data()), static_cast<DWORD>(Content.size()), 0);

	// Hash defines
	if (!DefinesStr.empty())
	{
		CryptHashData(hHash, reinterpret_cast<const BYTE*>(DefinesStr.c_str()), static_cast<DWORD>(DefinesStr.size()), 0);
	}

	// Get hash value
	DWORD HashLen = 16;
	CryptGetHashParam(hHash, HP_HASHVAL, OutHash, &HashLen, 0);

	CryptDestroyHash(hHash);
	CryptReleaseContext(hProv, 0);

	return true;
}

wstring FShaderBinaryCache::GetCacheFilePath(const FShaderKey& Key) const
{
	return CacheDirectory + L"/" + Key.GetCacheFileName();
}

void FShaderBinaryCache::ClearCache()
{
	try
	{
		std::filesystem::remove_all(CacheDirectory);
		EnsureCacheDirectoryExists();
		UE_LOG("ShaderBinaryCache: Cache cleared");
	}
	catch (const std::exception& e)
	{
		UE_LOG_ERROR("ShaderBinaryCache: Failed to clear cache: %s", e.what());
	}
}

FILETIME FShaderBinaryCache::GetShaderFolderTimestamp(const wstring& ShaderFolderPath) const
{
	FILETIME LatestTimestamp = {};
	LatestTimestamp.dwLowDateTime = 0;
	LatestTimestamp.dwHighDateTime = 0;

	try
	{
		// Iterate all .hlsl files in the folder
		for (const auto& Entry : std::filesystem::recursive_directory_iterator(ShaderFolderPath))
		{
			if (Entry.is_regular_file())
			{
				wstring Extension = Entry.path().extension().wstring();
				if (Extension == L".hlsl")
				{
					FILETIME FileTime = GetFileTimestamp(Entry.path().wstring());

					// Update if this file is newer
					if (IsFileTimeNewer(FileTime, LatestTimestamp))
					{
						LatestTimestamp = FileTime;
					}
				}
			}
		}
	}
	catch (const std::exception& e)
	{
		UE_LOG_ERROR("ShaderBinaryCache: Failed to scan shader folder '%ls': %s",
			ShaderFolderPath.c_str(), e.what());
	}

	return LatestTimestamp;
}

bool FShaderBinaryCache::EnsureCacheDirectoryExists()
{
	try
	{
		if (!std::filesystem::exists(CacheDirectory))
		{
			std::filesystem::create_directories(CacheDirectory);
			UE_LOG("ShaderBinaryCache: Created cache directory '%ls'", CacheDirectory.c_str());
		}
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG_ERROR("ShaderBinaryCache: Failed to create cache directory: %s", e.what());
		return false;
	}
}

FILETIME FShaderBinaryCache::GetFileTimestamp(const wstring& FilePath) const
{
	FILETIME Result = {};

	HANDLE FileHandle = CreateFileW(
		FilePath.c_str(),
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

bool FShaderBinaryCache::IsFileTimeNewer(const FILETIME& Time1, const FILETIME& Time2) const
{
	ULARGE_INTEGER t1, t2;
	t1.LowPart = Time1.dwLowDateTime;
	t1.HighPart = Time1.dwHighDateTime;
	t2.LowPart = Time2.dwLowDateTime;
	t2.HighPart = Time2.dwHighDateTime;

	return t1.QuadPart > t2.QuadPart;
}

bool FShaderBinaryCache::WriteEntryToFile(const wstring& FilePath, const FShaderCacheEntry& Entry)
{
	std::ofstream File(FilePath, std::ios::binary);
	if (!File.is_open())
	{
		UE_LOG_ERROR("ShaderBinaryCache: Cannot write to '%ls'", FilePath.c_str());
		return false;
	}

	try
	{
		// Write header
		File.write(reinterpret_cast<const char*>(&Entry.Header), sizeof(FShaderCacheHeader));

		// Write metadata
		// - Source path length + data
		uint32 PathLen = static_cast<uint32>(Entry.Metadata.SourcePath.size());
		File.write(reinterpret_cast<const char*>(&PathLen), sizeof(uint32));
		File.write(reinterpret_cast<const char*>(Entry.Metadata.SourcePath.c_str()), PathLen * sizeof(wchar_t));

		// - Defines count + data
		uint32 DefinesCount = static_cast<uint32>(Entry.Metadata.Defines.size());
		File.write(reinterpret_cast<const char*>(&DefinesCount), sizeof(uint32));
		for (const FShaderDefine& Define : Entry.Metadata.Defines)
		{
			uint32 NameLen = static_cast<uint32>(Define.Name.size());
			uint32 DefLen = static_cast<uint32>(Define.Definition.size());

			File.write(reinterpret_cast<const char*>(&NameLen), sizeof(uint32));
			File.write(Define.Name.c_str(), NameLen);

			File.write(reinterpret_cast<const char*>(&DefLen), sizeof(uint32));
			File.write(Define.Definition.c_str(), DefLen);
		}

		// - Shader folder timestamp
		File.write(reinterpret_cast<const char*>(&Entry.Metadata.ShaderFolderTimestamp), sizeof(FILETIME));

		// - Bytecode size
		File.write(reinterpret_cast<const char*>(&Entry.Metadata.BytecodeSize), sizeof(uint32));

		// - Compile flags
		File.write(reinterpret_cast<const char*>(&Entry.Metadata.CompileFlags), sizeof(uint32));

		// Write bytecode
		File.write(reinterpret_cast<const char*>(Entry.Bytecode.data()), Entry.Bytecode.size());

		File.close();
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG_ERROR("ShaderBinaryCache: Exception writing file: %s", e.what());
		return false;
	}
}

bool FShaderBinaryCache::ReadEntryFromFile(const wstring& FilePath, FShaderCacheEntry& OutEntry)
{
	std::ifstream File(FilePath, std::ios::binary);
	if (!File.is_open())
	{
		return false;
	}

	try
	{
		// Read header
		File.read(reinterpret_cast<char*>(&OutEntry.Header), sizeof(FShaderCacheHeader));
		if (!OutEntry.Header.IsValid())
		{
			return false;
		}

		// Read metadata
		// - Source path
		uint32 PathLen = 0;
		File.read(reinterpret_cast<char*>(&PathLen), sizeof(uint32));
		OutEntry.Metadata.SourcePath.resize(PathLen);
		File.read(reinterpret_cast<char*>(OutEntry.Metadata.SourcePath.data()), PathLen * sizeof(wchar_t));

		// - Defines
		uint32 DefinesCount = 0;
		File.read(reinterpret_cast<char*>(&DefinesCount), sizeof(uint32));
		OutEntry.Metadata.Defines.clear();
		for (uint32 i = 0; i < DefinesCount; ++i)
		{
			FShaderDefine Define;

			uint32 NameLen = 0, DefLen = 0;
			File.read(reinterpret_cast<char*>(&NameLen), sizeof(uint32));
			Define.Name.resize(NameLen);
			File.read(&Define.Name[0], NameLen);

			File.read(reinterpret_cast<char*>(&DefLen), sizeof(uint32));
			Define.Definition.resize(DefLen);
			File.read(&Define.Definition[0], DefLen);

			OutEntry.Metadata.Defines.push_back(Define);
		}

		// - Shader folder timestamp
		File.read(reinterpret_cast<char*>(&OutEntry.Metadata.ShaderFolderTimestamp), sizeof(FILETIME));

		// - Bytecode size
		File.read(reinterpret_cast<char*>(&OutEntry.Metadata.BytecodeSize), sizeof(uint32));

		// - Compile flags
		File.read(reinterpret_cast<char*>(&OutEntry.Metadata.CompileFlags), sizeof(uint32));

		// Read bytecode
		OutEntry.Bytecode.resize(OutEntry.Metadata.BytecodeSize);
		File.read(reinterpret_cast<char*>(OutEntry.Bytecode.data()), OutEntry.Metadata.BytecodeSize);

		File.close();
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG_ERROR("ShaderBinaryCache: Exception reading file: %s", e.what());
		return false;
	}
}

// ===== Validation Methods (SOLID: Single Responsibility Principle) =====

bool FShaderBinaryCache::ValidateCacheEntry(
	const FShaderCacheEntry& Entry,
	const FShaderKey& Key,
	const FILETIME& ShaderFolderTimestamp,
	uint32 CompileFlags) const
{
	// Early-return pattern: Fast failure on first invalid check

	if (!ValidateHeader(Entry.Header))
		return false;

	if (!ValidateSourceHash(Entry, Key))
		return false;

	if (!ValidateTimestamp(Entry, ShaderFolderTimestamp))
		return false;

	if (!ValidateCompileFlags(Entry, CompileFlags))
		return false;

	return true;
}

bool FShaderBinaryCache::ValidateHeader(const FShaderCacheHeader& Header) const
{
	if (!Header.IsValid())
	{
		UE_LOG_WARNING("ShaderBinaryCache: Invalid cache header (magic/version mismatch)");
		return false;
	}
	return true;
}

bool FShaderBinaryCache::ValidateSourceHash(const FShaderCacheEntry& Entry, const FShaderKey& Key) const
{
	uint8 CurrentHash[16];
	if (!const_cast<FShaderBinaryCache*>(this)->CalculateSourceHash(Key, CurrentHash))
	{
		UE_LOG_WARNING("ShaderBinaryCache: Failed to calculate source hash for validation");
		return false;
	}

	if (memcmp(CurrentHash, Entry.Header.MD5Hash, 16) != 0)
	{
		UE_LOG("ShaderBinaryCache: Hash mismatch for '%ls' (source or defines changed)", Key.SourcePath.c_str());
		return false;
	}

	return true;
}

bool FShaderBinaryCache::ValidateTimestamp(const FShaderCacheEntry& Entry, const FILETIME& ShaderFolderTimestamp) const
{
	if (IsFileTimeNewer(ShaderFolderTimestamp, Entry.Metadata.ShaderFolderTimestamp))
	{
		UE_LOG("ShaderBinaryCache: Cache outdated (shader folder modified since cache creation)");
		return false;
	}
	return true;
}

bool FShaderBinaryCache::ValidateCompileFlags(const FShaderCacheEntry& Entry, uint32 CompileFlags) const
{
	if (Entry.Metadata.CompileFlags != CompileFlags)
	{
		UE_LOG("ShaderBinaryCache: Compile flags changed (cached: 0x%X, current: 0x%X)",
			Entry.Metadata.CompileFlags, CompileFlags);
		return false;
	}
	return true;
}
