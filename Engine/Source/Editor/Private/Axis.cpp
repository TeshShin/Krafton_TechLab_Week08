#include "pch.h"
#include "Editor/Public/Axis.h"
#include "Renderer/Public/Renderer.h"
#include "Renderer/Public/RenderResourceFactory.h"

FAxis::FAxis()
{
	// UE x(forward) - X축 양의 방향 (짙은 빨간색)
	AxisVertices.push_back({ { 0.0f,0.0f,0.0f }, {}, { 1,0,0,1 }, {} });
	AxisVertices.push_back({ { 50000.0f,0.0f,0.0f }, {}, { 1,0,0,1 }, {} });

	// UE x(forward) - X축 음의 방향 (옅은 빨간색)
	AxisVertices.push_back({ { 0.0f,0.0f,0.0f }, {}, { 1.0f,0.5f,0.5f,1 }, {} });
	AxisVertices.push_back({ { -50000.0f,0.0f,0.0f }, {}, { 1.0f,0.5f,0.5f,1 }, {} });

	// UE y(right) - Y축 양의 방향 (짙은 초록색)
	AxisVertices.push_back({ { 0.0f,0.0f,0.0f }, {}, { 0,1,0,1 }, {} });
	AxisVertices.push_back({ { 0.0f,50000.0f,0.0f }, {}, { 0,1,0,1 }, {} });

	// UE y(right) - Y축 음의 방향 (옅은 초록색)
	AxisVertices.push_back({ { 0.0f,0.0f,0.0f }, {}, { 0.5f,1.0f,0.5f,1 }, {} });
	AxisVertices.push_back({ { 0.0f,-50000.0f,0.0f }, {}, { 0.5f,1.0f,0.5f,1 }, {} });

	// UE z(up) - Z축 양의 방향 (짙은 파란색)
	AxisVertices.push_back({ { 0.0f,0.0f,0.0f }, {}, { 0,0,1,1 }, {} });
	AxisVertices.push_back({ { 0.0f,0.0f,50000.0f }, {}, { 0,0,1,1 }, {} });

	// UE z(up) - Z축 음의 방향 (옅은 파란색)
	AxisVertices.push_back({ { 0.0f,0.0f,0.0f }, {}, { 0.5f,0.5f,1.0f,1 }, {} });
	AxisVertices.push_back({ { 0.0f,0.0f,-50000.0f }, {}, { 0.5f,0.5f,1.0f,1 }, {} });

	Primitive.NumVertices = static_cast<int>(AxisVertices.size());
	Primitive.VertexBuffer = FRenderResourceFactory::CreateVertexBuffer(AxisVertices);
	Primitive.Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	Primitive.Color = FVector4(1, 1, 1, 0);
	Primitive.Location = FVector(0, 0, 0);
	Primitive.Rotation = FQuaternion(0, 0, 0, 1);
	Primitive.Scale = FVector(1, 1, 1);
}

FAxis::~FAxis()
{
	SafeRelease(Primitive.VertexBuffer);
}

TArray<const FEditorPrimitive*> FAxis::GetEditorPrimitive() const
{
	return { &Primitive };
}
