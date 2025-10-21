#pragma once
#include "Core/Public/Object/Object.h"

class UPropertyBase;

/**
 * @brief UI의 기능 단위인 위젯 클래스의 Interface Class
 * 위젯이라면 필수적인 공통 인터페이스와 기능을 제공
 */
UCLASS()
class UWidget : public UObject
{
	GENERATED_BODY()
	DECLARE_CLASS(UWidget, UObject)

public:
	// Essential Role
	// 필요하지 않은 기능이 있을 수 있으나 구현 시 반드시 고려하라는 의미의 순수 가상 함수 처리
	virtual void Initialize() = 0;
	virtual void Update() = 0;
	virtual void RenderWidget() = 0;

	// 후처리는 취사 선택
	virtual void PostProcess() {}

	// Singleton 판별 함수 (default는 false)
	virtual bool IsSingleton() const { return false; }

	// Special Member Function
	UWidget() = default;
	~UWidget() override = default;

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
	virtual void RenderProperties(UObject* TargetObject, bool bShowHeader = true);
};
