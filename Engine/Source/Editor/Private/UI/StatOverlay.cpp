#include "pch.h"
#include "Editor/Public/UI/StatOverlay.h"
#include "Manager/Public/TimeManager.h"
#include "Renderer/Public/Renderer.h"
#include "Renderer/Public/ShadowMapManager.h"

IMPLEMENT_SINGLETON_CLASS(UStatOverlay, UObject)

UStatOverlay::UStatOverlay() {}
UStatOverlay::~UStatOverlay() = default;

void UStatOverlay::Initialize()
{
    auto* DeviceResources = URenderer::GetInstance().GetDeviceResources();
    DWriteFactory = DeviceResources->GetDWriteFactory();

    if (DWriteFactory)
    {
        DWriteFactory->CreateTextFormat(
            L"Consolas",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            15.0f,
            L"en-us",
            &TextFormat
        );
    }

	// 시연용 Shadow Enable
	EnableStat(EStatType::Shadow);
}

void UStatOverlay::Release()
{
    SafeRelease(TextFormat);

    DWriteFactory = nullptr;
}

void UStatOverlay::Render()
{
    auto* DeviceResources = URenderer::GetInstance().GetDeviceResources();
    IDXGISwapChain* SwapChain = DeviceResources->GetSwapChain();
    ID3D11Device* D3DDevice = DeviceResources->GetDevice();

    ID2D1Factory1* D2DFactory = nullptr;
    D2D1_FACTORY_OPTIONS opts{};
#ifdef _DEBUG
    opts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &opts, (void**)&D2DFactory)))
        return;

    IDXGISurface* Surface = nullptr;
    SwapChain->GetBuffer(0, __uuidof(IDXGISurface), (void**)&Surface);

    IDXGIDevice* DXGIDevice = nullptr;
    D3DDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&DXGIDevice);

    ID2D1Device* D2DDevice = nullptr;
    D2DFactory->CreateDevice(DXGIDevice, &D2DDevice);
    if (D2DDevice == nullptr)
    {
        return;
    }

    ID2D1DeviceContext* D2DCtx = nullptr;
    D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &D2DCtx);

    D2D1_BITMAP_PROPERTIES1 BmpProps = {};
    BmpProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    BmpProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    BmpProps.dpiX = 96.0f;
    BmpProps.dpiY = 96.0f;
    BmpProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    ID2D1Bitmap1* TargetBmp = nullptr;
    D2DCtx->CreateBitmapFromDxgiSurface(Surface, &BmpProps, &TargetBmp);

    D2DCtx->SetTarget(TargetBmp);
    D2DCtx->BeginDraw();

    // CurrentLineY를 사용하여 각 렌더 함수가 자동으로 다음 줄로 이동
    CurrentLineY = OverlayY;

    if (IsStatEnabled(EStatType::FPS))     RenderFPS(D2DCtx);
    if (IsStatEnabled(EStatType::Memory))  RenderMemory(D2DCtx);
    if (IsStatEnabled(EStatType::Picking)) RenderPicking(D2DCtx);
    if (IsStatEnabled(EStatType::Decal))   RenderDecalInfo(D2DCtx);
	if (IsStatEnabled(EStatType::Shadow))    RenderShadow(D2DCtx);
    if (IsStatEnabled(EStatType::Time))    RenderTimeInfo(D2DCtx);

    D2DCtx->EndDraw();
    D2DCtx->SetTarget(nullptr);

    SafeRelease(TargetBmp);
    SafeRelease(D2DCtx);
    SafeRelease(D2DDevice);
    SafeRelease(DXGIDevice);
    SafeRelease(Surface);
    SafeRelease(D2DFactory);
}

void UStatOverlay::RenderFPS(ID2D1DeviceContext* D2DCtx)
{
    auto& timeManager = UTimeManager::GetInstance();
    CurrentFPS = timeManager.GetFPS();
    FrameTime = timeManager.GetDeltaTime() * 1000;

    char buf[64];
    sprintf_s(buf, sizeof(buf), "FPS: %.1f (%.2f ms)", CurrentFPS, FrameTime);
    FString text = buf;

    float r = 0.5f, g = 1.0f, b = 0.5f;
    if (CurrentFPS < 30.0f) { r = 1.0f; g = 0.0f; b = 0.0f; }
    else if (CurrentFPS < 60.0f) { r = 1.0f; g = 1.0f; b = 0.0f; }

    RenderTextLine(D2DCtx, text, r, g, b);
}

void UStatOverlay::RenderMemory(ID2D1DeviceContext* d2dCtx)
{
    float MemoryMB = static_cast<float>(TotalAllocationBytes) / (1024.0f * 1024.0f);

    char Buf[64];
    sprintf_s(Buf, sizeof(Buf), "Memory: %.1f MB (%u objects)", MemoryMB, TotalAllocationCount);
    FString text = Buf;

    RenderTextLine(d2dCtx, text, 1.0f, 1.0f, 0.0f);
}

void UStatOverlay::RenderPicking(ID2D1DeviceContext* D2DCtx)
{
    float AvgMs = PickAttempts > 0 ? AccumulatedPickingTimeMs / PickAttempts : 0.0f;

    char Buf[128];
    sprintf_s(Buf, sizeof(Buf), "Picking Time %.2f ms (Attempts %u, Accum %.2f ms, Avg %.2f ms)",
        LastPickingTimeMs, PickAttempts, AccumulatedPickingTimeMs, AvgMs);
    FString Text = Buf;

    float r = 0.0f, g = 1.0f, b = 0.8f;
    if (LastPickingTimeMs > 5.0f) { r = 1.0f; g = 0.0f; b = 0.0f; }
    else if (LastPickingTimeMs > 1.0f) { r = 1.0f; g = 1.0f; b = 0.0f; }

    RenderTextLine(D2DCtx, Text, r, g, b);
}

void UStatOverlay::RenderDecalInfo(ID2D1DeviceContext* D2DCtx)
{
    {
        char Buf[128];
        sprintf_s(Buf, sizeof(Buf), "Rendered Decal: %d (Collided Components: %d)",
            RenderedDecal, CollidedCompCount);
        FString Text = Buf;
        RenderTextLine(D2DCtx, Text, 0.f, 1.f, 0.f);
    }

    {
        char Buf[128];
        sprintf_s(Buf, sizeof(Buf), "Decal Pass Time: %.4f ms", FScopeCycleCounter::GetTimeProfile("DecalPass").Milliseconds);
        FString Text = Buf;
        RenderTextLine(D2DCtx, Text, 0.f, 1.f, 0.f);
    }
}

void UStatOverlay::RenderShadow(ID2D1DeviceContext* D2DCtx)
{
	FShadowMapManager& ShadowMapManager = FShadowMapManager::GetInstance();
    const FShadowStatData& Stats = ShadowMapManager.GetShadowStats();

    auto ToMB = [](uint64_t bytes) -> double {
        return static_cast<double>(bytes) / (1024.0 * 1024.0);
    };

    char Buf[256];
    FString Text;
    const float R = 0.0f, G = 1.0f, B = 0.0f;

    // --- 총 VRAM 사용량 ---
    {
        sprintf_s(Buf, sizeof(Buf), "Total Shadow VRAM: %.2f MB", ToMB(Stats.VRAM_Total));
        Text = Buf;
        RenderTextLine(D2DCtx, Text, R, G, B);
    }

    // --- 할당된 라이트 개수 ---
    {
        uint32 TotalAllocated = Stats.Allocated_Directional + Stats.Allocated_Spot + Stats.Allocated_PointCubes;
        sprintf_s(Buf, sizeof(Buf), "Shadow Casting Lights: %d", TotalAllocated);
        Text = Buf;
        RenderTextLine(D2DCtx, Text, R, G, B);
    }

    // --- Directional Light 상세 ---
    {
        sprintf_s(Buf, sizeof(Buf), "  Directional: %d (%dx%d)",
            Stats.Allocated_Directional,
            Stats.Config_DirResolution, Stats.Config_DirResolution);
        Text = Buf;
        RenderTextLine(D2DCtx, Text, R, G, B);
    }
    {
        sprintf_s(Buf, sizeof(Buf), "    Depth: %.2f MB, Moments: %.2f MB",
            ToMB(Stats.VRAM_Directional_Depth),
            ToMB(Stats.VRAM_Directional_Moments));
        Text = Buf;
        RenderTextLine(D2DCtx, Text, R, G, B);
    }

    // --- Spot Lights 상세 ---
    {
        sprintf_s(Buf, sizeof(Buf), "  Spot Lights: %d / %d (%dx%d)",
            Stats.Allocated_Spot,
            Stats.Config_MaxSpotShadows,
            Stats.Config_SpotResolution, Stats.Config_SpotResolution);
        Text = Buf;
        RenderTextLine(D2DCtx, Text, R, G, B);
    }
    {
        sprintf_s(Buf, sizeof(Buf), "    Depth: %.2f MB, Moments: %.2f MB",
            ToMB(Stats.VRAM_Spot_Depth),
            ToMB(Stats.VRAM_Spot_Moments));
        Text = Buf;
        RenderTextLine(D2DCtx, Text, R, G, B);
    }

    // --- Point Lights 상세 ---
    {
        sprintf_s(Buf, sizeof(Buf), "  Point Lights: %d / %d (%dx%d)",
            Stats.Allocated_PointCubes,
            Stats.Config_MaxPointShadowCubes,
            Stats.Config_PointResolution, Stats.Config_PointResolution);
        Text = Buf;
        RenderTextLine(D2DCtx, Text, R, G, B);
    }
    {
        sprintf_s(Buf, sizeof(Buf), "    Moments (CubeArray): %.2f MB",
            ToMB(Stats.VRAM_Point_Moments));
        Text = Buf;
        RenderTextLine(D2DCtx, Text, R, G, B);
    }
    {
        // (Initialize에서 생성한 포인트라이트 전용 임시 DSV)
        sprintf_s(Buf, sizeof(Buf), "    Pass DSV (Temp): %.2f MB",
            ToMB(Stats.VRAM_Point_Pass_DSV));
        Text = Buf;
        RenderTextLine(D2DCtx, Text, R, G, B);
    }
}

void UStatOverlay::RenderTimeInfo(ID2D1DeviceContext* D2DCtx)
{
    const TArray<FString> ProfileKeys = FScopeCycleCounter::GetTimeProfileKeys();

    for (const FString& Key : ProfileKeys)
    {
        const FTimeProfile& Profile = FScopeCycleCounter::GetTimeProfile(Key);

        char buf[128];
        sprintf_s(buf, sizeof(buf), "%s: %.2f ms", Key.c_str(), Profile.Milliseconds);
        FString text = buf;

        float r = 0.8f, g = 0.8f, b = 0.8f;
        if (Profile.Milliseconds > 1.0f) { r = 1.0f; g = 1.0f; b = 0.0f; }

        RenderTextLine(D2DCtx, text, r, g, b);
    }
}

void UStatOverlay::RenderTextLine(ID2D1DeviceContext* D2DCtx, const FString& Text, float r, float g, float b)
{
    RenderText(D2DCtx, Text, OverlayX, CurrentLineY, r, g, b);
    CurrentLineY += LineHeight;
}

void UStatOverlay::RenderText(ID2D1DeviceContext* D2DCtx, const FString& Text, float x, float y, float r, float g, float b)
{
    if (!D2DCtx || Text.empty() || !TextFormat) return;

    std::wstring wText = ToWString(Text);

    ID2D1SolidColorBrush* Brush = nullptr;
    if (FAILED(D2DCtx->CreateSolidColorBrush(D2D1::ColorF(r, g, b), &Brush)))
        return;

    D2D1_RECT_F rect = D2D1::RectF(x, y, x + 800.0f, y + LineHeight);
    D2DCtx->DrawTextW(
        wText.c_str(),
        static_cast<UINT32>(wText.length()),
        TextFormat,
        &rect,
        Brush
    );

    SafeRelease(Brush);
}

std::wstring UStatOverlay::ToWString(const FString& InStr)
{
    if (InStr.empty()) return std::wstring();

    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, InStr.c_str(), (int)InStr.size(), NULL, 0);
    std::wstring wStr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, InStr.c_str(), (int)InStr.size(), &wStr[0], sizeNeeded);
    return wStr;
}

void UStatOverlay::EnableStat(EStatType type) { StatMask |= static_cast<uint8>(type); }
void UStatOverlay::DisableStat(EStatType type) { StatMask &= ~static_cast<uint8>(type); }
void UStatOverlay::SetStatType(EStatType type) { StatMask = static_cast<uint8>(type); }
bool UStatOverlay::IsStatEnabled(EStatType type) const { return (StatMask & static_cast<uint8>(type)) != 0; }

void UStatOverlay::RecordPickingStats(float elapsedMs)
{
    ++PickAttempts;
    LastPickingTimeMs = elapsedMs;
    AccumulatedPickingTimeMs += elapsedMs;
}

void UStatOverlay::RecordDecalStats(uint32 InRenderedDecal, uint32 InCollidedCompCount)
{
    RenderedDecal = InRenderedDecal;
    CollidedCompCount = InCollidedCompCount;
}
