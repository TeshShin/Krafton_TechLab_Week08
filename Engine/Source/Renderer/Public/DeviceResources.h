#pragma once

class UDeviceResources
{
public:
	UDeviceResources(HWND InWindowHandle);
	~UDeviceResources();

	void Create(HWND InWindowHandle);
	void Release();

	void CreateDeviceAndSwapChain(HWND InWindowHandle);
	void ReleaseDeviceAndSwapChain();

	// Direct2D
	void CreateFactories();
	void ReleaseFactories();

	void CreateBuffers();
	void ReleaseBuffers();

	void SwapFrameBuffers();

	ID3D11Device* GetDevice() const { return Device; }
	ID3D11DeviceContext* GetDeviceContext() const { return DeviceContext; }
	IDXGISwapChain* GetSwapChain() const { return SwapChain; }

	ID3D11RenderTargetView* GetBackBufferRTV() const { return BackBufferRTV; }
	ID3D11ShaderResourceView* GetBackBufferSRV() const { return BackBufferSRV; }

	ID3D11ShaderResourceView* GetSourceSRV() const { return FrameBufferSRV[SourceIdx]; }
	ID3D11RenderTargetView* GetSourceRTV() const { return FrameBufferRTV[SourceIdx]; }
	ID3D11RenderTargetView* GetDestinationRTV() const { return FrameBufferRTV[DestinationIdx]; }

	ID3D11RenderTargetView* GetNormalBufferRTV() const { return NormalBufferRTV; }
	ID3D11ShaderResourceView* GetNormalBufferSRV() const { return NormalBufferSRV; }

	ID3D11DepthStencilView* GetDepthBufferDSV() const { return DepthBufferDSV; }
	ID3D11ShaderResourceView* GetDepthBufferSRV() const { return DepthBufferSRV; }

	const D3D11_VIEWPORT& GetViewportInfo() const { return ViewportInfo; }
	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	void UpdateViewport();

	// Direct2D/DirectWrite factory getters
	IDWriteFactory* GetDWriteFactory() const { return DWriteFactory; }

private:
	void CreateBackBuffer();
	void ReleaseBackBuffer();

	void CreateFrameBuffers();
	void ReleaseFrameBuffers();

	void CreateNormalBuffer();
	void ReleaseNormalBuffer();

	void CreateDepthBuffer();
	void ReleaseDepthBuffer();

	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	// 최종 프레임의 Back Buffer로 쓰이는 Buffer에 관한 변수들
	ID3D11Texture2D* BackBuffer = nullptr;
	ID3D11RenderTargetView* BackBufferRTV = nullptr;
	ID3D11ShaderResourceView* BackBufferSRV = nullptr;

	// 핑퐁용 RTV/SRV로 쓰이는 Buffer들에 관한 변수들
	ID3D11Texture2D* FrameBuffer[2];
	ID3D11RenderTargetView* FrameBufferRTV[2];
	ID3D11ShaderResourceView* FrameBufferSRV[2];

	// Normal이 쓰여지는 Buffer에 관한 변수들
	ID3D11Texture2D* NormalBuffer = nullptr;
	ID3D11RenderTargetView* NormalBufferRTV = nullptr;
	ID3D11ShaderResourceView* NormalBufferSRV = nullptr;

	// Depth가 쓰여지는 Buffer에 관한 변수들
	ID3D11Texture2D* DepthBuffer = nullptr;
	ID3D11DepthStencilView* DepthBufferDSV = nullptr;
	ID3D11ShaderResourceView* DepthBufferSRV = nullptr;

	D3D11_VIEWPORT ViewportInfo = {};

	uint32 Width = 0;
	uint32 Height = 0;

	// Direct2D/DirectWrite factories
	ID2D1Factory* D2DFactory = nullptr;
	IDWriteFactory* DWriteFactory = nullptr;

	uint32 SourceIdx = 0;
	uint32 DestinationIdx = 1;
};
