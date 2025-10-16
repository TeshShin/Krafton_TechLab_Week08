#include "pch.h"
#include "Scene/Public/Actor/HeightFogActor.h"
#include "Scene/Public/Component/BillBoardComponent.h"
#include "Scene/Public/Component/HeightFogComponent.h"
#include "Manager/Public/AssetManager.h"

IMPLEMENT_CLASS(AHeightFogActor, AActor)

AHeightFogActor::AHeightFogActor()
{
}

UClass* AHeightFogActor::GetDefaultRootComponent()
{
    return UHeightFogComponent::StaticClass();
}

void AHeightFogActor::InitializeComponents()
{
    Super::InitializeComponents();
	
    UBillBoardComponent* Billboard = CreateDefaultSubobject<UBillBoardComponent>();
    Billboard->AttachToComponent(GetRootComponent());
    Billboard->SetIsVisualizationComponent(true);
    Billboard->SetSprite(UAssetManager::GetInstance().LoadTexture("Data/Icons/ExponentialHeightFog_64x.png"));
    Billboard->SetScreenSizeScaled(true);
}
