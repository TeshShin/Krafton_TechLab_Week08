#include "pch.h"
#include "Editor/Public/Line/BatchLineManager.h"
#include "Renderer/Public/Renderer.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Editor/Public/Line/GridLineSource.h"
#include "Editor/Public/Line/BoundingBoxLineSource.h"
#include "Editor/Public/Line/DebugLineSource.h"
#include "Editor/Public/Line/ILineSource.h"
#include "Editor/Public/Line/LineVertex.h"

struct FBatchLineManagerData
{
    TArray<ILineSource*> Sources;
    FGridLineSource* GridSource = nullptr;
    FBoundingBoxLineSource* BoundingBoxSource = nullptr;
    FDebugLineSource* DebugSource = nullptr;

    FEditorPrimitive Primitive;
    bool bIsDirty = true;
};

IMPLEMENT_SINGLETON_CLASS(UBatchLineManager, UObject)

UBatchLineManager::UBatchLineManager()
{
}

UBatchLineManager::~UBatchLineManager()
{
}

void UBatchLineManager::Init()
{
    Data = new FBatchLineManagerData();

    Data->GridSource = new FGridLineSource();
    Data->Sources.push_back(Data->GridSource);

    Data->BoundingBoxSource = new FBoundingBoxLineSource();
    Data->Sources.push_back(Data->BoundingBoxSource);

    Data->DebugSource = new FDebugLineSource();
    Data->Sources.push_back(Data->DebugSource);

    // Create shaders and other resources here
    ID3D11VertexShader* VertexShader;
    ID3D11InputLayout* InputLayout;
    TArray<D3D11_INPUT_ELEMENT_DESC> Layout =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FLineVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FLineVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/BatchLineShader.hlsl", Layout, &VertexShader, &InputLayout);

    ID3D11PixelShader* PixelShader;
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/BatchLineShader.hlsl", &PixelShader);

    Data->Primitive.VertexShader = VertexShader;
    Data->Primitive.InputLayout = InputLayout;
    Data->Primitive.PixelShader = PixelShader;
    Data->Primitive.Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
}

void UBatchLineManager::Release()
{
    SafeDelete(Data->GridSource);
    SafeDelete(Data->BoundingBoxSource);
    SafeDelete(Data->DebugSource);

    SafeRelease(Data->Primitive.VertexShader);
    SafeRelease(Data->Primitive.InputLayout);
    SafeRelease(Data->Primitive.PixelShader);
    SafeRelease(Data->Primitive.VertexBuffer);
    SafeRelease(Data->Primitive.IndexBuffer);

    delete Data;
    Data = nullptr;
}

void UBatchLineManager::AddDebugLine(const FName& InLabel, const FVector& InStart, const FVector& InEnd, const FVector4& InColor)
{
    Data->DebugSource->AddLine(InLabel, InStart, InEnd, InColor);
}

void UBatchLineManager::RemoveDebugLine(const FName& InLabel)
{
	Data->DebugSource->RemoveLine(InLabel);
}

void UBatchLineManager::AddDebugCircle(const FName& BaseLabel, const FVector& Center, float Radius, const FVector4& Color, TArray<FName>& OutLabels)
{
	constexpr int32 Segments = 32; // 원의 부드러움 정도

	for (int32 i = 0; i < Segments; ++i)
	{
		const float Angle1 = static_cast<float>(i) / Segments * 2.0f * PI;
		const float Angle2 = static_cast<float>(i + 1) / Segments * 2.0f * PI;

		FVector P1, P2;
		for (uint32 Axis = 0; Axis < 3; ++Axis)
		{
			if (Axis == 0) // XY 평면
			{
				P1 = FVector(cos(Angle1) * Radius, sin(Angle1) * Radius, 0.f);
				P2 = FVector(cos(Angle2) * Radius, sin(Angle2) * Radius, 0.f);
			}
			else if (Axis == 1) // XZ 평면
			{
				P1 = FVector(cos(Angle1) * Radius, 0.f, sin(Angle1) * Radius);
				P2 = FVector(cos(Angle2) * Radius, 0.f, sin(Angle2) * Radius);
			}
			else // YZ 평면
			{
				P1 = FVector(0.f, cos(Angle1) * Radius, sin(Angle1) * Radius);
				P2 = FVector(0.f, cos(Angle2) * Radius, sin(Angle2) * Radius);
			}

			const FName Label = FName(std::format("{}_Circle_{}_{}", BaseLabel.ToString(), Axis, i));

			// 라인 추가 및 라벨 저장
			Data->DebugSource->AddLine(Label, Center + P1, Center + P2, Color);
			OutLabels.emplace_back(Label);
		}
	}
}

void UBatchLineManager::UpdateGrid(float InCellSize)
{
    Data->GridSource->SetCellSize(InCellSize);
}

void UBatchLineManager::UpdateBoundingBox(const IBoundingVolume* InBoundingVolume)
{
    Data->BoundingBoxSource->SetBoundingVolume(InBoundingVolume);
}

float UBatchLineManager::GetGridCellSize() const
{
    return Data->GridSource->GetCellSize();
}

void UBatchLineManager::Update()
{
    if (!Data->bIsDirty)
    {
        for (ILineSource* Source : Data->Sources)
        {
            if (Source->IsDirty())
            {
                Data->bIsDirty = true;
                break;
            }
        }
    }

    if (Data->bIsDirty)
    {
        TArray<FLineVertex> AllVertices;
        TArray<uint32> AllIndices;

        uint32 VertexOffset = 0;
        for (ILineSource* Source : Data->Sources)
        {
            const TArray<FLineVertex>& Vertices = Source->GetVertices();
            const TArray<uint32>& Indices = Source->GetIndices();

            AllVertices.insert(AllVertices.end(), Vertices.begin(), Vertices.end());

            for (uint32 Index : Indices)
            {
                AllIndices.push_back(VertexOffset + Index);
            }

            VertexOffset += static_cast<uint32>(Vertices.size());
            Source->ClearDirtyFlag();
        }

        Data->Primitive.NumVertices = static_cast<uint32>(AllVertices.size());
        Data->Primitive.NumIndices = static_cast<uint32>(AllIndices.size());

        SafeRelease(Data->Primitive.VertexBuffer);
        SafeRelease(Data->Primitive.IndexBuffer);

        if (Data->Primitive.NumVertices > 0)
        {
            Data->Primitive.VertexBuffer = FRenderResourceFactory::CreateVertexBuffer(AllVertices, true);
        }
        if (Data->Primitive.NumIndices > 0)
        {
            Data->Primitive.IndexBuffer = FRenderResourceFactory::CreateIndexBuffer(AllIndices.data(), Data->Primitive.NumIndices * sizeof(uint32));
        }

        Data->bIsDirty = false;
    }
}

void UBatchLineManager::Render()
{
    if (Data->Primitive.VertexBuffer && Data->Primitive.IndexBuffer)
    {
        URenderer::GetInstance().RenderEditorPrimitive(Data->Primitive, Data->Primitive.RenderState, sizeof(FLineVertex), sizeof(uint32));
    }
}
