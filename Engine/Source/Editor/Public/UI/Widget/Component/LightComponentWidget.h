#pragma once
#include "Editor/Public/UI/Widget/Component/ComponentWidget.h"

/**
 * @brief 라이트 컴포넌트 위젯
 * 기본 UPROPERTY 렌더링 후 추가적인 라이트 관련 UI를 제공합니다.
 */
class ULightComponentWidget: public UComponentWidget
{
    DECLARE_CLASS(ULightComponentWidget, UComponentWidget);

public:
    ULightComponentWidget() = default;

    virtual ~ULightComponentWidget() = default;

    /*-----------------------------------------------------------------------------
        UWidget Features
     -----------------------------------------------------------------------------*/
public:
    virtual void Initialize() override;
    virtual void Update() override;
    virtual void RenderWidget() override;

private:
	class UCamera* PilotingCamera = nullptr;
};
