#pragma once

struct FEditorPrimitive
{
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;
	uint32 NumVertices;
	uint32 NumIndices;
	D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	FVector4 Color;
	FVector Location;
	FQuaternion Rotation;
	FVector Scale;
};

class IEditorPrimitive
{
public:
	virtual ~IEditorPrimitive() = default;
	virtual TArray<const FEditorPrimitive*> GetEditorPrimitive() const = 0;
};
