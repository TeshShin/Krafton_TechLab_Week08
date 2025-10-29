#pragma once
#include "Core/Public/Object/Object.h"
#include <d2d1.h>
#include <dwrite.h>

enum class EStatType : uint8
{
	None =		0,		 // 0
	FPS =		1 << 0,  // 1
	Memory =	1 << 1,  // 2
	Picking =	1 << 2,  // 4
	Decal =		1 << 3,  // 8
	Time =		1 << 4,	 // 16
	Shadow =		1 << 5,	 // 32
	All = FPS | Memory | Picking | Decal | Time | Shadow
};

UCLASS()
class UStatOverlay : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(UStatOverlay, UObject)

public:
	void Initialize();
	void Release();
	void Render();

	// Stat control methods
	void ToggleStat(EStatType Stat) { IsStatEnabled(Stat) ? DisableStat(Stat) : EnableStat(Stat); }

	// API to update stats
	void RecordPickingStats(float ElapsedMS);
	void RecordDecalStats(uint32 InRenderedDecal, uint32 InCollidedCompCount);

private:
	void RenderFPS(ID2D1DeviceContext* D2DCtx);
	void RenderMemory(ID2D1DeviceContext* D2DCtx);
	void RenderPicking(ID2D1DeviceContext* D2DCtx);
	void RenderDecalInfo(ID2D1DeviceContext* D2DCtx);
	void RenderShadow(ID2D1DeviceContext* D2DCtx);
	void RenderTimeInfo(ID2D1DeviceContext* D2DCtx);
	void RenderText(ID2D1DeviceContext* D2DCtx, const FString& Text, float X, float Y, float R, float G, float B);
	void RenderTextLine(ID2D1DeviceContext* D2DCtx, const FString& Text, float r, float g, float b);

	static constexpr float LineHeight = 20.0f;
	float CurrentLineY = 0.0f;

	// FPS Stats
	float CurrentFPS = 0.0f;
	float FrameTime = 0.0f;

	// Picking Stats
	uint32 PickAttempts = 0;
	float LastPickingTimeMs = 0.0f;
	float AccumulatedPickingTimeMs = 0.0f;

	// Decal Stats
	uint32 RenderedDecal = 0;
	uint32 CollidedCompCount = 0;

	// Rendering position
	float OverlayX = 40.0f;
	float OverlayY = 55.0f;

	uint8 StatMask = static_cast<uint8>(EStatType::None);

	// Helper methods
	std::wstring ToWString(const FString& InStr);
	void EnableStat(EStatType InStatType);
	void DisableStat(EStatType InStatType);
	void SetStatType(EStatType InStatType);
	bool IsStatEnabled(EStatType InStatType) const;

	IDWriteTextFormat* TextFormat = nullptr;

	IDWriteFactory* DWriteFactory = nullptr;
};
