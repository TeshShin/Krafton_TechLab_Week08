#include "pch.h"
#include "Editor/Public/BatchLineManager.h"
#include "Editor/Public/ILineSource.h"
#include "Renderer/Public/Renderer.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Editor/Public/GridLineSource.h"
#include "Editor/Public/BoundingBoxLineSource.h"
#include "Editor/Public/DebugLineSource.h"

struct FBatchLineManagerData
{
    TArray<ILineSource*> Sources;
    GridLineSource* GridSource = nullptr;
    BoundingBoxLineSource* BoundingBoxSource = nullptr;
    DebugLineSource* DebugSource = nullptr;

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

    Data->GridSource = new GridLineSource();
    Data->Sources.push_back(Data->GridSource);

    Data->BoundingBoxSource = new BoundingBoxLineSource();
    Data->Sources.push_back(Data->BoundingBoxSource);

    Data->DebugSource = new DebugLineSource();
    Data->Sources.push_back(Data->DebugSource);

    // Create shaders and other resources here
    ID3D11VertexShader* VertexShader;
    ID3D11InputLayout* InputLayout;
    TArray<D3D11_INPUT_ELEMENT_DESC> Layout = { {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0} };
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

void UBatchLineManager::AddLine(const FVector& InStart, const FVector& InEnd, const FVector4& InColor)
{
    Data->DebugSource->AddLine(InStart, InEnd, InColor);
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
        TArray<FVector> AllVertices;
        TArray<uint32> AllIndices;

        uint32 VertexOffset = 0;
        for (ILineSource* Source : Data->Sources)
        {
            const TArray<FVector>& Vertices = Source->GetVertices();
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
            Data->Primitive.VertexBuffer = FRenderResourceFactory::CreateVertexBuffer(AllVertices.data(), Data->Primitive.NumVertices * sizeof(FVector), true);
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
        URenderer::GetInstance().RenderEditorPrimitive(Data->Primitive, Data->Primitive.RenderState, sizeof(FVector), sizeof(uint32));
    }
}
