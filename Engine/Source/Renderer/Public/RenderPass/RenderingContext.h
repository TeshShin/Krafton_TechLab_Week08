#pragma once
#include "Core/Public/CoreTypes.h"

struct FRenderingContext
{
    FRenderingContext(){}

    FRenderingContext(class UCamera* InCurrentCamera, EViewModeIndex InViewMode, uint64 InShowFlags, const D3D11_VIEWPORT& InViewport, const FVector2& InRTSize)
        : CurrentCamera(InCurrentCamera), ViewMode(InViewMode), ShowFlags(InShowFlags), Viewport(InViewport), RTSize(InRTSize) {}

    UCamera* CurrentCamera = nullptr;
    EViewModeIndex ViewMode;
    uint64 ShowFlags;
    D3D11_VIEWPORT Viewport;
    FVector2 RTSize;

    TArray<class UPrimitiveComponent*> AllPrimitives;

    // Components By Render Pass
    TArray<class UStaticMeshComponent*> StaticMeshes;
    TArray<class UBillBoardComponent*> BillBoards;
    TArray<class UTextComponent*> Texts;
    TArray<class UUUIDTextComponent*> UUIDs;
    TArray<class UDecalComponent*> Decals;
    TArray<class ULightComponentBase*> Lights;
    TArray<class UHeightFogComponent*> Fogs;
};
