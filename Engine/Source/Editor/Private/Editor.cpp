#include "pch.h"
#include "Editor/Public/Editor.h"
#include "Editor/Public/Camera.h"
#include "Editor/Public/Viewport.h"
#include "Renderer/Public/Renderer.h"
#include "Manager/Public/UIManager.h"
#include "Manager/Public/InputManager.h"
#include "Manager/Public/ConfigManager.h"
#include "Manager/Public/TimeManager.h"
#include "Scene/Public/Component/PrimitiveComponent.h"
#include "Scene/Public/Level/Level.h"
#include "Core/Public/Misc/ScopeCycleCounter.h"
#include "Editor/Public/Line/BatchLineManager.h"
#include "Editor/Public/UI/StatOverlay.h"
#include "Scene/Public/Component/LightComponentBase.h"
#include "ImGui/imgui.h"

UEditor::UEditor()
{
	const TArray<float>& SplitterRatio = UConfigManager::GetInstance().GetSplitterRatio();
	RootSplitter.SetRatio(SplitterRatio[0]);
	LeftSplitter.SetRatio(SplitterRatio[1]);
	RightSplitter.SetRatio(SplitterRatio[2]);

	UE_LOG("Editor: Loaded splitter ratios from config: %.2f, %.2f, %.2f",
		SplitterRatio[0], SplitterRatio[1], SplitterRatio[2]);

	// 초기 레이아웃 상태 설정 (splitter ratio 기반)
	if (SplitterRatio[0] >= 0.99f && SplitterRatio[1] >= 0.99f)
	{
		ViewportLayoutState = EViewportLayoutState::Single;
		TargetViewportLayoutState = EViewportLayoutState::Single;

		// 단일 뷰포트로 시작할 때, 4분할 복원을 위한 기본값 저장
		SavedRootRatio = 0.5f;
		SavedLeftRatio = 0.5f;
		SavedRightRatio = 0.5f;

		UE_LOG("Editor: Starting in SINGLE viewport mode, SavedRatio=(0.5, 0.5, 0.5)");
	}
	else
	{
		ViewportLayoutState = EViewportLayoutState::Multi;
		TargetViewportLayoutState = EViewportLayoutState::Multi;

		// 다중 뷰포트로 시작할 때, 현재 비율 저장
		SavedRootRatio = SplitterRatio[0];
		SavedLeftRatio = SplitterRatio[1];
		SavedRightRatio = SplitterRatio[2];

		UE_LOG("Editor: Starting in MULTI viewport mode, SavedRatio=(%.2f, %.2f, %.2f)",
			SavedRootRatio, SavedLeftRatio, SavedRightRatio);
	}

	InitializeLayout();
}

UEditor::~UEditor()
{
	UConfigManager::GetInstance().SetSplitterRatio(RootSplitter.GetRatio(), LeftSplitter.GetRatio(), RightSplitter.GetRatio());
	SafeDelete(DraggedSplitter);
	SafeDelete(InteractionViewport);
}

void UEditor::Update()
{
	bool bIsInPIE = GEditor->IsPIESessionActive();
	if (bIsInPIE && !bWasInPIE)
	{
		auto& LineManager = UBatchLineManager::GetInstance();
		for (const FName& Label : DebugArrowLabels)
		{
			LineManager.RemoveDebugLine(Label);
		}
		DebugArrowLabels.clear();
	}
	bWasInPIE = bIsInPIE;

	URenderer& Renderer = URenderer::GetInstance();
	FViewport* Viewport = Renderer.GetViewportClient();
	UUIManager& UIManager = UUIManager::GetInstance();

	FVector MousePositionForViewport = UInputManager::GetInstance().GetMousePosition();
	bool bShouldProcessInput = true;

	// 마우스가 중앙 노드 영역 내에 있는지만 확인 (좌표 변환 없음)
	// UpdateLayout()에서 설정한 각 뷰포트의 위치는 이미 화면 절대 좌표
	if (UIManager.HasCentralNode())
	{
		ImVec2 CentralPos = UIManager.GetCentralNodePos();
		ImVec2 CentralSize = UIManager.GetCentralNodeSize();
		ImVec2 GlobalMousePos = ImGui::GetMousePos();

		// 마우스가 중앙 노드 영역 내에 있는지 확인
		if (GlobalMousePos.x >= CentralPos.x && GlobalMousePos.x <= CentralPos.x + CentralSize.x &&
			GlobalMousePos.y >= CentralPos.y && GlobalMousePos.y <= CentralPos.y + CentralSize.y)
		{
			bShouldProcessInput = true;
		}
		else
		{
			// 중앙 노드 밖에서는 입력 무시
			bShouldProcessInput = false;
		}
	}

	// 1. 마우스 위치를 기반으로 활성 뷰포트를 결정합니다.
	if (bShouldProcessInput)
	{
		Viewport->UpdateActiveViewportClient(MousePositionForViewport);

		// 2. 활성 뷰포트의 카메라의 제어만 업데이트합니다.
		if (UCamera* ActiveCamera = Viewport->GetActiveCamera())
		{
			// 만약 이동량이 있고, 직교 카메라라면 ViewportClient에 알립니다.
			const FVector MovementDelta = ActiveCamera->UpdateInput();
			if (MovementDelta.LengthSquared() > 0.f && ActiveCamera->GetCameraType() == ECameraType::ECT_Orthographic)
			{
				Viewport->UpdateOrthoFocusPointByDelta(MovementDelta);
			}
		}
	}

	UpdateBatchLines();
	UpdateLightDebugInfo();

	ProcessMouseInput();

	UpdateLayout();
}

void UEditor::UpdateLightDebugInfo()
{
	if (GEditor->IsPIESessionActive()) { return; }

	auto& LineManager = UBatchLineManager::GetInstance();
	for (const FName& Label : DebugArrowLabels)
	{
		LineManager.RemoveDebugLine(Label);
	}
	DebugArrowLabels.clear();

	ULevel* Level = GWorld->GetLevel();
	if (!Level) { return; }

	for (ULightComponentBase* Light : Level->GetLights())
	{
		if (Light)
		{
			Light->DrawDebugArrow(DebugArrowLabels);
		}
	}
}

void UEditor::SetSingleViewportLayout(int InActiveIndex)
{
	if (ViewportLayoutState == EViewportLayoutState::Animating) return;

	if (ViewportLayoutState == EViewportLayoutState::Multi)
	{
		SavedRootRatio = RootSplitter.GetRatio();
		SavedLeftRatio = LeftSplitter.GetRatio();
		SavedRightRatio = RightSplitter.GetRatio();
	}

	SourceRootRatio = RootSplitter.GetRatio();
	SourceLeftRatio = LeftSplitter.GetRatio();
	SourceRightRatio = RightSplitter.GetRatio();

	TargetRootRatio = SourceRootRatio;
	TargetLeftRatio = SourceLeftRatio;
	TargetRightRatio = SourceRightRatio;

	switch (InActiveIndex)
	{
	case 0: // 좌상단
		TargetRootRatio = 1.0f;
		TargetLeftRatio = 1.0f;
		break;
	case 1: // 좌하단
		TargetRootRatio = 1.0f;
		TargetLeftRatio = 0.0f;
		break;
	case 2: // 우상단
		TargetRootRatio = 0.0f;
		TargetRightRatio = 1.0f;
		break;
	case 3: // 우하단
		TargetRootRatio = 0.0f;
		TargetRightRatio = 0.0f;
		break;
	default:
		RestoreMultiViewportLayout();
		return;
	}

	ViewportLayoutState = EViewportLayoutState::Animating;
	TargetViewportLayoutState = EViewportLayoutState::Single;
	AnimationStartTime = UTimeManager::GetInstance().GetGameTime();
}

/**
 * @brief 저장된 비율을 사용하여 4분할 뷰포트 레이아웃으로 복원합니다.
 */
void UEditor::RestoreMultiViewportLayout()
{
	if (ViewportLayoutState == EViewportLayoutState::Animating) return;

	SourceRootRatio = RootSplitter.GetRatio();
	SourceLeftRatio = LeftSplitter.GetRatio();
	SourceRightRatio = RightSplitter.GetRatio();

	TargetRootRatio = SavedRootRatio;
	TargetLeftRatio = SavedLeftRatio;
	TargetRightRatio = SavedRightRatio;

	UE_LOG("RestoreMultiViewportLayout: Source(%.2f, %.2f, %.2f) -> Target(%.2f, %.2f, %.2f)",
		SourceRootRatio, SourceLeftRatio, SourceRightRatio,
		TargetRootRatio, TargetLeftRatio, TargetRightRatio);

	ViewportLayoutState = EViewportLayoutState::Animating;
	TargetViewportLayoutState = EViewportLayoutState::Multi;
	AnimationStartTime = UTimeManager::GetInstance().GetGameTime();
}

void UEditor::InitializeLayout()
{
	// 1. 루트 스플리터의 자식으로 2개의 수평 스플리터를 '주소'로 연결합니다.
	RootSplitter.SetChildren(&LeftSplitter, &RightSplitter);

	// 2. 각 수평 스플리터의 자식으로 뷰포트 윈도우들을 '주소'로 연결합니다.
	LeftSplitter.SetChildren(&ViewportWindows[0], &ViewportWindows[1]);
	RightSplitter.SetChildren(&ViewportWindows[2], &ViewportWindows[3]);

	// 3. 초기 레이아웃 계산 (비율은 생성자에서 설정된 값 유지)
	const D3D11_VIEWPORT& ViewportInfo = URenderer::GetInstance().GetDeviceResources()->GetViewportInfo();
	FRect FullScreenRect = { ViewportInfo.TopLeftX, ViewportInfo.TopLeftY, ViewportInfo.Width, ViewportInfo.Height };
	RootSplitter.Resize(FullScreenRect);
}

void UEditor::UpdateBatchLines()
{
	// if (ShowFlags & EEngineShowFlags::SF_Octree)
	// {
	// 	UBatchLineManager::GetInstance().UpdateOctreeVertices(GWorld->GetLevel()->GetStaticOctree());
	// }
	// else
	// {
	// 	// If we are not showing the octree, clear the lines, so they don't persist
	// 	UBatchLineManager::GetInstance().ClearOctreeLines();
	// }

	if (UActorComponent* Component = GetSelectedComponent())
	{
		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
		{
			if (ShowFlags & EEngineShowFlags::SF_Bounds)
			{
				if (PrimitiveComponent->GetBoundingBox()->GetType() == EBoundingVolumeType::AABB)
				{
					FVector WorldMin, WorldMax; PrimitiveComponent->GetWorldAABB(WorldMin, WorldMax);
					FAABB AABB(WorldMin, WorldMax);
					UBatchLineManager::GetInstance().UpdateBoundingBox(&AABB);
				}
				else
				{
					UBatchLineManager::GetInstance().UpdateBoundingBox(PrimitiveComponent->GetBoundingBox());
					// 만약 선택된 타입이 decalspotlightcomponent라면
					// if (Component->IsA(UDecalSpotLightComponent::StaticClass()))
					// {
					// 	UBatchLineManager::GetInstance().UpdateSpotLightVertices(Cast<UDecalSpotLightComponent>(Component));
					// }
				}
				return;
			}
		}
	}

	UBatchLineManager::GetInstance().UpdateBoundingBox(nullptr);
}

void UEditor::UpdateLayout()
{
	URenderer& Renderer = URenderer::GetInstance();
	UInputManager& Input = UInputManager::GetInstance();
	UUIManager& UIManager = UUIManager::GetInstance();
	const FPoint MousePosition = { Input.GetMousePosition().X, Input.GetMousePosition().Y };
	bool bIsHoveredOnSplitter = false;

	// 뷰포트를 전환 중이라면 애니메이션을 적용합니다.
	if (ViewportLayoutState == EViewportLayoutState::Animating)
	{
		float ElapsedTime = UTimeManager::GetInstance().GetGameTime() - AnimationStartTime;
		float Alpha = clamp(ElapsedTime / AnimationDuration, 0.0f, 1.0f);

		float NewRootRatio = Lerp(SourceRootRatio, TargetRootRatio, Alpha);
		float NewLeftRatio = Lerp(SourceLeftRatio, TargetLeftRatio, Alpha);
		float NewRightRatio = Lerp(SourceRightRatio, TargetRightRatio, Alpha);

		RootSplitter.SetRatio(NewRootRatio);
		LeftSplitter.SetRatio(NewLeftRatio);
		RightSplitter.SetRatio(NewRightRatio);

		if (Alpha >= 1.0f)
		{
			ViewportLayoutState = TargetViewportLayoutState;
		}
	}

	// 1. 드래그 상태가 아니라면 커서의 상태를 감지합니다.
	if (DraggedSplitter == nullptr)
	{
		if (LeftSplitter.IsHovered(MousePosition) || RightSplitter.IsHovered(MousePosition))
		{
			bIsHoveredOnSplitter = true;
		}
		else if (RootSplitter.IsHovered(MousePosition))
		{
			bIsHoveredOnSplitter = true;
		}
	}

	// 2. 스플리터 위에 커서가 있으며 클릭을 한다면, 드래그 상태로 활성화합니다.
	if (UInputManager::GetInstance().IsKeyPressed(EKeyInput::MouseLeft) && bIsHoveredOnSplitter)
	{
		// 호버 상태에 따라 드래그할 스플리터를 결정합니다.
		if (LeftSplitter.IsHovered(MousePosition)) { DraggedSplitter = &LeftSplitter; }				// 좌상, 좌하
		else if (RightSplitter.IsHovered(MousePosition)) { DraggedSplitter = &RightSplitter; }		// 우상, 우하
		else if (RootSplitter.IsHovered(MousePosition)) { DraggedSplitter = &RootSplitter; }
	}

	// 3. 드래그 상태라면 스플리터 기능을 이행합니다.
	if (DraggedSplitter)
	{
		// 중앙 노드 영역을 기준으로 사용
		ImVec2 CentralPos = UIManager.GetCentralNodePos();
		ImVec2 CentralSize = UIManager.GetCentralNodeSize();
		FRect WorkableRect = { CentralPos.x, CentralPos.y, CentralSize.x, CentralSize.y };

		FRect ParentRect;

		if (DraggedSplitter == &RootSplitter)
		{
			ParentRect = WorkableRect;
		}
		else
		{
			if (DraggedSplitter == &LeftSplitter)
			{
				ParentRect.Left = WorkableRect.Left;
				ParentRect.Top = WorkableRect.Top;
				ParentRect.Width = WorkableRect.Width * RootSplitter.GetRatio();
				ParentRect.Height = WorkableRect.Height;
			}
			else if (DraggedSplitter == &RightSplitter)
			{
				ParentRect.Left = WorkableRect.Left + WorkableRect.Width * RootSplitter.GetRatio();
				ParentRect.Top = WorkableRect.Top;
				ParentRect.Width = WorkableRect.Width * (1.0f - RootSplitter.GetRatio());
				ParentRect.Height = WorkableRect.Height;
			}
		}

		// 마우스 위치를 부모 영역에 대한 비율(0.0 ~ 1.0)로 변환합니다.
		float NewRatio = 0.5f;
		if (dynamic_cast<SSplitterV*>(DraggedSplitter)) // 수직 스플리터
		{
			if (ParentRect.Width > 0)
			{
				NewRatio = (MousePosition.X - ParentRect.Left) / ParentRect.Width;
			}
		}
		else // 수평 스플리터
		{
			if (ParentRect.Height > 0)
			{
				NewRatio = (MousePosition.Y - ParentRect.Top) / ParentRect.Height;
			}
		}

		// 계산된 비율을 스플리터에 적용합니다.
		DraggedSplitter->SetRatio(NewRatio);
	}

	// 4. 매 프레임 현재 비율에 맞게 전체 레이아웃 크기를 다시 계산하고, 그 결과를 실제 FViewport에 반영합니다.
	// 중앙 노드 영역을 기준으로 사용
	ImVec2 CentralPos = UIManager.GetCentralNodePos();
	ImVec2 CentralSize = UIManager.GetCentralNodeSize();
	FRect WorkableRect = { CentralPos.x, CentralPos.y, CentralSize.x, CentralSize.y };
	RootSplitter.Resize(WorkableRect);

	if (FViewport* ViewportClient = URenderer::GetInstance().GetViewportClient())
	{
		auto& Viewports = ViewportClient->GetViewports();
		for (int i = 0; i < 4; ++i)
		{
			if (i < Viewports.size())
			{
				const FRect& Rect = ViewportWindows[i].Rect;
				Viewports[i].SetViewportInfo({ Rect.Left, Rect.Top, Rect.Width, Rect.Height, 0.0f, 1.0f });
			}
		}
	}

	// 마우스 클릭 해제를 하면 드래그 비활성화
	if (UInputManager::GetInstance().IsKeyReleased(EKeyInput::MouseLeft)) { DraggedSplitter = nullptr; }
}

void UEditor::ProcessMouseInput()
{
	// 선택된 뷰포트의 정보들을 가져옵니다.
	FViewport* ViewportClient = URenderer::GetInstance().GetViewportClient();
	FViewportClient* CurrentViewport = nullptr;
	UCamera* CurrentCamera = nullptr;

	// 이미 선택된 뷰포트 영역이 존재한다면 선택된 뷰포트 처리를 진행합니다.
	if (InteractionViewport) { CurrentViewport = InteractionViewport; }
	// 선택된 뷰포트 영역이 존재하지 않는다면 현재 마우스 위치의 뷰포트를 선택합니다.
	else { CurrentViewport = ViewportClient->GetActiveViewportClient(); }

	// 처리할 영역이 존재하지 않으면 진행을 중단합니다.
	if (CurrentViewport == nullptr) { return; }

	CurrentCamera = &CurrentViewport->Camera;

	AActor* ActorPicked = GetSelectedActor();
	Gizmo.Update(CurrentCamera);

	const UInputManager& InputManager = UInputManager::GetInstance();
	const FVector& MousePos = InputManager.GetMousePosition();
	const D3D11_VIEWPORT& ViewportInfo = CurrentViewport->GetViewportInfo();

	const float NdcX = ((MousePos.X - ViewportInfo.TopLeftX) / ViewportInfo.Width) * 2.0f - 1.0f;
	const float NdcY = -(((MousePos.Y - ViewportInfo.TopLeftY) / ViewportInfo.Height) * 2.0f - 1.0f);

	FRay WorldRay = CurrentCamera->ConvertToWorldRay(NdcX, NdcY);

	static EGizmoDirection PreviousGizmoDirection = EGizmoDirection::None;
	FVector CollisionPoint;
	float ActorDistance = -1;

	if (InputManager.IsKeyPressed(EKeyInput::Tab)) { Gizmo.ChangeWorldLocalMode(); }
	if (InputManager.IsKeyPressed(EKeyInput::Space)) { Gizmo.ChangeGizmoMode(); }
	if (InputManager.IsKeyReleased(EKeyInput::MouseLeft))
	{
		Gizmo.EndDrag();
		// 드래그가 끝나면 선택된 뷰포트를 비활성화 합니다.
		InteractionViewport = nullptr;
	}

	if (Gizmo.IsDragging() && Gizmo.GetSelectedComponent())
	{
		switch (Gizmo.GetGizmoMode())
		{
		case EGizmoMode::Translate:
		{
			FVector GizmoDragLocation = GetGizmoDragLocation(CurrentCamera, WorldRay);
			Gizmo.SetLocation(GizmoDragLocation);
			break;
		}
		case EGizmoMode::Rotate:
		{
			FQuaternion GizmoDragRotation = GetGizmoDragRotation(CurrentCamera, WorldRay);
			Gizmo.SetComponentRotation(GizmoDragRotation);
			break;
		}
		case EGizmoMode::Scale:
		{
			FVector GizmoDragScale = GetGizmoDragScale(CurrentCamera, WorldRay);
			Gizmo.SetComponentScale(GizmoDragScale);
		}
		}
	}
	else
	{
		if (GetSelectedActor() && Gizmo.HasComponent())
		{
			ObjectPicker.PickGizmo(CurrentCamera, WorldRay, Gizmo, CollisionPoint);
		}
		else
		{
			Gizmo.SetGizmoDirection(EGizmoDirection::None);
		}

		if (!ImGui::GetIO().WantCaptureMouse && InputManager.IsKeyPressed(EKeyInput::MouseLeft))
		{
			TArray<UPrimitiveComponent*> Candidate;

			ULevel* CurrentLevel = GWorld->GetLevel();
			ObjectPicker.FindCandidateFromOctree(CurrentLevel->GetStaticOctree(), WorldRay, Candidate);

			TArray<UPrimitiveComponent*>& DynamicCandidates = CurrentLevel->GetDynamicPrimitives();
			if (!DynamicCandidates.empty())
			{
				Candidate.insert(Candidate.end(), DynamicCandidates.begin(), DynamicCandidates.end());
			}


			TStatId StatId("Picking");
			FScopeCycleCounter PickCounter(StatId);
			UPrimitiveComponent* PrimitiveCollided = ObjectPicker.PickPrimitive(CurrentCamera, WorldRay, Candidate, &ActorDistance);
			ActorPicked = PrimitiveCollided ? PrimitiveCollided->GetOwner() : nullptr;
			float ElapsedMs = PickCounter.Finish(); // 피킹 시간 측정 종료
			UStatOverlay::GetInstance().RecordPickingStats(ElapsedMs);
		}

		if (Gizmo.GetGizmoDirection() == EGizmoDirection::None)
		{
			SelectActor(ActorPicked);
			if (PreviousGizmoDirection != EGizmoDirection::None)
			{
				Gizmo.OnMouseRelease(PreviousGizmoDirection);
			}
		}
		else
		{
			PreviousGizmoDirection = Gizmo.GetGizmoDirection();
			if (InputManager.IsKeyPressed(EKeyInput::MouseLeft))
			{
				Gizmo.OnMouseDragStart(CollisionPoint);
				// 드래그가 활성화하면 뷰포트를 고정합니다.
				InteractionViewport = CurrentViewport;
			}
			else
			{
				Gizmo.OnMouseHovering();
			}
		}
	}
}

FVector UEditor::GetGizmoDragLocation(UCamera* InActiveCamera, FRay& WorldRay)
{
	FVector MouseWorld;
	FVector PlaneOrigin{ Gizmo.GetGizmoLocation() };
	FVector GizmoAxis = Gizmo.GetGizmoAxis();

	if (!Gizmo.IsWorldMode())
	{
		FQuaternion q = Gizmo.GetTargetComponent()->GetWorldRotationAsQuaternion();
		GizmoAxis = q.RotateVector(GizmoAxis);
	}

	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, GizmoAxis.Cross(InActiveCamera->CalculatePlaneNormal(GizmoAxis)), MouseWorld))
	{
		FVector MouseDistance = MouseWorld - Gizmo.GetDragStartMouseLocation();
		return Gizmo.GetDragStartActorLocation() + GizmoAxis * MouseDistance.Dot(GizmoAxis);
	}
	return Gizmo.GetGizmoLocation();
}

FQuaternion UEditor::GetGizmoDragRotation(UCamera* InActiveCamera, FRay& WorldRay)
{
	FVector MouseWorld;
	FVector PlaneOrigin{ Gizmo.GetGizmoLocation() };
	FVector GizmoAxis = Gizmo.GetStoredDragAxis();

	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, GizmoAxis, MouseWorld))
	{
		FVector PlaneOriginToMouse = MouseWorld - PlaneOrigin;
		FVector PlaneOriginToMouseStart = Gizmo.GetDragStartMouseLocation() - PlaneOrigin;
		PlaneOriginToMouse.Normalize();
		PlaneOriginToMouseStart.Normalize();

		float DotResult = (PlaneOriginToMouseStart).Dot(PlaneOriginToMouse);
		float Angle = acosf(std::max(-1.0f, std::min(1.0f, DotResult)));

		if ((PlaneOriginToMouseStart.Cross(PlaneOriginToMouse)).Dot(GizmoAxis) < 0)
		{
			Angle = -Angle;
		}

		FQuaternion StartRotQuat = FQuaternion::FromEuler(Gizmo.GetDragStartActorRotation());
		FQuaternion DeltaRotQuat = FQuaternion::FromAxisAngle(GizmoAxis, Angle);

		// 3. 곱셈 순서 수정 (Delta * Old)
		if (Gizmo.IsWorldMode())
		{
			// .ToEuler() 제거!
			return (DeltaRotQuat * StartRotQuat);
		}
		else
		{
			// .ToEuler() 제거!
			return (DeltaRotQuat * StartRotQuat);
		}
	}
	// 반환 타입 변경: FVector -> FQuaternion
	// Gizmo.GetComponentRotation() 은 FVector(Euler)를 반환하므로 Target의 쿼터니언을 직접 가져옵니다.
	if(Gizmo.GetSelectedComponent())
	{
		return Gizmo.GetSelectedComponent()->GetWorldRotationAsQuaternion();
	}
	// 비상시 Identity 반환
	return FQuaternion::Identity();
}

FVector UEditor::GetGizmoDragScale(UCamera* InActiveCamera, FRay& WorldRay)
{
	FVector MouseWorld;
	FVector PlaneOrigin = Gizmo.GetGizmoLocation();
	FVector CardinalAxis = Gizmo.GetGizmoAxis();

	FVector GizmoAxis = Gizmo.GetGizmoAxis();
	FQuaternion q = Gizmo.GetTargetComponent()->GetWorldRotationAsQuaternion();
	GizmoAxis = q.RotateVector(GizmoAxis);

	FVector PlaneNormal = GizmoAxis.Cross(InActiveCamera->CalculatePlaneNormal(GizmoAxis));
	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, PlaneNormal, MouseWorld))
	{
		FVector PlaneOriginToMouse = MouseWorld - PlaneOrigin;
		FVector PlaneOriginToMouseStart = Gizmo.GetDragStartMouseLocation() - PlaneOrigin;
		float DragStartAxisDistance = PlaneOriginToMouseStart.Dot(GizmoAxis);
		float DragAxisDistance = PlaneOriginToMouse.Dot(GizmoAxis);
		float ScaleFactor = 1.0f;
		if (abs(DragStartAxisDistance) > 0.1f)
		{
			ScaleFactor = DragAxisDistance / DragStartAxisDistance;
		}

		FVector DragStartScale = Gizmo.GetDragStartActorScale();
		if (ScaleFactor > MinScale)
		{
			if (Gizmo.GetSelectedComponent()->IsUniformScale())
			{
				float UniformValue = DragStartScale.Dot(CardinalAxis);
				return FVector(1.0f, 1.0f, 1.0f) * UniformValue * ScaleFactor;
			}
			else
			{
				return DragStartScale + CardinalAxis * (ScaleFactor - 1.0f) * DragStartScale.Dot(CardinalAxis);
			}
		}
		return Gizmo.GetComponentScale();
	}
	return Gizmo.GetComponentScale();
}

void UEditor::SelectActor(AActor* InActor)
{
	if (InActor == SelectedActor) return;

	SelectedActor = InActor;
	if (SelectedActor) { SelectComponent(InActor->GetRootComponent()); }
	else { SelectComponent(nullptr); }
}

void UEditor::SelectComponent(UActorComponent* InComponent)
{
	if (InComponent == SelectedComponent) return;

	if (SelectedComponent)
	{
		SelectedComponent->OnDeselected();
	}

	SelectedComponent = InComponent;
	if (SelectedComponent)
	{
		SelectedComponent->OnSelected();
	}
	UUIManager::GetInstance().OnSelectedComponentChanged(SelectedComponent);
}

TArray<const FEditorPrimitive*> UEditor::GetEditorDepthPrimitives() const
{
	TArray<const FEditorPrimitive*> AllPrimitives = Axis.GetEditorPrimitive();
	AllPrimitives.insert(AllPrimitives.end(), UBatchLineManager::GetInstance().GetBatchLinePrimitive());
	return AllPrimitives;
}

TArray<const FEditorPrimitive*> UEditor::GetEditorOverlayPrimitives() const
{
	TArray<const FEditorPrimitive*> AllPrimitives = Gizmo.GetEditorPrimitive();
	return AllPrimitives;
}
