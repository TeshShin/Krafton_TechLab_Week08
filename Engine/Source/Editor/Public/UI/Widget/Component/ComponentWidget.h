#pragma once
#include "Editor/Public/UI/Widget/Widget.h"

// 전방 선언
class UActorComponent;

/**
 * @brief 기본 컴포넌트 위젯
 *
 * 선택된 컴포넌트의 UPROPERTY를 자동으로 렌더링합니다.
 *
 * 사용 방법:
 * 1. 기본 사용: UComponentWidget을 그대로 사용하면 자동으로 모든 UPROPERTY가 표시됩니다.
 * 2. 커스터마이징: 이 클래스를 상속받아 RenderWidget()을 오버라이드하고,
 *    Super::RenderWidget()을 호출한 후 추가 UI를 구현할 수 있습니다.
 *
 * 예시:
 * ```cpp
 * void MyCustomWidget::RenderWidget()
 * {
 *     // 먼저 UPROPERTY를 자동으로 렌더링
 *     Super::RenderWidget();
 *
 *     // 그 다음 커스텀 UI 추가
 *     ImGui::Separator();
 *     ImGui::Text("Custom UI here");
 * }
 * ```
 */
UCLASS()
class UComponentWidget : public UWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(UComponentWidget, UWidget)

public:
	UComponentWidget() = default;
	virtual ~UComponentWidget() = default;

	virtual void Initialize() override {}
	virtual void Update() override {}

	/**
	 * @brief 위젯 렌더링
	 * 선택된 컴포넌트의 모든 UPROPERTY를 자동으로 렌더링합니다.
	 */
	virtual void RenderWidget() override;

protected:
	/**
	 * @brief UPROPERTY 시스템을 사용하여 자동으로 프로퍼티 UI를 렌더링합니다.
	 *
	 * VisibleAnywhere 또는 EditAnywhere 플래그가 있는 프로퍼티만 표시됩니다.
	 * EditAnywhere 플래그가 있으면 편집 가능한 UI로, 그 외에는 읽기 전용으로 표시됩니다.
	 *
	 * @param TargetObject 프로퍼티를 표시할 대상 객체
	 * @param bShowHeader 헤더("Properties")를 표시할지 여부 (기본: true)
	 */
	void RenderProperties(UObject* TargetObject, bool bShowHeader = true);
};
