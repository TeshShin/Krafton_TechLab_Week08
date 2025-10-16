#include "pch.h"
#include "Scene/Public/Actor/DecalActor.h"
#include "Scene/Public/Component/BillBoardComponent.h"
#include "Scene/Public/Component/DecalComponent.h"
#include "Manager/Public/AssetManager.h"

IMPLEMENT_CLASS(ADecalActor, AActor)

ADecalActor::ADecalActor()
{
}

UClass* ADecalActor::GetDefaultRootComponent()
{
    return UDecalComponent::StaticClass();
}

void ADecalActor::InitializeComponents()
{
    Super::InitializeComponents();
    
    UBillBoardComponent* Billboard = CreateDefaultSubobject<UBillBoardComponent>();
    Billboard->AttachToComponent(GetRootComponent());
    Billboard->SetIsVisualizationComponent(true);
    Billboard->SetSprite(UAssetManager::GetInstance().LoadTexture("Data/Icons/DecalActor_64x.png"));
    Billboard->SetScreenSizeScaled(true);
}
