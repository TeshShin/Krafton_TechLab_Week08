#include "pch.h"
#include "Editor/Public/Gizmo.h"
#include "Editor/Public/Camera.h"
#include "Manager/Public/AssetManager.h"
#include "Renderer/Public/Renderer.h"
#include "Editor/Public/Editor.h"

FGizmo::FGizmo()
{
	UAssetManager& ResourceManager = UAssetManager::GetInstance();
	Primitives.resize(3);
	Primitives[0].resize(3);
	Primitives[1].resize(3);
	Primitives[2].resize(3);
	GizmoColor.resize(3);

	/* *
	* @brief 0: Forward(x), 1: Right(y), 2: Up(z)
	*/
	GizmoColor[0] = FVector4(1, 0, 0, 1);
	GizmoColor[1] = FVector4(0, 1, 0, 1);
	GizmoColor[2] = FVector4(0, 0, 1, 1);

	for (uint32 Idx = 0; Idx < 3; Idx++)
	{
		/* *
		* @brief Translation Setting
		*/
		const float ScaleT = TranslateCollisionConfig.Scale;
		Primitives[0][Idx].VertexBuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::Arrow);
		Primitives[0][Idx].NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::Arrow);
		Primitives[0][Idx].Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		Primitives[0][Idx].Scale = FVector(ScaleT, ScaleT, ScaleT);

		/* *
		* @brief Rotation Setting
		*/
		Primitives[1][Idx].VertexBuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::Ring);
		Primitives[1][Idx].NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::Ring);
		Primitives[1][Idx].Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		Primitives[1][Idx].Scale = FVector(ScaleT, ScaleT, ScaleT);

		/* *
		* @brief Scale Setting
		*/
		Primitives[2][Idx].VertexBuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::CubeArrow);
		Primitives[2][Idx].NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::CubeArrow);
		Primitives[2][Idx].Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		Primitives[2][Idx].Scale = FVector(ScaleT, ScaleT, ScaleT);

		/* *
		* @brief Render State
		*/
		RenderState.FillMode = EFillMode::Solid;
		RenderState.CullMode = ECullMode::None;
	}
}

FGizmo::~FGizmo() = default;

void FGizmo::Update(UCamera* InCamera)
{
	TargetComponent = Cast<USceneComponent>(GEditor->GetEditorModule()->GetSelectedComponent());
	if (!TargetComponent || !InCamera) { return; }

	float Scale;
	if (InCamera->GetCameraType() == ECameraType::ECT_Perspective)
	{
		const float DistanceToCamera = (InCamera->GetLocation() - TargetComponent->GetWorldLocation()).Length();
		Scale = DistanceToCamera * ScaleFactor;
		if (DistanceToCamera < MinScaleFactor)
			Scale = MinScaleFactor * ScaleFactor;
	}
	else // Orthographic
	{
		Scale = OrthoScaleFactor;
	}

	TranslateCollisionConfig.Scale = Scale;
	RotateCollisionConfig.Scale = Scale;

	TArray<FEditorPrimitive>& P = Primitives[static_cast<int>(GizmoMode)];
	for (uint32 Idx = 0; Idx < 3; Idx++)
	{
		P[Idx].Location = TargetComponent->GetWorldLocation();
		P[Idx].Scale = FVector(Scale, Scale, Scale);
	}

	// 2) 로컬 회전 중에도 기즈모를 실시간으로 갱신
	FQuaternion LocalRot;
	if (GizmoMode == EGizmoMode::Scale)
	{
		LocalRot = TargetComponent->GetWorldRotationAsQuaternion();
	}
	else
	{
		LocalRot = bIsWorld ? FQuaternion::Identity() : TargetComponent->GetWorldRotationAsQuaternion();
	}

	// X축 (Forward) - 빨간색
	P[0].Rotation = LocalRot * FQuaternion::Identity();
	P[0].Color = ColorFor(EGizmoDirection::Forward);

	// Y축 (Right) - 초록색 (Z축 주위로 90도 회전)
	P[1].Rotation =  LocalRot * FQuaternion::FromAxisAngle(FVector::UpVector(), 90.0f * ToRad);
	P[1].Color = ColorFor(EGizmoDirection::Right);

	// Z축 (Up) - 파란색 (Y축 주위로 -90도 회전)
	P[2].Rotation =  LocalRot * FQuaternion::FromAxisAngle(FVector::RightVector(), -90.0f * ToRad);
	P[2].Color = ColorFor(EGizmoDirection::Up);
}

void FGizmo::ChangeGizmoMode()
{
	switch (GizmoMode)
	{
	case EGizmoMode::Translate:
		GizmoMode = EGizmoMode::Rotate; break;
	case EGizmoMode::Rotate:
		GizmoMode = EGizmoMode::Scale; break;
	case EGizmoMode::Scale:
		GizmoMode = EGizmoMode::Translate;
	}
}

void FGizmo::SetLocation(const FVector& Location)
{
	TargetComponent->SetWorldLocation(Location);
}

bool FGizmo::IsInRadius(float Radius)
{
	if (Radius >= RotateCollisionConfig.InnerRadius * RotateCollisionConfig.Scale && Radius <= RotateCollisionConfig.OuterRadius * RotateCollisionConfig.Scale)
		return true;
	return false;
}

void FGizmo::OnMouseDragStart(FVector& CollisionPoint)
{
	bIsDragging = true;
	DragStartMouseLocation = CollisionPoint;

	if (TargetComponent)
	{
		DragStartActorLocation = TargetComponent->GetWorldLocation();
		DragStartActorRotation = TargetComponent->GetWorldRotation();
		DragStartActorScale = TargetComponent->GetWorldScale3D();
		FVector LocalAxis = GetGizmoAxis();
		FVector WorldSpaceDragAxis = LocalAxis;

		if (!IsWorldMode())
		{
			FQuaternion StartRotation = TargetComponent->GetWorldRotationAsQuaternion();
			WorldSpaceDragAxis = StartRotation.RotateVector(LocalAxis);
		}

		StoredDragAxis = WorldSpaceDragAxis;
	}
}

// 하이라이트 색상은 렌더 시점에만 계산 (상태 오염 방지)
FVector4 FGizmo::ColorFor(EGizmoDirection InAxis) const
{
	const int Idx = AxisIndex(InAxis);
	//UE_LOG("%d", Idx);
	const FVector4& BaseColor = GizmoColor[Idx];
	const bool bIsHighlight = (InAxis == GizmoDirection);

	const FVector4 Paint = bIsHighlight ? FVector4(1,1,0,1) : BaseColor;
	//UE_LOG("InAxis: %d, Idx: %d, Dir: %d, base color: %.f, %.f, %.f, bHighLight: %d", InAxis, Idx, GizmoDirection, BaseColor.X, BaseColor.Y, BaseColor.Z, bIsHighlight);

	if (bIsDragging)
		return BaseColor;
	else
		return Paint;
}

TArray<const FEditorPrimitive*> FGizmo::GetEditorPrimitive() const
{
	if (!TargetComponent) { return {}; }
	const TArray<FEditorPrimitive>& P = Primitives[static_cast<int>(GizmoMode)];
	return { &P[0], &P[1], &P[2] };
}
