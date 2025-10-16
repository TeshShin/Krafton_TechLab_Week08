#pragma once
#include "Core/Public/CoreTypes.h"
#include "Core/Public/Types.h"
#include "Asset/Public/StaticMesh.h"
#include "Asset/Public/ObjImporter.h"
#include <memory>

class FObjManager
{
public:
	static FStaticMesh* LoadObjStaticMeshAsset(const FName& PathFileName, const FObjImporter::Configuration& Config = {});
	static UStaticMesh* LoadObjStaticMesh(const FName& PathFileName, const FObjImporter::Configuration& Config = {});
	static void CreateMaterialsFromMTL(UStaticMesh* StaticMesh, FStaticMesh* StaticMeshAsset, const FName& ObjFilePath);

	static constexpr size_t INVALID_INDEX = SIZE_MAX;
	
private:
	static TMap<FName, std::unique_ptr<FStaticMesh>> ObjFStaticMeshMap;
};
