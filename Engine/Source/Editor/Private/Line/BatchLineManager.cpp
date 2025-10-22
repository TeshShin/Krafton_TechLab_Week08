#include "pch.h"
#include "Editor/Public/Line/BatchLineManager.h"
#include "Renderer/Public/RenderResourceFactory.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Editor/Public/Line/GridLineSource.h"
#include "Editor/Public/Line/BoundingBoxLineSource.h"
#include "Editor/Public/Line/DebugLineSource.h"
#include "Editor/Public/Line/ILineSource.h"

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

UBatchLineManager::UBatchLineManager() = default;

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

	Data->Primitive.Location = FVector(0, 0, 0);
	Data->Primitive.Scale = FVector(1, 1, 1);
	Data->Primitive.Rotation = FQuaternion::Identity();
    Data->Primitive.Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
}

void UBatchLineManager::Release()
{
    SafeDelete(Data->GridSource);
    SafeDelete(Data->BoundingBoxSource);
    SafeDelete(Data->DebugSource);

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

void UBatchLineManager::AddDebugArrow(const FName& InLabel, const FVector& InStart, const FVector& InEnd, const FVector4& InColor, float InHeadSize, TArray<FName>& OutLabels)
{
	// Draw the arrow shaft
	AddDebugLine(InLabel, InStart, InEnd, InColor);

	// Draw the arrow head
	FVector Direction = (InEnd - InStart).GetNormalized();
	FVector UpVector = (abs(Direction.Z) < 0.999f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
	FVector RightVector = Direction.Cross(UpVector).GetNormalized();
	UpVector = RightVector.Cross(Direction);

	FVector HeadBase = InEnd - Direction * InHeadSize;

	FVector HeadPoint1 = HeadBase + RightVector * InHeadSize * 0.5f;
	FVector HeadPoint2 = HeadBase - RightVector * InHeadSize * 0.5f;
	FVector HeadPoint3 = HeadBase + UpVector * InHeadSize * 0.5f;
	FVector HeadPoint4 = HeadBase - UpVector * InHeadSize * 0.5f;

	FName Label1 = FName(InLabel.ToString() + "_Head1");
	FName Label2 = FName(InLabel.ToString() + "_Head2");
	FName Label3 = FName(InLabel.ToString() + "_Head3");
	FName Label4 = FName(InLabel.ToString() + "_Head4");
	FName Label5 = FName(InLabel.ToString() + "_HeadBase1");
	FName Label6 = FName(InLabel.ToString() + "_HeadBase2");
	FName Label7 = FName(InLabel.ToString() + "_HeadBase3");
	FName Label8 = FName(InLabel.ToString() + "_HeadBase4");

	OutLabels.emplace_back(InLabel);
	OutLabels.emplace_back(Label1);
	OutLabels.emplace_back(Label2);
	OutLabels.emplace_back(Label3);
	OutLabels.emplace_back(Label4);
	OutLabels.emplace_back(Label5);
	OutLabels.emplace_back(Label6);
	OutLabels.emplace_back(Label7);
	OutLabels.emplace_back(Label8);

	AddDebugLine(Label1, InEnd, HeadPoint1, InColor);
	AddDebugLine(Label2, InEnd, HeadPoint2, InColor);
	AddDebugLine(Label3, InEnd, HeadPoint3, InColor);
	AddDebugLine(Label4, InEnd, HeadPoint4, InColor);

	AddDebugLine(Label5, HeadPoint1, HeadPoint3, InColor);
	AddDebugLine(Label6, HeadPoint3, HeadPoint2, InColor);
	AddDebugLine(Label7, HeadPoint2, HeadPoint4, InColor);
	AddDebugLine(Label8, HeadPoint4, HeadPoint1, InColor);
}

void UBatchLineManager::AddDebugCone(const FName& BaseLabel, const FVector& TipLocation, const FVector& Direction,
	float Radius, float ConeAngleDegrees, const FVector4& Color, TArray<FName>& OutLabels)
{
	FVector UpVector;
	const FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);
	if (abs(Direction.Dot(WorldUp)) > 0.999f)
	{
		UpVector = FVector(0.0f, 1.0f, 0.0f);
	}
	else
	{
		UpVector = WorldUp;
	}
	const FVector RightVector = Direction.Cross(UpVector).GetNormalized();
	const FVector ConeUpVector = RightVector.Cross(Direction);

	const float AngleRad = ConeAngleDegrees * ToRad;
	const float ConeHeight = Radius * cos(AngleRad);
	const float BottomCircleRadius = Radius * sin(AngleRad);

	const FVector BaseCenter = TipLocation + Direction * ConeHeight;
	// 밑면 원 그리기
	constexpr int32 CircleSegments = 32;
	for (int32 i = 0; i < CircleSegments; ++i)
	{
		const float Angle1 = static_cast<float>(i) / CircleSegments * 2.0f * PI;
		const float Angle2 = static_cast<float>(i + 1) / CircleSegments * 2.0f * PI;
		const FVector P1_Offset = (RightVector * cos(Angle1) + ConeUpVector * sin(Angle1));
		const FVector P2_Offset = (RightVector * cos(Angle2) + ConeUpVector * sin(Angle2));

		const FName Label = FName(std::format("{}_Circle_{}", BaseLabel.ToString(), i));
		Data->DebugSource->AddLine(Label, BaseCenter + P1_Offset * BottomCircleRadius, BaseCenter + P2_Offset * BottomCircleRadius, Color);
		OutLabels.emplace_back(Label);
	}

	// 원뿔 꼭짓점과 밑면을 잇는 선 그리기
	constexpr int32 EdgeLines = 32;
	for (int32 i = 0; i < EdgeLines; ++i)
	{
		const float Angle = static_cast<float>(i) / EdgeLines * 2.0f * PI;
		const FVector EdgePointOffset = (RightVector * cos(Angle) + ConeUpVector * sin(Angle));

		const FName EdgeLabel(std::format("{}_EdgeLine_{}", BaseLabel.ToString(), i));
		Data->DebugSource->AddLine(EdgeLabel, TipLocation, BaseCenter + EdgePointOffset * BottomCircleRadius, Color);
		OutLabels.emplace_back(EdgeLabel);
	}

	// 밑면에서 기존 원 -Angle ~ +Angle 선 그리기
	for (int32 i = 0; i < CircleSegments; ++i)
	{
		const float t1 = static_cast<float>(i) / CircleSegments;
		const float t2 = static_cast<float>(i + 1) / CircleSegments;

		// 0~2PI 대신, StartAngleRad부터 AngleRangeRad 범위 내에서 각도를 계산
		const float Angle1 = -AngleRad + 2 * t1 * AngleRad;
		const float Angle2 = -AngleRad + 2 * t2 * AngleRad;

		FVector P1, P2;
		for (uint32 Axis = 0; Axis < 2; ++Axis)
		{
			if (Axis == 0) // XY 평면
			{
				P1 = FVector(cos(Angle1) * Radius, sin(Angle1) * Radius, 0.f);
				P2 = FVector(cos(Angle2) * Radius, sin(Angle2) * Radius, 0.f);
			}
			else // XZ 평면
			{
				P1 = FVector(cos(Angle1) * Radius, 0.f, sin(Angle1) * Radius);
				P2 = FVector(cos(Angle2) * Radius, 0.f, sin(Angle2) * Radius);
			}

			const FName Label = FName(std::format("{}_Circle_{}_{}", BaseLabel.ToString(), Axis, i));

			Data->DebugSource->AddLine(Label, TipLocation + P1, TipLocation + P2, Color);
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
        TArray<FNormalVertex> AllVertices;
        TArray<uint32> AllIndices;

        uint32 VertexOffset = 0;
        for (ILineSource* Source : Data->Sources)
        {
        	if (GEditor->IsPIESessionActive() && !Source->IsRenderInPIE()) { continue; }
            const TArray<FLineVertex>& Vertices = Source->GetVertices();
            const TArray<uint32>& Indices = Source->GetIndices();

        	for (const FLineVertex& Vertex : Vertices)
        	{
        		AllVertices.emplace_back(FNormalVertex::FromLineVertex(Vertex));
        	}

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

const struct FEditorPrimitive* UBatchLineManager::GetBatchLinePrimitive() const
{
	if (Data)
	{
		return &Data->Primitive;
	}
	return nullptr;
}
