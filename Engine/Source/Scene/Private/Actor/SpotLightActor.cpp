#include "pch.h"
#include "Scene/Public/Actor/SpotLightActor.h"
#include "Manager/Public/AssetManager.h"
#include "Scene/Public/Component/BillBoardComponent.h"
#include "Scene/Public/Component/SpotLightComponent.h"

IMPLEMENT_CLASS(ASpotLightActor, AActor)

ASpotLightActor::ASpotLightActor()
{
}

UClass* ASpotLightActor::GetDefaultRootComponent()
{
    return USpotLightComponent::StaticClass();
}

void ASpotLightActor::InitializeComponents()
{
    Super::InitializeComponents();

    UBillBoardComponent* Billboard = CreateDefaultSubobject<UBillBoardComponent>();
    Billboard->AttachToComponent(GetRootComponent());
    Billboard->SetIsVisualizationComponent(true);
    Billboard->SetSprite(UAssetManager::GetInstance().LoadTexture("Data/Icons/SpotLight_64x.PNG"));
    Billboard->SetScreenSizeScaled(true);
}
