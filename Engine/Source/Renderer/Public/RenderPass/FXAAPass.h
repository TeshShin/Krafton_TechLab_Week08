#pragma once
#include "RenderPass.h"

class UDeviceResources;

struct alignas(16) FFXAAConstants
{
    FVector2 InvResolution = FVector2();
    float FXAASpanMax = 16.0f;
    float FXAAReduceMul = 1.0f / 16.0f;
    float FXAAReduceMin = 1.0f / 256.0f;
    float Padding = 0.0f;
};

class FFXAAPass : public FRenderPass
{
public:
	/**
	 * @brief FXAAPass 클래스의 생성자입니다.
	 * @param InPipeline 파이프라인 객체입니다.
	 * @param InDeviceResources 디바이스 리소스 객체입니다.
	 */
	FFXAAPass(UPipeline* InPipeline, UDeviceResources* InDeviceResources);
	/**
	 * @brief FXAAPass 클래스의 소멸자입니다.
	 */
	~FFXAAPass();

	bool CanRender(const FRenderingContext& Context) override;
	void SetRenderTargets(class UDeviceResources* DeviceResources) override;
	/**
	 * @brief FXAA 렌더링 패스를 실행합니다.
	 * @param Context 렌더링 컨텍스트입니다.
	 */
	void Execute(FRenderingContext& Context) override;

	/**
	 * @brief FXAAPass에서 사용된 리소스를 해제합니다.
	 */
	void Release() override;


private:

private:
    UDeviceResources* DeviceResources = nullptr;

    ID3D11VertexShader* VertexShader = nullptr;
    ID3D11PixelShader* PixelShader = nullptr;
    ID3D11SamplerState* SamplerState = nullptr;

    ID3D11Buffer* FXAAConstantBuffer = nullptr;
    FFXAAConstants FXAAParams{};

	ID3D11ShaderResourceView* SceneSRV = nullptr;
};
