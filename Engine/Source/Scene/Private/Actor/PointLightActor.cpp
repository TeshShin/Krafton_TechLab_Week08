#include "pch.h"
#include "Scene/Public/Actor/PointLightActor.h"
#include "Manager/Public/AssetManager.h"
#include "Scene/Public/Component/BillBoardComponent.h"
#include "Scene/Public/Component/PointLightComponent.h"

IMPLEMENT_CLASS(APointLightActor, AActor)

APointLightActor::APointLightActor()
{
}

UClass* APointLightActor::GetDefaultRootComponent()
{
    return UPointLightComponent::StaticClass();
}

void APointLightActor::InitializeComponents()
{
    Super::InitializeComponents();
	
    UBillBoardComponent* Billboard = CreateDefaultSubobject<UBillBoardComponent>();
    Billboard->AttachToComponent(GetRootComponent());
    Billboard->SetIsVisualizationComponent(true);
    Billboard->SetSprite(UAssetManager::GetInstance().LoadTexture("Data/Icons/PointLight_64x.png"));
    Billboard->SetScreenSizeScaled(true);
}
