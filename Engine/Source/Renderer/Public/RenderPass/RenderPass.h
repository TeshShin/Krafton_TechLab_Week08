#pragma once
#include "RenderingContext.h"

class UPipeline;

/**
 * @brief 특정 Primitive Type별로 달라지는 RenderPass를 관리하고 실행하도록 하는 기본 인터페이스
 */
class FRenderPass
{
public:
    FRenderPass(UPipeline* InPipeline)
        : Pipeline(InPipeline) {}

    virtual ~FRenderPass() = default;

    /**
     * @brief 현재 렌더 패스를 렌더할 수 있는지 확인 (ViewMode, ShowFlag 등에 영향받음)
     */
    virtual bool CanRender(const FRenderingContext& Context) = 0;

    /**
     * @brief 프레임마다 실행할 렌더 타겟 설정 함수
     * Execute 전에 호출되어 해당 Pass의 렌더 타겟을 설정함
     * @param DeviceResources RTV/DSV/Buffer 등을 담고 있는 DeviceResources 객체
     */
    virtual void SetRenderTargets(class UDeviceResources* DeviceResources) = 0;

    /**
     * @brief 프레임마다 실행할 렌더 함수
     * @param Context 프레임 렌더링에 필요한 모든 정보를 담고 있는 객체
     */
    virtual void Execute(FRenderingContext& Context) = 0;

    /**
     * @brief 생성한 객체들 해제
     */
    virtual void Release() = 0;

protected:
    UPipeline* Pipeline;
};
